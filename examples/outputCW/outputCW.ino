/*
  outputCW.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a ~1000Hz sinus signal with changing amplitude on the first
  DAC channel using the integrated cosine waveform (CW) generator.

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

DacESP32 dac1(DAC_CHAN0_GPIO_NUM);

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. ~1000Hz Sinus signal on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);

  // output a sinus signal with frequency ~1000Hz and max. amplitude (default)
  dac1.outputCW(1000);
  // wait 5 secs before changing amplitude
  delay(5000);
}

void loop() {
  // change signal amplitude every second
  for (uint8_t i = 0; i < 4; i++) {
    delay(1000);
    if (i == 0)      
      dac1.setCwScale(DAC_COSINE_ATTEN_DB_0);   // max. amplitude
    else if (i == 1) 
      dac1.setCwScale(DAC_COSINE_ATTEN_DB_6);   // 1/2 amplitude (-6dB)
    else if (i == 2)
      dac1.setCwScale(DAC_COSINE_ATTEN_DB_12);  // 1/4 amplitude (-12dB)
    else if (i == 3)
      dac1.setCwScale(DAC_COSINE_ATTEN_DB_18);  // 1/8 amplitude (-18dB)
  }
}