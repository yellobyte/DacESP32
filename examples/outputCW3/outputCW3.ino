/*
  outputCW3.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a 1kHz sinus signal on the first DAC channel using the integrated cosine waveform (CW) generator.
  The sinus signal gets switch off/on every 2 seconds.

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN0_GPIO_NUM);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Sinus signal output on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);
  
  dac1.outputCW(1000);
}

void loop() {
  // disable the DAC channel after 2s
  delay(2000);
  dac1.disable();
  // enable the DAC channel again after 2s
  delay(2000);
  dac1.enable();
}
