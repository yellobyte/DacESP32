# DacESP32

The ESP32 contains two 8-bit DACs (digital to analog converter) which convert a 8-bit value into a voltage level on the DAC output channels 1 & 2 (GPIO pins 25 & 26). In fact, the DAC channels can not only be set to arbitrary output voltages between 0...+3.3V (VDD) but driven by an integrated common cosine waveform (CW) generator as well. The CW generator is explained more detailed further down.

This Arduino library makes using the two ESP32 DAC output channels fast & easy.

Below an example for generating a sinus signal of ~2kHz on DAC channel 1 (GPIO pin 25) and a steady voltage level of about 1.3V (100/255 * 3.3V) on DAC channel 2 (GPIO pin 26).  

```c
...
#include "DacESP32.h"

DacESP32 dac1(DAC_CHANNEL_1)
         dac2(DAC_CHANNEL_2);

setup() {
  dac1.outputCW(2000);      // ~2kHz sinus signal on pin 25
  dac2.outputVoltage(100);  // steady voltage level of ~1.3V on pin 26
}
...
```
For more usage infos have a look at the many [examples](https://github.com/yellobyte/DacESP32/blob/main/examples) included.

## :zap: Application notes

Put the DacESP32 library folder into your IDE standard library folder. You may have to restart the IDE. All examples were build & tested with ArduinoIDE V1.8.19 and VSCode/PlatformIO.


### :hammer_and_wrench: Modifiable definitions in DacESP32.cpp

All CW generator frequency calculations are done with the assumption of RTC8M_CLK (clock source that feeds both DACs controller section) to run near 8MHz. If your ESP32 exemplar is way off you might want to uncomment below line for adjustment/tuning. Default value is 172. Lowering the value will lower the frequency and vice versa. For more infos see section CW generator below.  
`//#define CK8M_DFREQ_ADJUSTED 172`
   
Below definition enables a more accurate setting of the CW generator output frequency and is uncommented by default. However, the digital controller clock (dig_clk_rtc_freq = RTC8M_CLK / 1 + RTC_CNTL_CK8M_DIV_SEL) of both the DAC and ADC modules in the ESP32 might get changed (due to altered value of RTC_CNTL_CK8M_DIV_SEL). Comment below line out if this causes problems or high frequency accuracy is not needed.  
`#define CW_FREQUENCY_HIGH_ACCURACY`  

Below value defines the CW generators minimum number of voltage steps per cycle. Too low values will reduce the maximal possible CW output frequency though. Lowering the value will increase the minimum number of voltage steps/cycle and vice versa. For more infos see section CW generator below.  
`#define SW_FSTEP_MAX  256`

## :information_source: The integrated cosine waveform (CW) generator 

There is only one CW generator in the ESP32. When enabled on both DAC channels than the signal frequency on both channels will always be equal ! Phase, amplitude and offset can be set independently for both channels though.

The frequency of the internal cosine waveform (CW) generator is easily set but somehow limited in range and stepsize. However, for simple requirements using the internal CW generator instead of external DAC hardware might save you time & costs.

According to ESP32 technical specs the CW frequency is calculated as follows:  
  - fcw = RTC8M_CLK / (1 + RTC_CNTL_CK8M_DIV_SEL) * (SENS_SW_FSTEP / 65536)  

RTC8M_CLK is an internal RC oscillator clock (belonging to the group of low-power clocks) with a default frequency of roughly 8 MHz (mostly slightly above according to spec). This frequency is adjustable/tunable by changing the value of register RTC_CNTL_CK8M_DFREQ (default 172).

As above formula tells you, changing the frequency is only possible in defined steps. With CK8M_DIV_SEL = 0 (default) a minimal frequency step is ~122 Hz, by setting CK8M_DIV_SEL to 7 (max) the stepsize gets greatly reduced to ~15.3 Hz. Function setCwFrequency() makes use of it if CW_FREQUENCY_HIGH_ACCURACY is defined.  
Be aware: Changing CK8M_DIV_SEL will change the digital controller clock (dig_clk_rtc_freq) of both the DAC and(!) ADC modules in the ESP32. If you use them simultaneously you have to take this into account.

To assure at least 256 points per cycle the value for SW_FSTEP should be limited to 256 which still results in a possible highest output frequency of ~32kHz.  
  - SW_FSTEP_MAX = 128  -->  voltage steps/cycle >= 512, fmax ~15.6kHz  
  - SW_FSTEP_MAX = 256  -->  voltage steps/cycle >= 256, fmax ~31.3kHz   
  - SW_FSTEP_MAX = 512  -->  voltage steps/cycle >= 128, fmax ~62.6kHz  

Choose a value that fits your application best.  

Actual measurements on a randomly picked ESP32 dev module showed notable deviations between calculated & generated frequency. With CK8M_DFREQ = 172 (default), CK8M_DIV_SEL = 0 and varying SW_FSTEP the results were:
  - SW_FSTEP = 1   -> calculated:   122.07 Hz, measured:   125.8 Hz
  - SW_FSTEP = 25  -> calculated:  3051.75 Hz, measured:  3145.0 Hz
  - SW_FSTEP = 256 -> calculated: 31250.00 Hz, measured: 32226.4 Hz  

Hence RTC8M_CLK seemed to run notably higher (3%) than the 8MHz as expected. However, defining CK8M_DFREQ_ADJUSTED = 161 for adjustment almost led to a complete match between calculated & measured frequencies.
	
## :file_folder: Documentation

Folder [**Doc**](https://github.com/yellobyte/DacESP32/blob/main/doc) contains a collection of files for further information:
  - Oscilloscope screenshots of output signals generated by examples
  - Tables of technically possible CW generator frequencies with different settings
  - platformio.ini I used for testing the lib in the VSCode/PlatformIO IDE

