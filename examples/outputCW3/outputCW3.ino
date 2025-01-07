/*
  outputCW3.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is assigned to  GPIO25 (Pin 25) and DAC channel 2 is assigned to GPIO26 (Pin 26).

  This sketch generates a 1kHz sinus signal on DAC channel 1 (Pin 25) using the integrated cosine waveform (CW) generator.
  The sinus signal gets switch off/on every 2 seconds.

  Last updated 2025-01-07, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(GPIO_NUM_25);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Sinus signal output on GPIO (Pin) number: ");
  Serial.println(GPIO_NUM_25);
  
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
