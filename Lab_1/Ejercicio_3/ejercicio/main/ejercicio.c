#include <stdio.h>
#include "esp_timer.h"
#include "xtensa/core-macros.h"
#include "esp_cpu.h"

void app_main(void)
{
  int var_1 = 233;
  int var_2 = 128;
  int X = 500;
  int64_t start_time, end_time;
  uint32_t cycles_start, cycles_end;

  start_time = esp_timer_get_time();
  cycles_start = esp_cpu_get_cycle_count();
  for (int i = 0; i==X; i++) {
    int result_0 = var_1 + var_2;
    int result_1 = var_1 + 10;
    int result_2 = var_1%var_2;
    int result_3 = var_1*var_2;
    int result_4 = var_1/var_2;
  }
  end_time = esp_timer_get_time();
  cycles_end = esp_cpu_get_cycle_count();
  int64_t elapsed_time = end_time - start_time;
  uint32_t elapsed_cycles = cycles_end - cycles_start;

}
