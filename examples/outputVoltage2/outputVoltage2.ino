/*
  outputVoltage2.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is assigned to GPIO25 (Pin 25) and DAC channel 2 is assigned to GPIO26 (Pin 26).

  This sketch activates only DAC channel 1 and repeatedly generates increasing voltage 
  levels from 0...3.3V (VDD) in steps of 0.15V. 

  Last updated 2025-01-05, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN_0);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Voltage level changes on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);
}

void loop() {
  float level;

  for (level = 0; level <= (float)3.3; level += (float)0.15) {
    dac1.outputVoltage(level);
    delay(1000);
  }
}
