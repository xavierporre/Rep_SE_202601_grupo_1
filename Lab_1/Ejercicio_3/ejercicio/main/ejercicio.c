#include <stdio.h>
#include "esp_timer.h"
#include "xtensa/core-macros.h"
#include "esp_cpu.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

void app_main(void)
{
  volatile int var_1 = 233;
  volatile int var_2 = 128;
  volatile int X = 500 * 5000;
  volatile int64_t start_time, end_time;
  volatile uint32_t cycles_start, cycles_end;

  start_time = esp_timer_get_time();
  cycles_start = esp_cpu_get_cycle_count();
  for (int i = 0; i < X; i++) {
    volatile int result_0 = var_1 + var_2;
    volatile int result_1 = var_1 + 10;
    volatile int result_2 = var_1%var_2;
    volatile int result_3 = var_1*var_2;
    volatile int result_4 = var_1/var_2;
  }
  end_time = esp_timer_get_time();
  cycles_end = esp_cpu_get_cycle_count();
  // int instruction_count = 26 * X;
  volatile int alternative_instruction_count = X;
  volatile int64_t elapsed_time = end_time - start_time;
  volatile uint32_t elapsed_cycles = cycles_end - cycles_start;
  volatile float cpi = (float)elapsed_cycles / alternative_instruction_count;
  volatile uint32_t frequency = configCPU_CLOCK_HZ;
  volatile float time_using_cycles = (float)elapsed_cycles / (float)frequency * 1e6f;
  printf("Alternative instruction count: %d\n", alternative_instruction_count);
  printf("Elapsed time (us): %lld\n", elapsed_time);
  printf("Elapsed cycles: %lu\n", elapsed_cycles);
  printf("Time using cycles: %.4f\n", time_using_cycles);
  printf("CPI: %.4f\n", cpi);
}
