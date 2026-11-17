#include "Utility.h"

unsigned long Utility::Elapsed = 0;

void Utility::MarkTime() {
  Elapsed = millis();
}

void Utility::ShowTime() {
  Elapsed = millis() - Elapsed;
  Serial.print("Job time (ms) :");
  Serial.println(Elapsed);
}