/*
  outputVoltage.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch activates only the first DAC channel and generates a sawtooth waveform with a 
  very low frequency (~4Hz). 

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN0_GPIO_NUM);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Voltage level changes on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);
}

void loop() {
  uint16_t level;
  
  // outputs the full 8-bit scale from 0 to 255
  for (level = 0; level < 256; level++) {
    dac1.outputVoltage((uint8_t)level);
    delay(1);
  }
}
