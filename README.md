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
  //dac2.outputVoltage((uint8_t)100); // same as above but with 8-bit argument, range 0...255
                                      // (Vout = 100/255 * 3.3V = ~1.3V)
}
...
```
For more usage infos have a look at the [**examples**](https://github.com/yellobyte/DacESP32/tree/main/examples) included.

## :zap: Application notes

In **Arduino IDE** open the **Library Manager** and go to menu items **Sketch** -> **Include Library** -> **Manage Libraries**. Search for DacESP32 and click on **install**.  

In **VSCode/PlatformIO** click the **platformio sidebar icon**, open the **libraries** view, search for DacESP32 and click on **Add to Project**.

All examples were build & tested with ArduinoIDE V2.3.4/ESP32 Core V3.0.7 and VSCode/PlatformIO[PIOArduino] (Core 6.1.16 - Home 3.4.4) with ESP32 Core V3.1.0.  

As of library **version 2.0.0** the Arduino ESP32 Core version **3.0.0** or higher is required for a successful build. The library has been redesigned and now uses *driver/dac_oneshot.h* and *driver/dac_cosine.h* instead of the deprecated *driver/dac.h*. 

As of library **version 1.1.0** certain library parameters can be altered with **build options**. There is no need to edit parameter definitions in DacESP32.cpp anymore. Build options can be customized as follows:

#### Building with Arduino IDE:

Add a file named "build_opt.h" containing build options to your sketch directory, e.g.:  
```c
-DSW_FSTEP_MAX=512
-DCK8M_DFREQ_ADJUSTED=169
```
Please note: Changes made to "build_opt.h" after a first build will not be detected by the Arduino IDE. Forcing the IDE to rebuild the whole project, deleting DacESP32.cpp.o in the temporary build directory or simply restarting the IDE will fix that.  

#### Building with VSCode/PlatformIO:

Add build options to your projects _platformio.ini_ file, e.g.:  
```c
build_flags = 
  -DCK8M_DFREQ_ADJUSTED=175
  -DCHANNEL_VOLTAGE_MAX=3.35
```
### :bangbang: **ATTENTION:**  
Older Espressif ESP32 framework versions have some needed type definitions missing and compiling this library **up to version 1.1.1** produces errors like **'_definition for dac_cw_scale_t missing_'** or **'_definition for dac_cw_phase_t missing_'**. In this case please add build option `DACESP32_TYPE_DEFS` to your config file as explained above.

### :hammer_and_wrench: Available library build options

Actually **none** of below build options are required for successfully building the library. If you just want to play around with the ESP32 DAC channels, generate some sine waves or set voltage levels with moderate accuracy, don't worry about them.  

However, if you want to generate sinus signals with a frequency higher than ~31.5kHz or with higher frequency accuracy, more or less than 256 voltage steps per cycle or voltage levels with higher voltage accuracy then below build options will be helpful.

`CK8M_DFREQ_ADJUSTED=value`: The CW generator can be **adjusted/tuned** to achieve a higher frequency accuracy. Just add this build option to your config file. Values higher than 172 will tune the CW generator upwards. Values lower than 172 will tune the CW generator downwards. Only integer values are allowed. Example: A value of 173 will increase the CW frequency only slightly, a value of 180 will increase it notably.  

`CW_FREQUENCY_HIGH_ACCURACY=0`: Setting the CW generator **output frequency** can only be done in steps as explained with a formula further down. However, the library tries to keep the steps as small as technically possible (~15Hz at minimum). By doing so the digital controller clock of both the DAC and (!) ADC sections in the ESP32 might get changed. If this causes problems with the ADC section in the ESP32 then use this build option. Negative side effect: the resulting step size will become a constant ~122Hz.

`SW_FSTEP_MAX=value`: This build option alters the **minimum number of voltage steps per cycle** and the **maximal possible CW output frequency**. Without this build option the library defaults to **SW_FSTEP_MAX=256**. Hence using this build option with value 256 would be pointless. Accepted values are:
- **SW_FSTEP_MAX=64**   --->  voltage steps per cycle **>=1024**, fmax **~7.8kHz**  
- **SW_FSTEP_MAX=128**  --->  voltage steps per cycle **>=512**,  fmax **~15.6kHz**
- **SW_FSTEP_MAX=256**  --->  voltage steps per cycle **>=256**,  fmax **~31.2kHz**  (default)  
- **SW_FSTEP_MAX=512**  --->  voltage steps per cycle **>=128**,  fmax **~62.6kHz**  
- **SW_FSTEP_MAX=1024**  --->  voltage steps per cycle **>=64**,  fmax **~125.2kHz**  

Depending on your projects requirements always try to keep SW_FSTEP_MAX as low as possible to get max voltage steps/cycle and with it a cleaner signal.  

IMPORTANT: Above definition gets applied only without build option 'CW_FREQUENCY_HIGH_ACCURACY=0'.

`CHANNEL_VOLTAGE_MAX=Vreal`: The maximal possible DAC output voltage of your ESP32 depends on the supply voltage (VDD), the chip itself and the actual load on your DAC channels. It will slightly vary with every dev board you try and is usually between ~3.2...~3.4V. If it's way off then your ESP32 chip or the surrounding circuitry might be faulty !  

To generate a more precise output voltage you can do this: Generate max voltage level on the DAC channel used, e.g. with _outputVoltage(255)_ and measure the real output voltage, preferably with the designated load. Then add this build option holding the measured value to your config file, e.g.: CHANNEL_VOLTAGE_MAX=3.28.  

## :information_source: The integrated cosine waveform (CW) generator 

There is only one cosine waveform CW generator in the ESP32. When enabled on both DAC channels then the signal frequency on both channels will always be equal ! Phase, amplitude and offset can be set independently for both channels though.

The frequency of the CW generator is easily set but somewhat limited in range and stepsize. But for simple requirements using the internal CW generator to create a sine wave instead of external DAC hardware might save you time & costs.

According to ESP32 technical specs the CW frequency **fcw** is calculated as follows:  
  - **fcw = (RTC8M_CLK / (1 + RTC_CNTL_CK8M_DIV_SEL)) * (SENS_SAR_SW_FSTEP / 65536)**  

As the formula shows, changing the frequency is only possible in defined steps. With RTC8M_CLK at ~8MHz and RTC_CNTL_CK8M_DIV_SEL=0 (default) the stepsize is ~122 Hz. With RTC_CNTL_CK8M_DIV_SEL=7 (max) the stepsize is greatly reduced to ~15.3 Hz. The library by default sets the output frequency by altering both variables SENS_SAR_SW_FSTEP and RTC_CNTL_CK8M_DIV_SEL. Various combinations of these two variables are tried in order to hit the target frequency as close as possible.

RTC8M_CLK in above formula is an internal RC oscillator clock (belonging to the group of low-power clocks) with a default frequency of roughly 8 MHz or slightly above (the various ESP32 specs are a bit fuzzy on this subject). The measured output frequency on a randomly picked ESP32 dev module showed a notable deviation (production tolerance, temperature ?) from the calculated frequency:  
  - Calculated:   122.07 Hz, measured:   125.8 Hz
  - Calculated:  3051.75 Hz, measured:  3145.0 Hz
  - Calculated: 31250.00 Hz, measured: 32226.4 Hz  

Hence RTC8M_CLK seemed to be approximately **3%** higher than the 8MHz expected. Tuning the CW generator downwards with build option `CK8M_DFREQ_ADJUSTED=161` led to an almost perfect match between calculated & measured frequencies.

**Please note:**  
The digital controller clock of both the DAC **and** ADC sections in the ESP32 is calculated as follows: dig_clk_rtc_freq = RTC8M_CLK / 1 + RTC_CNTL_CK8M_DIV_SEL. Which is actually the first term of the formula above. If you therefore prefer dig_clk_rtc_freq to stay untouched then use build option `CW_FREQUENCY_HIGH_ACCURACY=0`.  

## :file_folder: Documentation

Folder [**Doc**](https://github.com/yellobyte/DacESP32/tree/main/doc) contains a collection of files for further information:
  - Oscilloscope screenshots of output signals generated by examples
  - Tables of technically possible CW generator frequencies with different settings
  - platformio.ini I used for testing the lib in the VSCode/PlatformIO IDE

## :relaxed: Postscript

If you run into trouble with this lib or have suggestions how to improve it, feel free to contact me or create an issue.  
