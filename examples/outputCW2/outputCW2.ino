/*
  outputCW2.ino

  The ESP32 contains two 8-bit DAC output channels.
  The first DAC channel is assigned to GPIO25 and the second one to GPIO26.

  The ESP32-S2 contains two 8-bit DAC output channels as well.
  Here the first DAC channel is assigned to GPIO17 and the second one to GPIO18.

  This sketch generates a sinus signal on the first DAC channel using the integrated cosine waveform (CW) generator.
  The signal frequency increases over time. The DAC channel is disabled for a short pause after each frequency change.

  The DAC channel gets enabled automatically with each call to outputCW().

  Last updated 2025-05-28, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

#define FREQU_START 500           // Hz
#define FREQU_STOP  5000
#define FREQU_STEP  500

#define SIGNAL_ON_MS  5000        // ms
#define SIGNAL_OFF_MS 2000

DacESP32 dac1(DAC_CHAN0_GPIO_NUM);
int frequ = FREQU_START;

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Sinus signal output on GPIO (Pin) number: ");
  Serial.println(DAC_CHAN0_GPIO_NUM);
}

void loop() {
  // output a sinus signal with max. amplitude (default) for SIGNAL_ON_MS milliseconds
  dac1.outputCW(frequ);
  delay(SIGNAL_ON_MS);

  // disable the DAC channel for SIGNAL_OFF_MS milliseconds
  dac1.disable();
  delay(SIGNAL_OFF_MS);

  // increase frequency by FREQU_STEP up to FREQU_STOP and repeat cycle
  frequ += FREQU_STEP;
  if (frequ > FREQU_STOP) frequ = FREQU_START;
}
