#pragma once
#include <stdint.h>
using TickType_t = uint32_t;
constexpr uint32_t configTICK_RATE_HZ = 1024;
extern uint32_t testTicks;
inline uint32_t xTaskGetTickCount() { return testTicks; }
