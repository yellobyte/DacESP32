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

#ifndef DacESP32_h
#define DacESP32_h

#include <Arduino.h>
#include "soc/sens_reg.h"
#include "soc/dac_channel.h"
#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "hal/dac_types.h"

#ifndef DAC_CHAN0_GPIO_NUM
#error "DacESP32 lib version 2.x.x requires Arduino ESP32 Core v3.x.x. (ESP-IDF v5.1.4) or higher!"
#endif

#include "driver/dac_oneshot.h"
#include "driver/dac_cosine.h"

//
// definitions
//
#define RTCIO_PAD_DAC1_REG (DR_REG_RTCIO_BASE + 0x0084)
#define RTCIO_PAD_DAC2_REG (DR_REG_RTCIO_BASE + 0x0088)
#define RTCIO_PAD_PDAC_DRV_S            30
#define RTCIO_PAD_PDAC_DRV_V            0x3
#define RTCIO_PAD_PDAC_HOLD_S           29
#define RTCIO_PAD_PDAC_HOLD_V           0x1
#define RTCIO_PAD_PDAC_RDE_S            28
#define RTCIO_PAD_PDAC_RDE_V            0x1
#define RTCIO_PAD_PDAC_RUE_S            27
#define RTCIO_PAD_PDAC_RUE_V            0x1
#define RTCIO_PAD_PDAC_DAC_S            19
#define RTCIO_PAD_PDAC_DAC_V            0xFF
#define RTCIO_PAD_PDAC_XPD_DAC_S        18
#define RTCIO_PAD_PDAC_XPD_DAC_V        0x1
#define RTCIO_PAD_PDAC_MUX_SEL_S        17
#define RTCIO_PAD_PDAC_MUX_SEL_V        0x1
#define RTCIO_PAD_PDAC_SLP_IE_S         13
#define RTCIO_PAD_PDAC_SLP_IE_V         0x1
#define RTCIO_PAD_PDAC_SLP_OE_S         12
#define RTCIO_PAD_PDAC_SLP_OE_V         0x1
#define RTCIO_PAD_PDAC_FUN_IE_S         11
#define RTCIO_PAD_PDAC_FUN_IE_V         0x1
#define RTCIO_PAD_PDAC_DAC_XPD_FORCE_S  10
#define RTCIO_PAD_PDAC_DAC_XPD_FORCE_V  0x1

#define DAC_ONE_HANDLE_UNDEFINED (dac_oneshot_handle_t) -1
#define DAC_COS_HANDLE_UNDEFINED (dac_cosine_handle_t)  -1
#define DAC_CHAN_UNDEFINED (dac_channel_t) -1
#define DAC_CHAN_MAX       (dac_channel_t)  2
#define DAC_CW_OFFSET_DEFAULT 0
//#define DAC_CW_FREQU_DEFAULT  0
#define CK8M_DIV_MAX          7

// for compatibility reasons
#define DAC_CW_SCALE_1 DAC_COSINE_ATTEN_DB_0   // 1/1 (0dB, default)
#define DAC_CW_SCALE_2 DAC_COSINE_ATTEN_DB_6   // 1/2 (-6dB)
#define DAC_CW_SCALE_4 DAC_COSINE_ATTEN_DB_12  // 1/4 (-12dB)
#define DAC_CW_SCALE_8 DAC_COSINE_ATTEN_DB_18  // 1/8 (-18dB)

// Master clock for digital controller section of both (!) DAC & ADC systems. 
// According to spec the frequency is approximately 8MHz. Don't change this value !! 
// Use build option CK8M_DFREQ_ADJUSTED for tuning the output frequency.
#define CK8M 8000000UL

#if 0 // not used
typedef enum {
  DAC_CW_INVERT_NONE    = 0x0,
  DAC_CW_INVERT_ALL     = 0x1,
  DAC_CW_INVERT_MSB     = 0x2,
  DAC_CW_INVERT_NOT_MSB = 0x3
} dac_cw_invert_t;
#endif

// as defined in dac_cosine.c @ Arduino ESP32 v3.x.x
struct dac_cosine_s_ {
    dac_cosine_config_t cfg;
    bool                is_started;
};

// DacESP32 class
class DacESP32
{
  public:
    DacESP32(int pin); 
    DacESP32(gpio_num_t pin);
    DacESP32(dac_channel_t channel);
    ~DacESP32();
    esp_err_t getGPIOnum(gpio_num_t *gpio_num);
    esp_err_t enable(void);
    esp_err_t disable(void);
    esp_err_t outputVoltage(uint8_t value);
    esp_err_t outputVoltage(float voltage);
    esp_err_t outputCW(uint32_t frequency);
    esp_err_t outputCW(uint32_t frequency, dac_cosine_atten_t atten,
                       dac_cosine_phase_t phase = DAC_COSINE_PHASE_0, int8_t offset = DAC_CW_OFFSET_DEFAULT);                       
    esp_err_t setCwFrequency(uint32_t frequency);
    esp_err_t setCwScale(dac_cosine_atten_t attenuation);
    esp_err_t setCwOffset(int8_t offset);
    esp_err_t setCwPhase(dac_cosine_phase_t phase);
    dac_channel_t  getChannel() { return m_oneshot_cfg.chan_id; };
    dac_cosine_atten_t getCwScale() { return m_cosine_cfg.atten; };
    dac_cosine_phase_t getCwPhase() { return m_cosine_cfg.phase; };
    int8_t    getCwOffset() { return m_cosine_cfg.offset; };     

    #ifdef DACESP32_DEBUG_FUNCTIONS_ENABLED
    void printObjectVariables(const char *s = "");
    static void printDacRegisterSettings(const char *s = "");   
    #endif

    // shared by all objects of this class
    static size_t   m_objectCount;         // number of objects created
    static uint32_t m_cwFrequency;         // CW generator output frequency, shared by all objects 
    static bool     m_ch0_locked;    
    static bool     m_ch1_locked;

  private:
    esp_err_t calcFrequSettings(const uint32_t frequency, uint8_t *clkdiv, uint32_t *sw_fstep);
    
    dac_channel_t        m_channel;        // DAC channel this object is assigned to
    //gpio_num_t           m_gpio_pin;       // resp. GPIO pin numper

    int16_t              m_oneshot_value;  
    dac_oneshot_handle_t m_oneshot_handle;
    dac_oneshot_config_t m_oneshot_cfg;

    dac_cosine_handle_t  m_cosine_handle;
    dac_cosine_config_t  m_cosine_cfg;
};

#endif
