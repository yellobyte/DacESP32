/*
  outputCWandVoltage.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is GPIO25 (Pin 25) and DAC channel 2 is GPIO26 (Pin 26).

  This sketch generates a ~200Hz sinus wave on DAC channel 1 using the integrated
  cosine waveform (CW) generator and a ~70Hz triangle waveform on DAC channel 2
  setting the DAC output level discretely with function outputVoltage().

  Last updated 2022-09-04, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHANNEL_1),
         dac2(DAC_CHANNEL_2);

void setup() {
  gpio_num_t pinCh1, pinCh2;

  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started.");
  Serial.print("Sinus signal on GPIO (Pin) number: ");
  dac1.getGPIOnum(&pinCh1);
  Serial.println(pinCh1);
  Serial.print("Triangle signal on GPIO (Pin) number: ");
  dac2.getGPIOnum(&pinCh2);
  Serial.println(pinCh2);

  // outputs a ~200Hz sinus signal on DAC channel 1 (GPIO pin 25)
  dac1.outputCW(200);
}

void loop() {
  int16_t level;
  
  // generates a triangle signal on DAC channel 2 (GPIO pin 26) using
  // the full 8-bit scale from 0 to 255
  for (level = 0; level < 256; level++) {
    dac2.outputVoltage((uint8_t)level);
  }
  for (level = 255; level >= 0; level--) {
    dac2.outputVoltage((uint8_t)level);
  }
}
