/*
  outputAnalogDigitalMixed.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a ~500Hz sinus signal on the first DAC channel 
  followed by a ~500Hz digital output signal, 5 seconds each & alternatingly. 

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

uint32_t counter, 
         mode = 1; // analog output starts

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Voltage level changes on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);
}

void loop() {
  if (mode++ % 2) {
    // ~500Hz sinus signal output for 5 secs
    DacESP32 dac1(DAC_CHAN0_GPIO_NUM);
    dac1.outputCW(500);
    delay(5000);
  }
  else {
    // ~500Hz digital output for 5 secs
    pinMode(DAC_CHAN0_GPIO_NUM, OUTPUT);
    counter = 0;
    do {
      digitalWrite(DAC_CHAN0_GPIO_NUM,!digitalRead(DAC_CHAN0_GPIO_NUM));
      delay(1);
    } while (counter++ < 5000);
  }
}
