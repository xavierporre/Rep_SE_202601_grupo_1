/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "identificador_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "esp_main.h"

// Ejecutar la inferencia cada N frames capturados. El analisis de borde
// (grilla 3x2) corre en TODOS los frames (~1 ms); la inferencia es lenta
// (cientos de ms). Subir a 2-3 si el borde reacciona lento en el ring.
#define INFERENCE_EVERY_N 1

namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;

// In order to use optimized tensorflow lite kernels, a signed int8_t quantized
// model is preferred over the legacy unsigned model format. This means that
// throughout this project, input images must be converted from unisgned to
// signed format. The easiest and quickest way to convert from unsigned to
// signed 8-bit integers is to subtract 128 from the unsigned value to get a
// signed value.

#if CONFIG_NN_OPTIMIZED
constexpr int scratchBufSize = 60 * 1024;
#else
constexpr int scratchBufSize = 0;
#endif
// An area of memory to use for input, output, and intermediate arrays.
// Keeping allocation on bit larger size to accomodate future needs.
constexpr int kTensorArenaSize = 320 * 1024 + scratchBufSize;
static uint8_t *tensor_arena;//[kTensorArenaSize]; // Maybe we should move this to external
}  // namespace

void setup() {
  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_identificador_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf("Model provided is schema version %d not equal to supported "
                "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  if (tensor_arena == NULL) {
    tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (tensor_arena == NULL) {
    printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize);
    return;
  }

  // AllOpsResolver registra todos los ops y versiones — evita incompatibilidades
  // entre la versión de TFLite usada para cuantizar y TFLite Micro de ESP-IDF.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::AllOpsResolver micro_op_resolver;

  // Build an interpreter to run the model with.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    MicroPrintf("AllocateTensors() failed");
    return;
  }

  // Get information about the memory area to use for the model's input.
  input = interpreter->input(0);

#ifndef CLI_ONLY_INFERENCE
  // Initialize Camera
  TfLiteStatus init_status = InitCamera();
  if (init_status != kTfLiteOk) {
    MicroPrintf("InitCamera failed\n");
    return;
  }

#if DISPLAY_SUPPORT
  create_gui();
#endif // DISPLAY_SUPPORT
#endif // CLI_ONLY_INFERENCE
}

#ifndef CLI_ONLY_INFERENCE
void loop() {
  if (input == nullptr) return;  // setup() falló — evita crash por puntero null

  // Get image from provider.
  if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input->data.int8)) {
    MicroPrintf("Image capture failed.");
  }

  // El analisis de borde corre sobre TODOS los frames (es barato);
  // se copia el frame antes de Invoke() porque la arena puede reutilizarse.
  SetLastImage(input->data.int8);

  // Scores de la ultima inferencia (persisten entre frames sin inferencia)
  static float s_person_score_f    = 0.0f;
  static float s_no_person_score_f = 1.0f;
  static int   s_frame_count       = 0;

  if ((s_frame_count++ % INFERENCE_EVERY_N) == 0) {
    int64_t t0 = esp_timer_get_time();

    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
      MicroPrintf("Invoke failed.");
    }

    TfLiteTensor* output = interpreter->output(0);

    // Process the inference results.
    int8_t person_score = output->data.int8[kPersonIndex];
    int8_t no_person_score = output->data.int8[kNotAPersonIndex];

    s_person_score_f =
        (person_score - output->params.zero_point) * output->params.scale;
    s_no_person_score_f =
        (no_person_score - output->params.zero_point) * output->params.scale;

    // Medir Invoke() en las primeras iteraciones para calibrar INFERENCE_EVERY_N
    if (s_frame_count <= 5 * INFERENCE_EVERY_N) {
      ESP_LOGI("robot_cam", "Invoke: %lld ms",
               (long long)((esp_timer_get_time() - t0) / 1000));
    }
  }

  // Respond to detection
  RespondToDetection(s_person_score_f, s_no_person_score_f);
  vTaskDelay(1); // to avoid watchdog trigger
}
#endif // CLI_ONLY_INFERENCE

#if defined(COLLECT_CPU_STATS)
  long long total_time = 0;
  long long start_time = 0;
  extern long long softmax_total_time;
  extern long long dc_total_time;
  extern long long conv_total_time;
  extern long long fc_total_time;
  extern long long pooling_total_time;
  extern long long add_total_time;
  extern long long mul_total_time;
#endif

void run_inference(void *ptr) {
  /* Convert from uint8 picture data to int8 */
  for (int i = 0; i < kNumCols * kNumRows; i++) {
    input->data.int8[i] = ((uint8_t *) ptr)[i] ^ 0x80;
  }

#if defined(COLLECT_CPU_STATS)
  long long start_time = esp_timer_get_time();
#endif
  // Run the model on this input and make sure it succeeds.
  if (kTfLiteOk != interpreter->Invoke()) {
    MicroPrintf("Invoke failed.");
  }

#if defined(COLLECT_CPU_STATS)
  long long total_time = (esp_timer_get_time() - start_time);
  printf("Total time = %lld\n", total_time / 1000);
#endif

  TfLiteTensor* output = interpreter->output(0);

  // Process the inference results.
  int8_t person_score = output->data.int8[kPersonIndex];
  int8_t no_person_score = output->data.int8[kNotAPersonIndex];

  float person_score_f =
      (person_score - output->params.zero_point) * output->params.scale;
  float no_person_score_f =
      (no_person_score - output->params.zero_point) * output->params.scale;
  RespondToDetection(person_score_f, no_person_score_f);
}

void run_inference_int8(const int8_t *ptr) {
  /* Data is already int8 (XOR 0x80 pre-applied), copy directly */
  for (int i = 0; i < kNumCols * kNumRows; i++) {
    input->data.int8[i] = ptr[i];
  }

#if defined(COLLECT_CPU_STATS)
  long long start_time = esp_timer_get_time();
#endif
  if (kTfLiteOk != interpreter->Invoke()) {
    MicroPrintf("Invoke failed.");
  }
#if defined(COLLECT_CPU_STATS)
  long long total_time = (esp_timer_get_time() - start_time);
  printf("Total time = %lld\n", total_time / 1000);
#endif

  TfLiteTensor* output = interpreter->output(0);
  int8_t person_score = output->data.int8[kPersonIndex];
  int8_t no_person_score = output->data.int8[kNotAPersonIndex];
  float person_score_f =
      (person_score - output->params.zero_point) * output->params.scale;
  float no_person_score_f =
      (no_person_score - output->params.zero_point) * output->params.scale;
  RespondToDetection(person_score_f, no_person_score_f);
}
