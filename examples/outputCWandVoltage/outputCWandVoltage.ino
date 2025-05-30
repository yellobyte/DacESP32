/*
  outputCWandVoltage.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a ~200Hz sinus wave on the first DAC channel using the integrated
  cosine waveform (CW) generator and a ~70Hz triangle waveform on the second DAC channel
  setting the DAC output level discretely with function outputVoltage().

  Last updated 2025-05-31, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN_0),
         dac2(DAC_CHAN_1);

void setup() {
  gpio_num_t pinCh1, pinCh2;

  Serial.begin(115200);

  Serial.println();
  Serial.println("Sketch started.");
  Serial.print("Sinus signal on GPIO (Pin) number: ");
  dac1.getGPIOnum(&pinCh1);
  Serial.println(pinCh1);
  Serial.print("Triangle signal on GPIO (Pin) number: ");
  dac2.getGPIOnum(&pinCh2);
  Serial.println(pinCh2);

  // outputs a ~200Hz sinus signal on first DAC channel
  dac1.outputCW(200);
}

void loop() {
  int16_t level;
  
  // generates a triangle signal on second DAC channel using the full 8-bit scale from 0 to 255
  for (level = 0; level < 256; level++) {
    dac2.outputVoltage((uint8_t)level);
  }
  for (level = 255; level >= 0; level--) {
    dac2.outputVoltage((uint8_t)level);
  }
}
