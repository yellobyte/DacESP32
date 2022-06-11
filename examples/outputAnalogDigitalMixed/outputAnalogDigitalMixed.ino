/*
  outputAnalogDigitalMixed.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is GPIO25 (Pin 25) and DAC channel 2 is GPIO26 (Pin 26).

  This sketch generates a ~500Hz sinus signal on DAC channel 1 (GPIO pin 25)
  followed by a ~500Hz digital output signal, 5 seconds each & alternatingly. 

  Last updated 2022-06-11, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

uint32_t counter, 
         mode = 1; // analog output starts

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Voltage level changes on GPIO (Pin) number: ");
  Serial.println(GPIO_NUM_25);
}

void loop() {
  if (mode++ % 2) {
    // ~500Hz sinus signal output for 5 secs
    DacESP32 dac1(GPIO_NUM_25);
    dac1.outputCW(500);
    delay(5000);
  }
  else {
    // ~500Hz digital output for 5 secs
    pinMode(GPIO_NUM_25, OUTPUT);
    counter = 0;
    do {
      digitalWrite(GPIO_NUM_25,!digitalRead(GPIO_NUM_25));
      delay(1);
    } while (counter++ < 5000);
  }
}
