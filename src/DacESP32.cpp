/*
  DacESP32, an Arduino library for using the ESP32 DAC output channels 
  
  Copyright (c) 2022 Thomas Jentzsch

  The ESP32 has two independent 8-bit DAC (Digital to Analog Converter)
  output channels, connected to GPIO25 (Channel 1) and GPIO26 (Channel 2).
  These DAC channels can be set to arbitrary output voltages
  between 0...+3.3V (VDD) or driven by a common cosine waveform (CW)
  generator. Please see Readme.md for more details.

  This library is an extension to the DAC routines provided 
  by the espressif32 framework. For more information have a look at 
  "https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
  api-reference/peripherals/dac.html#" or at Chapter 29.5 DAC in 
  document "ESP32 Technical Reference Manual" under
  "https://www.espressif.com/en/support/documents/technical-documents".

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation 
  files (the "Software"), to deal in the Software without restriction, 
  including without limitation the rights to use, copy, modify, merge, 
  publish, distribute, sublicense, and/or sell copies of the Software, 
  and to permit persons to whom the Software is furnished to do so, subject 
  to the following conditions:

  The above copyright notice and this permission notice shall be 
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "DacESP32.h"

//
// Below definitions can be altered with build options. There is no need  
// to edit this file. Please have a look at Readme.md for details !
//
// For fine tuning the CW generator output frequency. Values higher than 172 
// will increase the output frequency. Values lower than 172 will decrease it. 
#if CK8M_DFREQ_ADJUSTED == 172
#undef CK8M_DFREQ_ADJUSTED
#endif

// Enables a more accurate setting of the CW generator output frequency. 
// Can be disabled if it causes problems with ADC section in the ESP32. 
#if !defined CW_FREQUENCY_HIGH_ACCURACY
#define CW_FREQUENCY_HIGH_ACCURACY
#elif CW_FREQUENCY_HIGH_ACCURACY == 0
#undef CW_FREQUENCY_HIGH_ACCURACY
#endif

// Below value defines the CW generators minimum number of voltage steps per cycle as well
// as the maximal possible CW output frequency. With default value 256 it is ~31.3kHz.
// Lowering the value will increase the number of voltage steps/cycle and vice versa. 
// IMPORTANT: This definition is only valid without build option 'CW_FREQUENCY_HIGH_ACCURACY=0'.
#if defined CW_FREQUENCY_HIGH_ACCURACY
#if !defined SW_FSTEP_MAX
#define SW_FSTEP_MAX  256
#elif (SW_FSTEP_MAX != 64 && SW_FSTEP_MAX != 128 && SW_FSTEP_MAX != 256 && SW_FSTEP_MAX != 512 && SW_FSTEP_MAX != 1024)
#error "Build option SW_FSTEP_MAX is defined incorrectly ! Allowed values: 64, 128, 256, 512 or 1024."
#endif
#else // !defined CW_FREQUENCY_HIGH_ACCURACY
#define SW_FSTEP_MAX 1640 // limits setCwFrequency() to ~200kHz
#endif

// The maximum possible DAC output voltage depends on the actual supply voltage (VDD) of
// your ESP32. It will vary with the used LDO voltage regulator on your board, the load on
// the channels and other factors. Add build option with measured value if needed.
#ifndef CHANNEL_VOLTAGE_MAX
#define CHANNEL_VOLTAGE_MAX 3.30
#endif

#define CHANNEL_CHECK(channel)          \
  if (channel == DAC_CHAN_UNDEFINED) {  \
    log_e("channel setting invalid");   \
    return ESP_FAIL;                    \
  } 

#define FREQUENCY_CHECK(frequency)                                       \
  if (frequency < 16) {                                                  \
    log_e("invalid parameter: frequency (%d) out of range", frequency);  \
    return ESP_ERR_NOT_SUPPORTED;                                        \
  }

// initialize static members of class (shared by all objects of this class)
size_t   DacESP32::m_objectCount = 0;
uint32_t DacESP32::m_cwFrequency = 0;
bool     DacESP32::m_ch0_locked = 0;
bool     DacESP32::m_ch1_locked = 0;

//
// Class constructor.
// Parameter: channel...assigned DAC channel (DAC_CHAN_0 or DAC_CHAN_1)
//
DacESP32::DacESP32(dac_channel_t channel) 
{
  m_channel = DAC_CHAN_UNDEFINED;

  m_oneshot_value = -1;
  m_oneshot_cfg.chan_id = m_channel;
  m_oneshot_handle = DAC_ONE_HANDLE_UNDEFINED;

  m_cosine_cfg.chan_id = m_channel;
  m_cosine_handle = DAC_COS_HANDLE_UNDEFINED;

  if (++m_objectCount > DAC_CHAN_MAX) {
    log_e("DacESP32 objects created = %d > %d (max DAC channels) !", m_objectCount, DAC_CHAN_MAX);
    return;
  }

  if (channel == DAC_CHAN_0) {
    if (m_ch0_locked) { log_e("DAC channel 0 already in use"); return; }
    m_ch0_locked = true;
    m_channel = DAC_CHAN_0;
    //m_gpio_pin = (gpio_num_t)DAC_CHAN0_GPIO_NUM;
    m_oneshot_cfg.chan_id = m_channel;
    m_cosine_cfg.chan_id = m_channel;
  }
  else if (channel == DAC_CHAN_1) {
    if (m_ch1_locked) { log_e("DAC channel 1 already in use"); return; }
    m_ch1_locked = true;
    m_channel = DAC_CHAN_1;
    //m_gpio_pin = (gpio_num_t)DAC_CHAN1_GPIO_NUM;
    m_oneshot_cfg.chan_id = m_channel;
    m_cosine_cfg.chan_id = m_channel;
  }
  else {
    log_e("channel setting invalid");
    return;
  }

  // default CW generator settings for this channel
  m_cosine_cfg.clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
  m_cosine_cfg.atten = DAC_COSINE_ATTEN_DB_0,
  m_cosine_cfg.phase = DAC_COSINE_PHASE_0,
  m_cosine_cfg.offset = DAC_CW_OFFSET_DEFAULT,
  m_cosine_cfg.flags.force_set_freq = true;

  // frequency setting shared by all objects
  m_cosine_cfg.freq_hz = m_cwFrequency;
  if (m_cwFrequency == 0) {
    // CW generator not yet in use
#ifdef CK8M_DFREQ_ADJUSTED
    REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DFREQ, CK8M_DFREQ_ADJUSTED);
#endif
  }
}

//
// Class constructor.
// Parameter: pin...assigned GPIO pin
//
DacESP32::DacESP32(gpio_num_t pin) 
  : DacESP32((dac_channel_t)(pin == DAC_CHAN0_GPIO_NUM) ? DAC_CHAN_0 : 
                            (pin == DAC_CHAN1_GPIO_NUM) ? DAC_CHAN_1 : DAC_CHAN_UNDEFINED)
{
}

//
// Class destructor.
//
DacESP32::~DacESP32()
{
  if (m_oneshot_handle != DAC_ONE_HANDLE_UNDEFINED) {
    dac_oneshot_del_channel(m_oneshot_handle);
    log_d("dac oneshot delete: m_oneshot_handle=0x%x, channel=%d", (int)m_oneshot_handle, (int)m_oneshot_cfg.chan_id);
  }
  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    if (((dac_cosine_s_ *)m_cosine_handle)->is_started)
      dac_cosine_stop(m_cosine_handle);
    dac_cosine_del_channel(m_cosine_handle);
    log_d("dac cosine delete: m_cosine_handle=0x%x, channel=%d", (int)m_cosine_handle, (int)m_cosine_cfg.chan_id);
  }

  if (m_channel == DAC_CHAN_0) { 
    m_ch0_locked = false;
  }
  else if (m_channel == DAC_CHAN_1) {
    m_ch1_locked = false;
  }
  
  // decrease object counter
  m_objectCount--;
}

//
// Get the GPIO number of our assigned DAC channel. 
// Parameter: gpio_num...address of variable to hold the GPIO number
//
esp_err_t DacESP32::getGPIOnum(gpio_num_t *gpio_num)
{
  CHANNEL_CHECK(m_channel);

  if (m_channel == DAC_CHAN_0)
      *gpio_num = GPIO_NUM_25;
  else
      *gpio_num = GPIO_NUM_26;

  return ESP_OK;
}

//
// Enable DAC output.
//
esp_err_t DacESP32::enable()
{
  CHANNEL_CHECK(m_channel);

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    if (!((dac_cosine_s_ *)m_cosine_handle)->is_started)
      return dac_cosine_start(m_cosine_handle);
    else
      return ESP_OK;
  }
  if (m_oneshot_value >= 0) {
    return outputVoltage((uint8_t)m_oneshot_value);
  }

  log_w("no oneshot/cosine DAC channel registered");
  return ESP_OK;  
}

//
// Disable DAC output.
//
esp_err_t DacESP32::disable()
{
  CHANNEL_CHECK(m_channel);

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    if (((dac_cosine_s_ *)m_cosine_handle)->is_started)
      return dac_cosine_stop(m_cosine_handle);
    else
      return ESP_OK;
  }
  if (m_oneshot_handle != DAC_ONE_HANDLE_UNDEFINED) {
    esp_err_t err = dac_oneshot_del_channel(m_oneshot_handle);
    m_oneshot_handle = DAC_ONE_HANDLE_UNDEFINED;
    return err;
  }     
  
  log_w("no oneshot/cosine DAC channel registered");
  return ESP_OK;
}

//
// Set DAC output voltage. 
// Parameter: value...DAC output value
//                    DAC output is 8-bit. Maximum (255) corresponds to ~VDD.
//                    Range 0...255
//
esp_err_t DacESP32::outputVoltage(uint8_t value)
{
  esp_err_t err;
  
  CHANNEL_CHECK(m_oneshot_cfg.chan_id);

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    // stop and delete DAC cosine channel 
    if (((dac_cosine_s_ *)m_cosine_handle)->is_started)
      dac_cosine_stop(m_cosine_handle);
    log_d("dac cosine delete: m_cosine_handle=0x%x, channel=%d", (int)m_cosine_handle, (int)m_cosine_cfg.chan_id);
    dac_cosine_del_channel(m_cosine_handle);
    m_cosine_handle = DAC_COS_HANDLE_UNDEFINED;
  }  

  if (m_oneshot_handle == DAC_ONE_HANDLE_UNDEFINED) {
    // register DAC oneshot channel
    if ((err = dac_oneshot_new_channel(&m_oneshot_cfg, &m_oneshot_handle)) != ESP_OK) {
      log_e("dac_oneshot_new_channel() error %d, channel=%d", err, (int)m_oneshot_cfg.chan_id);
      return(err);
    }
    log_d("dac oneshot init: m_oneshot_handle=0x%x, channel=%d", (int)m_oneshot_handle, (int)m_oneshot_cfg.chan_id);
  }

  m_oneshot_value = value;

  return dac_oneshot_output_voltage(m_oneshot_handle, value);
}

//
// Set DAC output voltage. 
// Parameter: value...DAC output voltage (in Volt)
//                    Range 0...CHANNEL_VOLTAGE_MAX (see definition above)
//
esp_err_t DacESP32::outputVoltage(float voltage)
{
  if (voltage < 0 )
    voltage = 0;
  else if (voltage > (float) CHANNEL_VOLTAGE_MAX)
    voltage = (float) CHANNEL_VOLTAGE_MAX;

  return outputVoltage((uint8_t)((voltage / (float) CHANNEL_VOLTAGE_MAX) * 255));
}

//
// Config CW generator and enable output on selected channel.
//
// The ESP32 Arduino framework by default can only generate CW output frequencies above 130Hz and only in steps of ~122Hz.
//
// However, this library allows to set the CW frequency more accurately starting from 16Hz and upwards. This is done 
// by altering register values for SENS_SAR_SW_FSTEP and RTC_CNTL_CK8M_DIV_SEL directly in the ESP32 DAC section. 
// Various combinations of the two variables are tried in order to hit the target frequency as close as possible.
//
// Note: By doing so the digital controller clock of both the DAC and (!) ADC sections in the ESP32 might get reduced
//       and in this case the program looptime will increase! 
//       If this causes problems in your program then you must use build option 'CW_FREQUENCY_HIGH_ACCURACY=0'
//       which brings back the default behaviour as explained above.
//
esp_err_t DacESP32::outputCW(uint32_t frequency)
{
  return outputCW(frequency, m_cosine_cfg.atten, m_cosine_cfg.phase, m_cosine_cfg.offset);
}

esp_err_t DacESP32::outputCW(uint32_t frequency, dac_cosine_atten_t atten, dac_cosine_phase_t phase, int8_t offset)
{
  esp_err_t err;

  CHANNEL_CHECK(m_cosine_cfg.chan_id);
  FREQUENCY_CHECK(frequency);
  m_cosine_cfg.freq_hz = m_cwFrequency = frequency;
  m_cosine_cfg.atten = atten;
  m_cosine_cfg.phase = phase;
  m_cosine_cfg.offset = offset;
  m_oneshot_value = -1;

  // Delete DAC oneshot channel if active
  if (m_oneshot_handle != DAC_ONE_HANDLE_UNDEFINED) {
    log_d("dac oneshot delete: m_oneshot_handle=0x%x, channel=%d", (int)m_oneshot_handle, (int)m_oneshot_cfg.chan_id);
    dac_oneshot_del_channel(m_oneshot_handle);
    m_oneshot_handle = DAC_ONE_HANDLE_UNDEFINED;
  }   
  
  // Changing DAC parameters is only possible at registration, so unregister first if necessary
  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    if (((dac_cosine_s_ *)m_cosine_handle)->is_started)
      dac_cosine_stop(m_cosine_handle);
    log_d("dac cosine delete: m_cosine_handle=0x%x, channel=%d", (int)m_cosine_handle, (int)m_cosine_cfg.chan_id);
    dac_cosine_del_channel(m_cosine_handle);
    m_cosine_handle = DAC_COS_HANDLE_UNDEFINED;
  }

#ifdef CW_FREQUENCY_HIGH_ACCURACY
  uint8_t clkdiv;
  uint32_t sw_fstep;

  if ((err = calcFrequSettings(m_cosine_cfg.freq_hz, &clkdiv, &sw_fstep)) != ESP_OK) return err;
  // frequencies below 130Hz (default fmin) will only pass the new dac_cosine driver frequency check if rtc_clk_freq is reduced
  if (frequency < 130) REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, CK8M_DIV_MAX);
#endif

  if ((err = dac_cosine_new_channel(&m_cosine_cfg, &m_cosine_handle)) != ESP_OK) {
    if (err == ESP_ERR_NOT_SUPPORTED)
      log_e("invalid parameter: frequency (%d) out of range", (int)m_cosine_cfg.freq_hz);
    else
      log_e("dac_cosine_new_channel() error %d, channel=%d", err, (int)m_cosine_cfg.chan_id);
    return(err);
  }
  log_d("dac cosine init: m_cosine_handle=0x%x, channel=%d", (int)m_cosine_handle, (int)m_cosine_cfg.chan_id);

#ifdef CW_FREQUENCY_HIGH_ACCURACY
  REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, clkdiv);
  SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL1_REG, SENS_SW_FSTEP, sw_fstep, SENS_SW_FSTEP_S);
#endif
  dac_cosine_start(m_cosine_handle);

  return ESP_OK;
}

//
// Helper function: Calculates DAC parameters for various frequency settings.
// Produces higher frequency accuracy only without build option CW_FREQUENCY_HIGH_ACCURACY=0.
//
esp_err_t DacESP32::calcFrequSettings(const uint32_t frequency, uint8_t *clkdiv, uint32_t *sw_fstep)
{
  uint8_t  clk8mDiv = 0, div, divMax = 0;
  uint32_t swfstep = 0, fcw = 0, deltaAbs;
  float 
// prevents error with build flag '-Werror=unused-but-set-variable' combined with core debug levels < 4
#if CORE_DEBUG_LEVEL >= 4  
        stepSize = 0, 
#endif        
        stepSizeTemp;

#ifdef CW_FREQUENCY_HIGH_ACCURACY        
  divMax = CK8M_DIV_MAX;
#else
  divMax = 0;
#endif  

  // delta to start with (biggest possible stepsize + 1)
  deltaAbs = ((float)CK8M / 65536UL) + 1;
 
  // searching best combination for RTC_CNTL_CK8M_DIV_SEL & SENS_SW_FSTEP
  for (div = 0; div <= divMax; ) {
    stepSizeTemp = (((float)CK8M / (1 + div)) / 65536UL);
    for (uint16_t fstep = 1; fstep <= SW_FSTEP_MAX; fstep++) {
      fcw = (uint32_t)(stepSizeTemp * fstep);
      if (fcw > (frequency + deltaAbs)) {
        // target gets out of reach (fcw >> ftarget)
        break;
      }
      // calculate deviation from target frequency
      int dtemp = (int)(fcw - frequency);
      log_v("fcw = %d, deltaAbs = %d, dtemp = %d, div = %d, stepSize = %f", fcw, deltaAbs, dtemp, div, stepSizeTemp);
      if ((uint32_t)abs(dtemp) < deltaAbs) {
        // better combination found
        deltaAbs = (uint32_t)abs(dtemp);
        clk8mDiv = div;
        swfstep = fstep;
#if CORE_DEBUG_LEVEL >= 4    
        stepSize = stepSizeTemp;
#endif
      }
      if (deltaAbs == 0) goto end;
    }

    div++;
    if ((int32_t)((((float)CK8M / (1 + div)) / 65536UL) * SW_FSTEP_MAX) < ((int32_t)(frequency - deltaAbs))) {
      // target gets out of reach (fcw << ftarget)
      break;
    }
  }
end:

  if (clk8mDiv == 0 && swfstep == 0) {
    // no suitable combination found
    log_e("invalid parameter: frequency (%d) out of range", frequency);
    return ESP_ERR_NOT_SUPPORTED;
  }

  log_d("ftarget=%dHz, fcw=%dHz, abs(delta)=%d, clk8mDiv=%d, sw_fstep=%d, stepSize=%fHz", 
        frequency, (uint32_t)(stepSize * swfstep), deltaAbs, clk8mDiv, swfstep, stepSize);

  // return calculated values
  *clkdiv = clk8mDiv;
  *sw_fstep = swfstep;

  return ESP_OK;
}

//
// Changes the frequency of the cosine wave (CW) generator without (re-)registering the channel with
// outputCW() and therefore bypassing the dac_cosine driver. This might have some negative side effects
// though. However, much faster than changing the output frequency with outputCW().
// Useful e.g. for very fast frequency sweeps without any dropouts in the channel output.
//
esp_err_t DacESP32::setCwFrequency(uint32_t frequency)
{
  esp_err_t err;
  uint8_t clkdiv;
  uint32_t sw_fstep;

  CHANNEL_CHECK(m_channel);
  FREQUENCY_CHECK(frequency);

  if ((err = calcFrequSettings(frequency, &clkdiv, &sw_fstep)) != ESP_OK) 
    return err;

  m_cosine_cfg.freq_hz = m_cwFrequency = frequency;

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    // DAC cosine channel already active
    REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, clkdiv);
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL1_REG, SENS_SW_FSTEP, sw_fstep, SENS_SW_FSTEP_S);
  }

  return ESP_OK;
}

//
// Set the amplitude of the cosine wave (CW) generator output.
// Parameter: attenuation, possible values: 
//        DAC_COSINE_ATTEN_DB_0....max. amplitude (0dB, real measurements: Vmin=~0.04V, Vmax=~3.18V)
//        DAC_COSINE_ATTEN_DB_6....1/2 amplitude (-6dB)
//        DAC_COSINE_ATTEN_DB_12...1/4 amplitude (-12dB)
//        DAC_COSINE_ATTEN_DB_18...1/8 amplitude (-18dB)
//
// Note: Register mutation is a much quicker way than to deregister a cosine channel and register it 
//       again with altered amplitude parameter.
//
esp_err_t DacESP32::setCwScale(dac_cosine_atten_t atten)
{
  CHANNEL_CHECK(m_channel);

  if (atten != DAC_COSINE_ATTEN_DB_0 && atten != DAC_COSINE_ATTEN_DB_6 &&
      atten != DAC_COSINE_ATTEN_DB_12 && atten != DAC_COSINE_ATTEN_DB_18) {
    return ESP_ERR_INVALID_ARG;
  }

  m_cosine_cfg.atten = atten;

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    // DAC cosine channel already active
    if (m_channel == DAC_CHAN_0) {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE1, atten, SENS_DAC_SCALE1_S);
    }
    else {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE2, atten, SENS_DAC_SCALE2_S);
    }
  }
  
  return ESP_OK;
}

//
// Set DC offset for CW generator of selected DAC channel.
// Parameter: offset - allowed range -128...127 (8-bit)
//
// Note: 1) Unreasonable settings can cause waveform to be oversaturated (clipped).
//          Unclipped output signal with max. amplitude requires offset = 0 !
//       2) Register mutation is a much quicker way than to deregister a cosine channel and 
//          register it again with altered offset parameter.
//
esp_err_t DacESP32::setCwOffset(int8_t offset)
{
  CHANNEL_CHECK(m_channel);

  m_cosine_cfg.offset = offset;

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    // DAC cosine channel already active
    if (m_channel == DAC_CHAN_0) {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC1, offset, SENS_DAC_DC1_S);
    }
    else {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC2, offset, SENS_DAC_DC2_S);
    }
  }

  return ESP_OK;
}

//
// Setting the phase of the CW generator output.
// Parameter: phase - selected phase (0°/180°)
//
// Note: Register mutation is a much quicker way than to deregister a cosine channel and register it 
//       again with altered phase parameter.
//
esp_err_t DacESP32::setCwPhase(dac_cosine_phase_t phase)
{
  CHANNEL_CHECK(m_channel);

  if (phase != DAC_COSINE_PHASE_0 && phase != DAC_COSINE_PHASE_180) {
    return ESP_ERR_INVALID_ARG;
  }

  m_cosine_cfg.phase = phase;

  if (m_cosine_handle != DAC_COS_HANDLE_UNDEFINED) {
    // DAC cosine channel already active
    if (m_channel == DAC_CHAN_0) {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV1, phase, SENS_DAC_INV1_S);
    }
    else {
      SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV2, phase, SENS_DAC_INV2_S);
    }
  }

  return ESP_OK;
}

#ifdef DACESP32_DEBUG_FUNCTIONS_ENABLED
//
// Print object variables and/or DAC related register settings (only useful for debugging purposes!)
//
void DacESP32::printObjectVariables(const char *s)
{
  Serial.printf("\nObject Variables [%s]:\n", s);
  Serial.printf("  m_ch0_locked=%d, m_ch1_locked=%d, m_oneshot_handle=0x%x, m_cosine_handle=0x%x\n", 
                m_ch0_locked, m_ch1_locked, (int)m_oneshot_handle, (int)m_cosine_handle);
  Serial.printf("  m_oneshot_cfg.chan_id=%d, m_cosine_cfg.chan_id=%d, m_objectCount=%d, m_cwFrequency=%u\n", 
                m_oneshot_cfg.chan_id, m_cosine_cfg.chan_id, m_objectCount, (unsigned int)m_cwFrequency);
  Serial.printf("  m_cosine_cfg.atten=%d,    m_cosine_cfg.phase=%d,   m_cosine_cfg.offset=%d\n", 
                m_cosine_cfg.atten, m_cosine_cfg.phase, m_cosine_cfg.offset);
}

void DacESP32::printDacRegisterSettings(const char *s)
{
  uint32_t clk   = REG_READ(RTC_CNTL_CLK_CONF_REG),
           dac1  = READ_PERI_REG(RTCIO_PAD_DAC1_REG),
           dac2  = READ_PERI_REG(RTCIO_PAD_DAC2_REG),
           ctrl1 = READ_PERI_REG(SENS_SAR_DAC_CTRL1_REG),
           ctrl2 = READ_PERI_REG(SENS_SAR_DAC_CTRL2_REG);
  unsigned int ck8mdiv = (unsigned int)(clk >> RTC_CNTL_CK8M_DIV_SEL_S) & RTC_CNTL_CK8M_DIV_SEL_V,
               fstep = (unsigned int)(ctrl1 >> SENS_SW_FSTEP_S) & SENS_SW_FSTEP_V,
               fcw = (unsigned int)((((float)CK8M / (1 + ck8mdiv)) / 65536UL) * fstep),
               stepsize = (unsigned int)(((float)CK8M / (1 + ck8mdiv)) / 65536UL);

  Serial.printf("\nDAC related Register Settings [%s]:\n", s);               
  Serial.printf("Register: RTC_CNTL_CLK_CONF_REG=0x%08x\n", (unsigned int)clk);
                 // RTC_CNTL_CK8M_DFREQ: value controls tuning of 8M clock
                 // RTC_CNTL_CK8M_DIV_SEL: configures 8M clock division
  Serial.printf("  RTC_CNTL_FAST_CLK_RTC_SEL=%u, RTC_CNTL_CK8M_DFREQ=%u, RTC_CNTL_CK8M_DIV_SEL=%u\n", 
                (unsigned int)(clk >> RTC_CNTL_FAST_CLK_RTC_SEL_S) & RTC_CNTL_FAST_CLK_RTC_SEL_V, 
                (unsigned int)(clk >> RTC_CNTL_CK8M_DFREQ_S) & RTC_CNTL_CK8M_DFREQ_V,
                ck8mdiv);
  Serial.printf("Register: RTCIO_PAD_DAC1_REG=0x%08x\n", (unsigned int)dac1);
  Serial.printf("  RTCIO_PAD_PDAC1_RDE=%u,     RTCIO_PAD_PDAC1_RUE=%u\n", 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_RDE_S) & RTCIO_PAD_PDAC_RDE_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_RUE_S) & RTCIO_PAD_PDAC_RUE_V);
  Serial.printf("  RTCIO_PAD_PDAC1_SLP_IE=%u,  RTCIO_PAD_PDAC1_SLP_OE=%u,        RTCIO_PAD_PDAC1_FUN_IE=%u\n",
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_SLP_IE_S) & RTCIO_PAD_PDAC_SLP_IE_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_SLP_OE_S) & RTCIO_PAD_PDAC_SLP_OE_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_FUN_IE_S) & RTCIO_PAD_PDAC_FUN_IE_V);
  Serial.printf("  RTCIO_PAD_PDAC1_DRV=%u,     RTCIO_PAD_PDAC1_DAC=0x%02x (%03u),  RTCIO_PAD_PDAC1_XPD_DAC=%u\n", 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_DRV_S) & RTCIO_PAD_PDAC_DRV_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V,
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_XPD_DAC_S) & RTCIO_PAD_PDAC_XPD_DAC_V);
  Serial.printf("  RTCIO_PAD_PDAC1_MUX_SEL=%u, RTCIO_PAD_PDAC1_DAC_XPD_FORCE=%u\n", 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_MUX_SEL_S) & RTCIO_PAD_PDAC_MUX_SEL_V, 
                (unsigned int)(dac1 >> RTCIO_PAD_PDAC_DAC_XPD_FORCE_S) & RTCIO_PAD_PDAC_DAC_XPD_FORCE_V);
  Serial.printf("Register: RTCIO_PAD_DAC2_REG=0x%08x\n", (unsigned int)dac2);
  Serial.printf("  RTCIO_PAD_PDAC2_RDE=%u,     RTCIO_PAD_PDAC2_RUE=%u\n", 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_RDE_S) & RTCIO_PAD_PDAC_RDE_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_RUE_S) & RTCIO_PAD_PDAC_RUE_V);
  Serial.printf("  RTCIO_PAD_PDAC2_SLP_IE=%u,  RTCIO_PAD_PDAC2_SLP_OE=%u,        RTCIO_PAD_PDAC2_FUN_IE=%u\n",
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_SLP_IE_S) & RTCIO_PAD_PDAC_SLP_IE_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_SLP_OE_S) & RTCIO_PAD_PDAC_SLP_OE_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_FUN_IE_S) & RTCIO_PAD_PDAC_FUN_IE_V);
  Serial.printf("  RTCIO_PAD_PDAC2_DRV=%u,     RTCIO_PAD_PDAC2_DAC=0x%02x (%03d),  RTCIO_PAD_PDAC2_XPD_DAC=%u\n", 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_DRV_S) & RTCIO_PAD_PDAC_DRV_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V,
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_XPD_DAC_S) & RTCIO_PAD_PDAC_XPD_DAC_V);
  Serial.printf("  RTCIO_PAD_PDAC2_MUX_SEL=%u, RTCIO_PAD_PDAC2_DAC_XPD_FORCE=%u\n", 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_MUX_SEL_S) & RTCIO_PAD_PDAC_MUX_SEL_V, 
                (unsigned int)(dac2 >> RTCIO_PAD_PDAC_DAC_XPD_FORCE_S) & RTCIO_PAD_PDAC_DAC_XPD_FORCE_V);
  Serial.printf("Register: SENS_SAR_DAC_CTRL1_REG=0x%08x\n", (unsigned int)ctrl1);
  Serial.printf("  SENS_SW_TONE_EN=%u,         SENS_SW_FSTEP=%u ---> resulting fcw=%uHz, stepsize=%uHz~\n", 
                (unsigned int)(ctrl1 >> SENS_SW_TONE_EN_S) & SENS_SW_TONE_EN_V, fstep, fcw, stepsize);
  Serial.printf("Register: SENS_SAR_DAC_CTRL2_REG=0x%08x\n", (unsigned int)ctrl2);
  Serial.printf("  SENS_DAC_CW_EN1=%u,         SENS_DAC_CW_EN2=%u\n", 
                (unsigned int)(ctrl2 >> SENS_DAC_CW_EN1_S) & SENS_DAC_CW_EN1_V, 
                (unsigned int)(ctrl2 >> SENS_DAC_CW_EN2_S) & SENS_DAC_CW_EN2_V);
  Serial.printf("  SENS_DAC_INV1=%u,           SENS_DAC_INV2=%u\n", 
                (unsigned int)(ctrl2 >> SENS_DAC_INV1_S) & SENS_DAC_INV1_V, 
                (unsigned int)(ctrl2 >> SENS_DAC_INV2_S) & SENS_DAC_INV2_V);
  Serial.printf("  SENS_DAC_SCALE1=%u,         SENS_DAC_SCALE2=%u\n", 
                (unsigned int)(ctrl2 >> SENS_DAC_SCALE1_S) & SENS_DAC_SCALE1_V, 
                (unsigned int)(ctrl2 >> SENS_DAC_SCALE2_S) & SENS_DAC_SCALE2_V);
  Serial.printf("  SENS_DAC_DC1=0x%04x,       SENS_DAC_DC2=0x%04x\n", 
                (unsigned int)(ctrl2 >> SENS_DAC_DC1_S) & SENS_DAC_DC1_V, 
                (unsigned int)(ctrl2 >> SENS_DAC_DC2_S) & SENS_DAC_DC2_V);
}
#endif

