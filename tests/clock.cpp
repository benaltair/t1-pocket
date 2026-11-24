#include <cassert>
#include "T1CompanionClock.h"

uint32_t testTicks = UINT32_MAX - 511;

int main() {
  T1CompanionClock c;
  const uint32_t today = 1795470000;
  assert(!c.isSynced());
  c.setCurrentTime(4200000000UL);  // a date in 2103
  assert(!c.isSynced());
  c.setCurrentTime(today);
  assert(c.isSynced());
  testTicks += 512;
  c.tick();
  assert(c.getCurrentTime() == today);
  testTicks += 512;
  c.tick();
  assert(c.getCurrentTime() == today + 1);
  testTicks += 70 * configTICK_RATE_HZ;
  c.tick();
  assert(c.getCurrentTime() == today + 71);
  c.setCurrentTime(2556143999UL);
  assert(c.getCurrentTimeUnique() == 2556143999UL);
  c.setCurrentTime(today);
  assert(c.getCurrentTimeUnique() == today);
  assert(c.getCurrentTimeUnique() == today + 1);
  c.setCurrentTime(2556144000UL);
  c.setCurrentTime(0);
  assert(c.getCurrentTime() == today);
  T1CompanionClock restart;
  assert(!restart.isSynced());
}
