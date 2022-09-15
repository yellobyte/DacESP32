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
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/dac_channel.h"
#include "soc/rtc.h"
#include "driver/dac.h"

//
// definitions
//
#define RTCIO_PAD_DAC1_REG (DR_REG_RTCIO_BASE + 0x0084)
#define RTCIO_PAD_DAC2_REG (DR_REG_RTCIO_BASE + 0x0088)
#define RTCIO_PAD_PDAC_DRV_V  0x3
#define RTCIO_PAD_PDAC_DRV_S  30
#define RTCIO_PAD_PDAC_DAC_V  0xFF
#define RTCIO_PAD_PDAC_DAC_S  19
#define RTCIO_PAD_PDAC_XPD_DAC_V  0x1
#define RTCIO_PAD_PDAC_XPD_DAC_S  18
#define RTCIO_PAD_PDAC_MUX_SEL_V  0x1
#define RTCIO_PAD_PDAC_MUX_SEL_S  17
#define RTCIO_PAD_PDAC_RUE_S  27
#define RTCIO_PAD_PDAC_RUE_V  0x1
#define RTCIO_PAD_PDAC_RDE_S  28
#define RTCIO_PAD_PDAC_RDE_V  0x1
#define RTCIO_PAD_PDAC1_MUX_SEL (BIT(17))
#define RTCIO_PAD_PDAC2_MUX_SEL (BIT(17))
#define RTCIO_PAD_PDAC_DAC_XPD_FORCE_V  0x1
#define RTCIO_PAD_PDAC_DAC_XPD_FORCE_S  10

#define DAC_CHANNEL_UNDEFINED (dac_channel_t) -1
#define DAC_CW_OFFSET_DEFAULT 0
#define CK8M_DIV_MAX 7

// Master clock for digital controller section of both DAC & ADC systems.
// According to spec approximately 8MHz.
#define CK8M 8000000UL

// Type definitions needed only for older Espressif ESP32 framework versions:
//  <=1.0.6 on ArduinoIDE resp. <=3.3.0 on PlatformIO
// All newer framework versions provide these definitions in dac_types.h.

//#define DACESP32_TYPE_DEFS
#ifdef DACESP32_TYPE_DEFS
typedef enum {
    DAC_CW_SCALE_1 = 0x0,     // 1/1 (default)
    DAC_CW_SCALE_2 = 0x1,     // 1/2 
    DAC_CW_SCALE_4 = 0x2,     // 1/4
    DAC_CW_SCALE_8 = 0x3      // 1/8
} dac_cw_scale_t;

typedef enum {
    DAC_CW_PHASE_0   = 0x2,   // phase shift 0°
    DAC_CW_PHASE_180 = 0x3    // phase shift +180°
} dac_cw_phase_t;
#endif

typedef enum {
  DAC_CW_INVERT_NONE    = 0x0,
  DAC_CW_INVERT_ALL     = 0x1,
  DAC_CW_INVERT_MSB     = 0x2,
  DAC_CW_INVERT_NOT_MSB = 0x3
} dac_cw_invert_t;

// DacESP32 class
class DacESP32
{
  public:
    DacESP32(gpio_num_t pin);
    DacESP32(dac_channel_t channel);
    ~DacESP32();
    esp_err_t setPin(gpio_num_t pin);
    esp_err_t setChannel(dac_channel_t channel);
    esp_err_t getGPIOnum(gpio_num_t *gpio_num);
    esp_err_t enable(void);
    esp_err_t disable(void);
    esp_err_t outputVoltage(uint8_t value);
    esp_err_t outputVoltage(float voltage);
    esp_err_t outputCW(uint32_t frequency);
    esp_err_t outputCW(uint32_t frequency, dac_cw_scale_t scale,
                       dac_cw_phase_t phase = DAC_CW_PHASE_0, int8_t offset = DAC_CW_OFFSET_DEFAULT);                       
    esp_err_t setCwFrequency(uint32_t frequency);
    esp_err_t setCwScale(dac_cw_scale_t scale);
    esp_err_t setCwOffset(int8_t offset);
    esp_err_t setCwPhase(dac_cw_phase_t phase);
    dac_channel_t  getChannel() { return m_channel; };
    dac_cw_scale_t getCwScale() { return m_cwScale; };
    dac_cw_phase_t getCwPhase() { return m_cwPhase; };
    int8_t         getCwOffset() { return m_cwOffset; };     

    #ifdef DACESP32_DEBUG_FUNCTIONS_ENABLED
    void printObjectVariables(void);
    static void printDacRegisterSettings(void);   
    #endif

    // shared by all objects of this class
    static size_t   m_objectCount; // number of objects created
    static uint32_t m_cwFrequency; // CW generator output frequency, common to all objects 

  private:
    esp_err_t dacCwSelect(void);
    esp_err_t dacCwDeselect(void);
    
    dac_channel_t   m_channel;     // DAC channel this object is assigned to
    dac_cw_scale_t  m_cwScale;     // CW generator output amplitude
    dac_cw_phase_t  m_cwPhase;     // CW generator output phaseshift
    int8_t          m_cwOffset;    // CW generator output DC offset
};

#endif
