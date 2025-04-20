/*
  outputCWfastFrequSweep2.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is assigned to  GPIO25 (Pin 25) and DAC channel 2 is assigned to GPIO26 (Pin 26).

  This sketch generates a sinus signal on DAC channel 1 (GPIO25) using the integrated cosine waveform 
  (CW) generator. The output signal repeatedly sweeps fast from lower to higher frequencies. 
  A sawtooth shaped signal with constant frequency is output on DAC channel 2 (GPIO26).

  IMPORTANT: Needs build option CW_FREQUENCY_HIGH_ACCURACY=0 for the loop time to stay constant
             and therefore getting a stable sawtooth signal.

  Last updated 2025-04-20, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

#define FREQU_START 500           // Hz
#define FREQU_STOP  5000
#define FREQU_STEP  500

DacESP32 dac1(DAC_CHAN_0),
         dac2(DAC_CHAN_1);
int frequ = FREQU_START;

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Sinus signal on GPIO25 and sawtooth signal on GPIO26.");

  dac1.outputCW(frequ);           // register and activate the DAC output channel
}

void loop() {
  int16_t level;

  // calculate and set the new frequency of the sinus output signal
  frequ += FREQU_STEP;
  if (frequ > FREQU_STOP) frequ = FREQU_START;
  if (dac1.setCwFrequency(frequ) != ESP_OK)
    Serial.printf("Error: setCwFrequency(%d)\n", frequ);

  // generate a sawtooth signal on GPIO26
  for (level = 0; level < 256; level++) {
    dac2.outputVoltage((uint8_t)level);
    delayMicroseconds(12);
  }
}
