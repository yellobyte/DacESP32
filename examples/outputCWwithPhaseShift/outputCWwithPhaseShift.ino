/*
  outputCWwithPhaseShift.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is GPIO25 (Pin 25) and DAC channel 2 is GPIO26 (Pin 26).

  This sketch generates a ~1000Hz sinus signal on both channels using the integrated
  cosine waveform (CW) generator. The two signals have a 180° phase shift in between.

  Last updated 2022-06-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHANNEL_1),
         dac2(DAC_CHANNEL_2);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Generating ~1000Hz Sinus signal on GPIO (Pin) numbers: ");
  Serial.println(DAC_CHANNEL_1_GPIO_NUM);
  Serial.print(" and ");
  Serial.println(DAC_CHANNEL_2_GPIO_NUM);

  // outputs a sinus signal with frequency ~1000Hz and max. amplitude (default)
  dac1.outputCW(1000);
  // outputs a sinus signal with frequency ~1000Hz, max. amplitude (default) and 180° phaseshift
  dac2.outputCW(1000, DAC_CW_SCALE_1, DAC_CW_PHASE_180);
}

void loop() {
  // 
}