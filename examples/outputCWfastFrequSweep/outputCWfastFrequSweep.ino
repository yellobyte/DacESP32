/*
  outputCWfastFrequSweep.ino

  The ESP32 contains two 8-bit DAC output channels.
  DAC channel 1 is assigned to  GPIO25 (Pin 25) and DAC channel 2 is assigned to GPIO26 (Pin 26).

  This sketch generates a sinus signal on DAC channel 1 (Pin 25) using the integrated cosine waveform 
  (CW) generator. The output signal repeatedly sweeps fast from lower to higher frequencies. 

  Last updated 2025-04-20, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "DacESP32.h"

#define FREQU_START 500           // Hz
#define FREQU_STOP  5000
#define FREQU_STEP  250

#define CHANGE_DELAY 200          // ms

DacESP32 dac1(GPIO_NUM_25);
int frequ = FREQU_START;

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Sketch started. Sinus signal output on GPIO (Pin) number: ");
  Serial.println(GPIO_NUM_25);

  dac1.outputCW(frequ);           // register and activate the DAC output channel
  delay(CHANGE_DELAY);
}

void loop() {
  // calculate and set the new frequency of the sinus output signal
  frequ += FREQU_STEP;
  if (frequ > FREQU_STOP) frequ = FREQU_START;
  if (dac1.setCwFrequency(frequ) != ESP_OK)
    Serial.printf("Error: setCwFrequency(%d)\n", frequ);
  
  delay(CHANGE_DELAY);
}
