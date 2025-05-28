/*
  outputVoltage2.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch activates only the first DAC channel and repeatedly generates increasing voltage 
  levels from 0...3.3V (VDD) in steps of 0.15V. 

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
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
