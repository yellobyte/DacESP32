# DacESP32

The ESP32 contains two 8-bit DAC (digital to analog converter) channels, connected to GPIO25 (DAC Channel 1) and GPIO26 (DAC Channel 2). Using the DACs allows these channels to be set to arbitrary output voltages between 0...+3.3V (VDD). Both channels can be driven alternatively by an integrated common cosine waveform (CW) generator. The CW generator is explained more detailed further down.

This Arduino library makes using the two ESP32 DAC output channels fast & easy.

Below an example for generating a sinus signal of ~2kHz on GPIO25 (Channel 1) and a steady voltage level of about 1.3V on GPIO26 (Channel 2).  

```c
...
#include "DacESP32.h"

DacESP32 dac1(DAC_CHANNEL_1),
         dac2(DAC_CHANNEL_2);

setup() {
  dac1.outputCW(2000);                // 2kHz sinus signal on pin 25
  dac2.outputVoltage((float)1.3);     // steady voltage level of 1.3V on pin 26
  // same as above but with 8-bit argument, range 0...255 (Vout = 100/255 * 3.3V)
  //dac2.outputVoltage((uint8_t)100); // steady voltage level of 1.3V on pin 26
}
...
```
For more usage infos have a look at the [**examples**](https://github.com/yellobyte/DacESP32/tree/main/examples) included.

## :zap: Application notes

To use this library, open the **Library Manager** in your **Arduino IDE**. Go to menu items **Sketch** -> **Include Library** -> **Manage Libraries**. Search for DacESP32 and click on **install**.  

In **VSCode/PlatformIO** click the **platformio sidebar icon**, open the **libraries** view and search for DacESP32 and click on **Add to Project**.

All examples were build & tested with ArduinoIDE V1.8.19 and VSCode/PlatformIO (Core 6.0.x/Home 3.4.x).  

**ATTENTION:**  
Older Espressif ESP32 framework versions (<=1.0.6 on ArduinoIDE resp. <=3.3.0 on PlatformIO) have some needed type definitions missing. So in case you get build errors like '_definition for dac_cw_scale_t missing_' or '_definition for dac_cw_phase_t missing_' please uncomment line '#define DACESP32_TYPE_DEFS' in DacESP32.h".

### :hammer_and_wrench: Modifiable definitions in DacESP32.cpp

All CW generator frequency calculations are done with the assumption of RTC8M_CLK (clock source that feeds both DACs controller section) to run near 8MHz. If your ESP32 exemplar is way off you might want to uncomment below line for adjustment/tuning. Default value is 172. Lowering the value will lower the frequency and vice versa. For more infos see section CW generator below.  
`//#define CK8M_DFREQ_ADJUSTED 172`
   
Below definition enables a more accurate setting of the CW generator output frequency and is uncommented by default. However, the digital controller clock (dig_clk_rtc_freq = RTC8M_CLK / 1 + RTC_CNTL_CK8M_DIV_SEL) of both the DAC and ADC modules in the ESP32 might get changed (due to altered value of RTC_CNTL_CK8M_DIV_SEL). Comment below line out if this causes problems or high frequency accuracy is not needed.  
`#define CW_FREQUENCY_HIGH_ACCURACY`  

Below value defines the CW generators minimum number of voltage steps per cycle. Too low values will reduce the maximal possible CW output frequency though. Lowering the value will increase the minimum number of voltage steps/cycle and vice versa. For more infos see section CW generator below.  
`#define SW_FSTEP_MAX  256`

The maximum possible DAC output voltage depends on the actual supply voltage (VDD) of your ESP32. It will vary with the used LDO voltage regulator on your board and other factors. To generate a more precise output voltage: Generate max voltage level on a DAC channel with outputVoltage(255) and measure the real voltage on it (with only light or no load!). Then replace below value with the measured one.  
`#define CHANNEL_VOLTAGE_MAX (float) 3.30`  

## :information_source: The integrated cosine waveform (CW) generator 

There is only one CW generator in the ESP32. When enabled on both DAC channels than the signal frequency on both channels will always be equal ! Phase, amplitude and offset can be set independently for both channels though.

The frequency of the internal cosine waveform (CW) generator is easily set but somehow limited in range and stepsize. However, for simple requirements using the internal CW generator instead of external DAC hardware might save you time & costs.

According to ESP32 technical specs the CW frequency fcw is calculated as follows:  
  - **fcw = RTC8M_CLK / (1 + RTC_CNTL_CK8M_DIV_SEL) * (SENS_SW_FSTEP / 65536)**  

As above formula tells you, changing the frequency is only possible in defined steps.  
With CK8M_DIV_SEL = 0 (default) a minimal frequency step is ~122 Hz, by setting CK8M_DIV_SEL to 7 (max) the stepsize gets greatly reduced to ~15.3 Hz. Function setCwFrequency() makes use of it if CW_FREQUENCY_HIGH_ACCURACY is defined.  
Be aware: Changing CK8M_DIV_SEL will change the digital controller clock (dig_clk_rtc_freq) of both the DAC and(!) ADC modules in the ESP32. If you use them simultaneously you have to take this into account.

RTC8M_CLK in above formula is an internal RC oscillator clock (belonging to the group of low-power clocks) with a default frequency of roughly 8 MHz or slightly above (the various ESP32 specs are a bit fuzzy on this subject). However, this frequency is adjustable/tunable by changing the value of ESP32 register RTC_CNTL_CK8M_DFREQ (default value 172).  

Actual measurements on a randomly picked ESP32 dev module showed notable deviations between calculated & generated frequency. With CK8M_DFREQ = 172 (default), CK8M_DIV_SEL = 0 and varying SW_FSTEP the results were:
  - SW_FSTEP = 1   -> calculated:   122.07 Hz, measured:   125.8 Hz
  - SW_FSTEP = 25  -> calculated:  3051.75 Hz, measured:  3145.0 Hz
  - SW_FSTEP = 256 -> calculated: 31250.00 Hz, measured: 32226.4 Hz  

Hence RTC8M_CLK seemed to run notably higher (**3%**) than the 8MHz expected. However, defining CK8M_DFREQ_ADJUSTED = 161 for adjustment almost led to a complete match between calculated & measured frequencies.

To assure at least 256 points per cycle the value for SW_FSTEP should be limited to 256 which still results in a possible highest output frequency of ~32kHz.  
  - **SW_FSTEP_MAX = 128**  -->  voltage steps/cycle >= **512**, fmax **~15.6kHz**  
  - **SW_FSTEP_MAX = 256**  -->  voltage steps/cycle >= **256**, fmax **~31.3kHz**   
  - **SW_FSTEP_MAX = 512**  -->  voltage steps/cycle >= **128**, fmax **~62.6kHz**  

Choose a value that fits your application best.  
	
## :file_folder: Documentation

Folder [**Doc**](https://github.com/yellobyte/DacESP32/tree/main/doc) contains a collection of files for further information:
  - Oscilloscope screenshots of output signals generated by examples
  - Tables of technically possible CW generator frequencies with different settings
  - platformio.ini I used for testing the lib in the VSCode/PlatformIO IDE

## :relaxed: Postscript

If you run into trouble with this lib or have suggestions how to improve it, feel free to contact me.  

And a last remark: Putting this library together took quite long hours and a lot of coffee. So if you like it, please give it a star. Thanks !