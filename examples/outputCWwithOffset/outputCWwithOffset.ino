/*
  outputCWwithOffset.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is GPIO25 (Pin 25) and DAC channel 2 is GPIO26 (Pin 26).

  This sketch generates a ~1000Hz sinus signal on both channels using the 
  integrated cosine waveform (CW) generator. The signal on channel 2 has
  only 50% amplitude, a varying offset and shows clipping with big offset.

  Last updated 2022-06-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHANNEL_1), 
         dac2(DAC_CHANNEL_2);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. 1000Hz Sinus signals on GPIO (Pin) numbers: ");
  Serial.print(DAC_CHANNEL_1_GPIO_NUM);
  Serial.print(" and ");
  Serial.println(DAC_CHANNEL_2_GPIO_NUM);

  // outputs a sinus signals with frequency ~1000Hz and max. amplitude (default)
  dac1.outputCW(1000);
  // outputs a sinus signals with frequency ~1000Hz and half amplitude
  dac2.outputCW(1000, DAC_CW_SCALE_2);

  // wait 5 secs before changing offset on channel 2
  delay(5000);
}

void loop() {
  int16_t offset;

  // offset gets positiv, signal on channel 2 shifts slowly up
  for (offset = 0; offset <= 127; offset++) {
    dac2.setCwOffset(offset);
    delay(50);
  }  

  // offset gets negativ, signal on channel 2 shifts slowly down
  for (offset = 0; offset >= -128; offset--) {
    dac2.setCwOffset(offset);
    delay(50);
  }  
}