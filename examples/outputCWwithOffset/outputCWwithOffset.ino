/*
  outputCWwithOffset.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a ~1000Hz sinus signal on both channels using the integrated cosine 
  waveform (CW) generator. The signal on the second channel has only 50% amplitude, a varying offset 
  and shows clipping with increasing offset.

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN_0), 
         dac2(DAC_CHAN_1);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. 1000Hz Sinus signals on GPIO (Pin) numbers: ");
  Serial.print(DAC_CHAN0_GPIO_NUM);
  Serial.print(" and ");
  Serial.println(DAC_CHAN1_GPIO_NUM);

  // outputs a sinus signal with frequency ~1000Hz and max. amplitude (default)
  dac1.outputCW(1000);
  // outputs a sinus signal with frequency ~1000Hz and half amplitude (-6dB)
  dac2.outputCW(1000, DAC_COSINE_ATTEN_DB_6);

  // wait 5 secs before changing offset on second channel
  delay(5000);
}

void loop() {
  int16_t offset;

  // offset gets positiv, signal on second channel shifts slowly up
  for (offset = 0; offset <= 127; offset++) {
    dac2.setCwOffset(offset);
    delay(50);
  }  

  // offset gets negativ, signal on second channel shifts slowly down
  for (offset = 0; offset >= -128; offset--) {
    dac2.setCwOffset(offset);
    delay(50);
  }  
}