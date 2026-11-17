// Utility.h
#ifndef UTILITY_H
#define UTILITY_H

class Utility {
 public:
  static void MarkTime();
  static void ShowTime();

 private:
  static unsigned long Elapsed;
};

#endif  // UTILITY_H