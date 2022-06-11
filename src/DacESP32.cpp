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

// All CW generator frequency calculations are done with the assumption 
// of RTC8M_CLK (clock source that feeds both DACs controller section) to run 
// near 8MHz. If your ESP32 exemplar is way off you might want to uncomment 
// below line for adjustment/tuning. Default value is 172. Lowering the value 
// will lower the frequency and vice versa. More detailed infos in Readme.md.
//#define CK8M_DFREQ_ADJUSTED 172

// Enables a more accurate setting of the CW generator output frequency. Be aware,
// the digital controller clock (dig_clk_rtc_freq) of both the DAC and ADC 
// modules might get changed (due to altered value of RTC_CNTL_CK8M_DIV_SEL). 
// Comment line out if this causes problems or high frequency accuracy is not 
// needed.
#define CW_FREQUENCY_HIGH_ACCURACY

// Below value defines the CW generators minimum number of voltage steps per cycle.
// Too low values will reduce the maximal possible CW output frequency though.
// Lowering the value will increase the minimum number of voltage steps/cycle and
// vice versa. More detailed infos about setting the value are found in Readme.md.
// Choose a value that fits your application best.
#define SW_FSTEP_MAX  256

// The maximum possible DAC output voltage depends on the actual supply voltage (VDD) 
// of your ESP32. It will vary with the used LDO voltage regulator on your board and
// other factors. To generate a more precise output voltage: Generate max voltage 
// level on a DAC channel with outputVoltage(255) and measure the real voltage on 
// it (with only light or no load!). Then replace below value with the measured one.
#define CHANNEL_VOLTAGE_MAX (float) 3.30

#define CHANNEL_CHECK                               \
  if (m_channel == DAC_CHANNEL_UNDEFINED) {         \
    log_e("channel setting invalid");               \
    return ESP_FAIL;                                \
  } 

// initialize static members of class (shared by all created objects)
size_t   DacESP32::m_objectCount = 0;     // clear object count
uint32_t DacESP32::m_cwFrequency = 0;     // invalidate CW generator frequency

//
// Class constructor.
// Parameter: channel...assigned DAC channel
//
DacESP32::DacESP32(dac_channel_t channel) 
{
  if (channel != DAC_CHANNEL_1 && channel != DAC_CHANNEL_2) {
    m_channel = DAC_CHANNEL_UNDEFINED;
    return;
  }

  m_channel = channel;
  dac_output_disable(m_channel);

  // default CW generator settings for this channel
  m_cwScale = DAC_CW_SCALE_1;
  m_cwPhase = DAC_CW_PHASE_0;
  m_cwOffset = DAC_CW_OFFSET_DEFAULT;

  // frequency setting common to all objects
  if (m_cwFrequency == 0) {
    // CW generator not yet in use
#ifdef CK8M_DFREQ_ADJUSTED
    REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DFREQ, CK8M_DFREQ_ADJUSTED);
#endif
    // set CK8M_DIV = 0 (default)
    REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, 0);
  }

  // increase every time object is created
  m_objectCount++;
  
  if (m_objectCount > DAC_CHANNEL_MAX) {
    log_w("DacESP32 objects created = %d > %d (DAC channels available) !", m_objectCount, DAC_CHANNEL_MAX);
  }
}

//
// Class constructor.
// Parameter: pin...assigned GPIO pin
//
DacESP32::DacESP32(gpio_num_t pin) 
  : DacESP32((dac_channel_t)(pin == DAC_CHANNEL_1_GPIO_NUM) ? DAC_CHANNEL_1 : 
                            (pin == DAC_CHANNEL_2_GPIO_NUM) ? DAC_CHANNEL_2 : DAC_CHANNEL_UNDEFINED)
{
}

//
// Class destructor.
//
DacESP32::~DacESP32()
{
  if (m_channel != DAC_CHANNEL_UNDEFINED) {
    dac_output_disable(m_channel);
  }

  // decrease counter when object is destroyed
  m_objectCount--;

  // disable CW generator if no objects left
  if (m_objectCount == 0) {
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
  }
}

//
// Get the GPIO number of our assigned DAC channel. 
// Parameter: gpio_num...address of variable to hold the GPIO number
//
esp_err_t DacESP32::getGPIOnum(gpio_num_t *gpio_num)
{
  CHANNEL_CHECK;

  return dac_pad_get_io_num(m_channel, gpio_num);
}

//
// (Re-)Assign DAC channel.
//
esp_err_t DacESP32::setChannel(dac_channel_t channel)
{
  if (channel != DAC_CHANNEL_1 && channel != DAC_CHANNEL_2) {
    log_e("parameter DAC channel/pin invalid");
    return ESP_ERR_INVALID_ARG;
  }

  if (m_channel != channel) {
    if (m_channel != DAC_CHANNEL_UNDEFINED) {
      dac_output_disable(m_channel);
    }
    dac_output_disable(channel);
    m_channel = channel;
  }

  return ESP_OK;
}

esp_err_t DacESP32::setPin(gpio_num_t pin)
{
  return setChannel((dac_channel_t)(pin == DAC_CHANNEL_1_GPIO_NUM) ? DAC_CHANNEL_1 : 
                                   (pin == DAC_CHANNEL_2_GPIO_NUM) ? DAC_CHANNEL_2 : DAC_CHANNEL_UNDEFINED);
}

//
// Enable DAC output.
//
esp_err_t DacESP32::enable()
{
  CHANNEL_CHECK;

  return dac_output_enable(m_channel);
}

//
// Disable DAC output.
//
esp_err_t DacESP32::disable()
{
  CHANNEL_CHECK;

  return dac_output_disable(m_channel);
}

//
// Set DAC output voltage. 
// Parameter: value...DAC output voltage (in Volt)
//                    Range 0...CHANNEL_VOLTAGE_MAX (see #definition above)
//
esp_err_t DacESP32::outputVoltage(float voltage)
{
  if (voltage < 0 )
    voltage = 0;
  else if (voltage > CHANNEL_VOLTAGE_MAX)
    voltage = CHANNEL_VOLTAGE_MAX;

  return outputVoltage((uint8_t)((voltage / CHANNEL_VOLTAGE_MAX) * 255));
}

//
// Set DAC output voltage. 
// Parameter: value...DAC output value
//                    DAC output is 8-bit. Maximum (255) corresponds to ~VDD.
//                    Range 0...255
//
esp_err_t DacESP32::outputVoltage(uint8_t value)
{
  CHANNEL_CHECK;

  if (m_channel == DAC_CHANNEL_1) {
    // disable CW generator on channel 1
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
    // enable DAC channel output
    SET_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTCIO_PAD_PDAC1_MUX_SEL | RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);
    // setting DAC output value
    SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, value, RTC_IO_PDAC1_DAC_S);
  }
  else {
    // disable CW generator on channel 2
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
    // enable DAC channel output
    SET_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTCIO_PAD_PDAC2_MUX_SEL | RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);
    // setting DAC output value
    SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, value, RTC_IO_PDAC2_DAC_S);
  }

  return ESP_OK;
}

//
// Following the routines for using the Cosine Waveform (CW) Generator.
// Readme.md provides more technical details about the CW generator. 
//
// Config CW generator & enable for output.
//
esp_err_t DacESP32::outputCW(uint32_t frequency)
{
  return outputCW(frequency, m_cwScale, m_cwPhase, m_cwOffset);
}

esp_err_t DacESP32::outputCW(uint32_t frequency, dac_cw_scale_t scale, dac_cw_phase_t phase, int8_t offset)
{
  CHANNEL_CHECK;

  esp_err_t result;
  
  // configure CW settings
  if ((result = setCwFrequency(frequency)) != ESP_OK ||
      (result = setCwScale(scale)) != ESP_OK ||
      (result = setCwPhase(phase)) != ESP_OK ||
      (result = setCwOffset(offset)) != ESP_OK) {

    return result;
  }

  dac_output_enable(m_channel);
  dacCwSelect();

  return ESP_OK;
}

//
// Sets CW frequency with lower accuracy (constant frequency steps ~122Hz) or higher accuracy 
// (frequency steps from ~15Hz up to ~122Hz) when CW_FREQUENCY_HIGH_ACCURACY is defined, 
// in this case CK8M_DIV_SEL gets changed. The highest frequency possible and the min. number 
// of voltage steps per cycle depend on definition of SW_FSTEP_MAX.
// Parameter: frequency - possible range is ~15...fmax
//             fmax: ~62.6kHz (SW_FSTEP_MAX = 512 --> voltage steps/cycle >= 128)
//             fmax: ~31.3kHz (SW_FSTEP_MAX = 256 --> voltage steps/cycle >= 256)
//             fmax: ~15.6kHz (SW_FSTEP_MAX = 128 --> voltage steps/cycle >= 512)
//
esp_err_t DacESP32::setCwFrequency(uint32_t frequency)
{
  if (frequency == 0) {
    log_e("invalid parameter: frequency (%d) out of range", frequency);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t  clk8mDiv = 0, div, divMax = 0;
  uint32_t frequencyStep = 0, fcw = 0, deltaAbs;
  float    stepSize, stepSizeTemp;

#ifdef CW_FREQUENCY_HIGH_ACCURACY
  divMax = CK8M_DIV_MAX;
#endif

  // delta to start with (biggest possible stepsize + 1)
  deltaAbs = ((float)CK8M / 65536UL) + 1;
 
  // searching output frequency closest to target frequency
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
        frequencyStep = fstep;
        stepSize = stepSizeTemp;
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

  if (clk8mDiv == 0 && frequencyStep == 0) {
    // no suitable combination found
    log_e("invalid parameter: frequency (%d) out of range", frequency);
    return ESP_ERR_INVALID_ARG;
  }

  log_d("ftarget=%d, fcw=%d, abs(delta)=%d, clk8mDiv=%d, frequencyStep=%d, stepSize=%f", 
        frequency, (uint32_t)(stepSize * frequencyStep), deltaAbs, clk8mDiv, frequencyStep, stepSize);

#ifdef CW_FREQUENCY_HIGH_ACCURACY
  REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, clk8mDiv);
#endif
  SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL1_REG, SENS_SW_FSTEP, frequencyStep, SENS_SW_FSTEP_S);

  m_cwFrequency = frequency;

  return ESP_OK;
}

//
// Set the amplitude of the cosine wave (CW) generator output.
// Parameter: scale - scaling factor
//             0...no scale (default), max. amplitude (real measurements were Vmin=~40mV, Vmax=~3.18V)
//             1...scale to 1/2 
//             2...scale to 1/4 
//             3...scale to 1/8
//
esp_err_t DacESP32::setCwScale(dac_cw_scale_t scale)
{
  CHANNEL_CHECK;

  if (scale != DAC_CW_SCALE_1 && scale != DAC_CW_SCALE_2 &&
      scale != DAC_CW_SCALE_4 && scale != DAC_CW_SCALE_8) {
    return ESP_ERR_INVALID_ARG;
  }

  if (m_channel == DAC_CHANNEL_1) {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE1, scale, SENS_DAC_SCALE1_S);
  }
  else {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE2, scale, SENS_DAC_SCALE2_S);
  }
  m_cwScale = scale;

  return ESP_OK;
}

//
// Set DC offset for CW generator of selected DAC channel.
// Parameter: offset - allowed range -128...127 (8-bit)
// Note: Unreasonable settings can cause waveform to be oversaturated (clipped).
//       Unclipped output signal with max. amplitude requires offset = 0 !
//
esp_err_t DacESP32::setCwOffset(int8_t offset)
{
  CHANNEL_CHECK;

  if (m_channel == DAC_CHANNEL_1) {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC1, offset, SENS_DAC_DC1_S);
  }
  else {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC2, offset, SENS_DAC_DC2_S);
  }
  return ESP_OK;
}

//
// Setting phase of CW generator output.
// Parameter: phase - selected phase (0°/180°)
//
esp_err_t DacESP32::setCwPhase(dac_cw_phase_t phase)
{
  CHANNEL_CHECK;

  if (phase != DAC_CW_PHASE_0 && phase != DAC_CW_PHASE_180) {
    return ESP_ERR_INVALID_ARG;
  }

  if (m_channel == DAC_CHANNEL_1) {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV1, phase, SENS_DAC_INV1_S);
  }
  else {
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV2, phase, SENS_DAC_INV2_S);
  }
  m_cwPhase = phase;

  return ESP_OK;
}


//
// Selects CW generator as source for DAC channel
//
esp_err_t DacESP32::dacCwSelect()
{
  //CHANNEL_CHECK;

  if (m_channel == DAC_CHANNEL_1) {
    SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
  }
  else {
    SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
  }

  // enable CW generator
  SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);

  return ESP_OK;
}

//
// Deselects CW generator as source for DAC channel
//
esp_err_t DacESP32::dacCwDeselect()
{
  //CHANNEL_CHECK;

  if (m_channel == DAC_CHANNEL_1) {
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
  }
  else {
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
  }

  if (!((READ_PERI_REG(SENS_SAR_DAC_CTRL2_REG) >> SENS_DAC_CW_EN1_S) & 1UL) &&
      !((READ_PERI_REG(SENS_SAR_DAC_CTRL2_REG) >> SENS_DAC_CW_EN2_S) & 1UL)) {
    // disable unused CW generator
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
  }

  return ESP_OK;
}

#ifdef DACESP32_DEBUG_FUNCTIONS_ENABLED
//
// Print object variables (only useful for debugging purposes)
//
void DacESP32::printObjectVariables()
{
  Serial.println("\nObject Variables:");
  Serial.printf("  m_channel=%d, m_objectCount=%d, m_cwFrequency=%d\n", m_channel, m_objectCount, m_cwFrequency);
  Serial.printf("  m_cwScale=%d, m_cwPhase=%d, m_cwOffset=%d\n", m_cwScale, m_cwPhase, m_cwOffset);
}

//
// Print selected register settings affecting the DAC system (only useful for debugging purposes)
//
void DacESP32::printDacRegisterSettings()
{
  uint32_t clk   = REG_READ(RTC_CNTL_CLK_CONF_REG),
           dac1  = READ_PERI_REG(RTCIO_PAD_DAC1_REG),
           dac2  = READ_PERI_REG(RTCIO_PAD_DAC2_REG),
           ctrl1 = READ_PERI_REG(SENS_SAR_DAC_CTRL1_REG),
           ctrl2 = READ_PERI_REG(SENS_SAR_DAC_CTRL2_REG);

  Serial.printf("\nRegister: RTC_CNTL_CLK_CONF_REG=0x%08x\n", clk);
                 // RTC_CNTL_CK8M_DFREQ: value controls tuning of 8M clock
                 // RTC_CNTL_CK8M_DIV_SEL: configures 8M clock division
  Serial.printf("  RTC_CNTL_FAST_CLK_RTC_SEL=%d, RTC_CNTL_CK8M_DFREQ=%d, RTC_CNTL_CK8M_DIV_SEL=%d\n", 
                (clk >> RTC_CNTL_FAST_CLK_RTC_SEL_S) & RTC_CNTL_FAST_CLK_RTC_SEL_V, (clk >> RTC_CNTL_CK8M_DFREQ_S) & RTC_CNTL_CK8M_DFREQ_V,
                (clk >> RTC_CNTL_CK8M_DIV_SEL_S) & RTC_CNTL_CK8M_DIV_SEL_V);
  Serial.printf("Register: RTCIO_PAD_DAC1_REG=0x%08x\n", dac1);
  Serial.printf("  RTCIO_PAD_PDAC1_RDE=%d,     RTCIO_PAD_PDAC1_RUE=%d\n", 
                (dac1 >> RTCIO_PAD_PDAC_RDE_S) & RTCIO_PAD_PDAC_RDE_V, (dac1 >> RTCIO_PAD_PDAC_RUE_S) & RTCIO_PAD_PDAC_RUE_V);
  Serial.printf("  RTCIO_PAD_PDAC1_DRV=%d,     RTCIO_PAD_PDAC1_DAC=0x%02x (%d),  RTCIO_PAD_PDAC1_XPD_DAC=%d\n", 
                (dac1 >> RTCIO_PAD_PDAC_DRV_S) & RTCIO_PAD_PDAC_DRV_V, 
                (dac1 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V, (dac1 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V,
                (dac1 >> RTCIO_PAD_PDAC_XPD_DAC_S) & RTCIO_PAD_PDAC_XPD_DAC_V);
  Serial.printf("  RTCIO_PAD_PDAC1_MUX_SEL=%d, RTCIO_PAD_PDAC1_DAC_XPD_FORCE=%d\n", 
                (dac1 >> RTCIO_PAD_PDAC_MUX_SEL_S) & RTCIO_PAD_PDAC_MUX_SEL_V, (dac1 >> RTCIO_PAD_PDAC_DAC_XPD_FORCE_S) & RTCIO_PAD_PDAC_DAC_XPD_FORCE_V);
  Serial.printf("Register: RTCIO_PAD_DAC2_REG=0x%08x\n", dac2);
  Serial.printf("  RTCIO_PAD_PDAC2_RDE=%d,     RTCIO_PAD_PDAC2_RUE=%d\n", 
                (dac2 >> RTCIO_PAD_PDAC_RDE_S) & RTCIO_PAD_PDAC_RDE_V, (dac2 >> RTCIO_PAD_PDAC_RUE_S) & RTCIO_PAD_PDAC_RUE_V);
  Serial.printf("  RTCIO_PAD_PDAC2_DRV=%d,     RTCIO_PAD_PDAC2_DAC=0x%02x (%d),  RTCIO_PAD_PDAC2_XPD_DAC=%d\n", 
                (dac2 >> RTCIO_PAD_PDAC_DRV_S) & RTCIO_PAD_PDAC_DRV_V, 
                (dac2 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V, (dac2 >> RTCIO_PAD_PDAC_DAC_S) & RTCIO_PAD_PDAC_DAC_V,
                (dac2 >> RTCIO_PAD_PDAC_XPD_DAC_S) & RTCIO_PAD_PDAC_XPD_DAC_V);
  Serial.printf("  RTCIO_PAD_PDAC2_MUX_SEL=%d, RTCIO_PAD_PDAC2_DAC_XPD_FORCE=%d\n", 
                (dac2 >> RTCIO_PAD_PDAC_MUX_SEL_S) & RTCIO_PAD_PDAC_MUX_SEL_V, (dac2 >> RTCIO_PAD_PDAC_DAC_XPD_FORCE_S) & RTCIO_PAD_PDAC_DAC_XPD_FORCE_V);
  Serial.printf("Register: SENS_SAR_DAC_CTRL1_REG=0x%08x\n", ctrl1);
  Serial.printf("  SENS_SW_TONE_EN=%d,   SENS_SW_FSTEP=%d\n", 
                (ctrl1 >> SENS_SW_TONE_EN_S) & SENS_SW_TONE_EN_V, (ctrl1 >> SENS_SW_FSTEP_S) & SENS_SW_FSTEP_V);
  Serial.printf("Register: SENS_SAR_DAC_CTRL2_REG=0x%08x\n", ctrl2);
  Serial.printf("  SENS_DAC_CW_EN1=%d,   SENS_DAC_CW_EN2=%d\n", 
                (ctrl2 >> SENS_DAC_CW_EN1_S) & SENS_DAC_CW_EN1_V, (ctrl2 >> SENS_DAC_CW_EN2_S) & SENS_DAC_CW_EN2_V);
  Serial.printf("  SENS_DAC_INV1=%d,     SENS_DAC_INV2=%d\n", 
                (ctrl2 >> SENS_DAC_INV1_S) & SENS_DAC_INV1_V, (ctrl2 >> SENS_DAC_INV2_S) & SENS_DAC_INV2_V);
  Serial.printf("  SENS_DAC_SCALE1=%d,   SENS_DAC_SCALE2=%d\n", 
                (ctrl2 >> SENS_DAC_SCALE1_S) & SENS_DAC_SCALE1_V, (ctrl2 >> SENS_DAC_SCALE2_S) & SENS_DAC_SCALE2_V);
  Serial.printf("  SENS_DAC_DC1=0x%04x, SENS_DAC_DC2=0x%04x\n", 
                (ctrl2 >> SENS_DAC_DC1_S) & SENS_DAC_DC1_V, (ctrl2 >> SENS_DAC_DC2_S) & SENS_DAC_DC2_V);
}
#endif

