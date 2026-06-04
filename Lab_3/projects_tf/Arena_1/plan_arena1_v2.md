# Plan: Arena_1 cam_arena — Optimización memoria + grilla 3×2 (v2)

## TL;DR
Hay dos problemas principales: (1) degradación de rendimiento por mal manejo del heap del intérprete y logs excesivos por frame; (2) análisis de borde limitado a 1 banda horizontal sin info de proximidad. Se aplican fixes de memoria/estabilidad, se reemplaza el análisis por una grilla 3×2 con histeresis, umbral combinado y campo `action` en el JSON, y se agrega timeout de seguridad en el servidor de control.

---

## Contexto descubierto

**Archivos clave:**
- `cam_arena/main/main_functions.cc` — TFLite setup + loop; `tensor_arena` como puntero heap SPIRAM
- `cam_arena/main/image_provider.cc` — `GetImage()`: captura cam, procesa, retorna fb; `MicroPrintf` cada frame
- `cam_arena/main/detection_responder.cc` — `analyze_edge()`: solo filas 80-95, 3 columnas
- `cam_arena/main/stream_server.h/.cc` — struct `detection_result_t` + JSON `/detection`
- `esp32_server/main/web.cpp` — `autoTask` parsea `"edge"`, buffer `s_auto_resp[128]`

**Problemas identificados:**
1. `tensor_arena` es puntero `heap_caps_malloc` (100 KB SPIRAM) — frágil si `setup()` se llama más de 1 vez
2. `MicroPrintf("Image Captured\n")` se llama en CADA frame (~30/s) — desperdicio de CPU/logs
3. `ESP_LOGI("[BORDE] %s")` cada frame también — exceso de logs que consumen ciclos de UART
4. Sin `setup_done` guard → si `setup()` re-entrara, re-invocaría `AllocateTensors()` con doble alloc
5. `analyze_edge()` usa solo las últimas 16 filas, sin info de distancia ni histeresis
6. `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` está en `# not set` → necesita activarse para usar `EXT_RAM_BSS_ATTR`
7. `s_auto_resp[128]` en web.cpp quedará pequeño con JSON ampliado
8. `json[96]` en stream_server.cc quedará pequeño con JSON ampliado
9. Sin histeresis en detección → robot oscila ante ruido de imagen o sombras
10. `global_max` como única referencia de umbral es frágil si toda la imagen es borde
11. Sin timeout de seguridad en `autoTask` → si la cámara deja de responder, el robot continúa con el último comando indefinidamente

---

## Fases

### Fase 1 — Fixes de memoria y estabilidad (cam_arena)

**Paso 1 — sdkconfig: habilitar BSS externo**
- `cam_arena/sdkconfig`: cambiar `# CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY is not set` → `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`
- También en `cam_arena/sdkconfig.defaults`: agregar la línea

**Paso 2 — main_functions.cc: tensor_arena estático + guard + heap monitor**
- Reemplazar `static uint8_t *tensor_arena;` por:
  ```c
  static uint8_t tensor_arena[kTensorArenaSize] EXT_RAM_BSS_ATTR;
  ```
- Eliminar el bloque `if (tensor_arena == NULL) { heap_caps_malloc(...) }` y el check siguiente
- Agregar guard al inicio de `setup()`:
  ```c
  static bool s_setup_done = false;
  if (s_setup_done) return;
  s_setup_done = true;
  ```
- Agregar `#include "esp_attr.h"` si no está (normalmente ya incluido vía `esp_heap_caps.h`)
- Agregar monitor de heap en `loop()` **por tiempo real**, no por contador de iteraciones:
  ```c
  static uint32_t last_heap_log_ms = 0;
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if (now_ms - last_heap_log_ms >= 5000) {
      ESP_LOGI("HEAP", "libre: %d | SPIRAM libre: %d",
               (int)esp_get_free_heap_size(),
               (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      last_heap_log_ms = now_ms;
  }
  ```
  > **Motivo:** el contador de iteraciones (cada 500 frames) puede equivaler a ~16 s a 30 fps — demasiado tarde para detectar un leak. El timer de 5 s es independiente de la velocidad del loop.

**Paso 3 — image_provider.cc: eliminar log por frame + garantizar fb_return**
- Eliminar `MicroPrintf("Image Captured\n")` (línea en path non-DISPLAY_SUPPORT)
- Reorganizar el path non-DISPLAY_SUPPORT con cleanup explícito para garantizar `esp_camera_fb_return(fb)` en **todo** path de error:
  ```c
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return kTfLiteError;

  // ... procesamiento ...

  cleanup:
    esp_camera_fb_return(fb);
    fb = NULL;
  return status;
  ```

---

### Fase 2 — Grilla 3×2 con histeresis y umbral combinado (cam_arena)

**Paso 4 — stream_server.h: ampliar detection_result_t**
- Nuevo struct:
  ```c
  typedef struct {
      char   edge[8];      // legacy: "none"|"left"|"right"|"center"
      char   action[16];   // "forward"|"turn_left"|"turn_right"|"stop"
      int8_t dist[3];      // 0=sin borde, 1=lejos, 2=cerca [izq,centro,der]
      int8_t grid[2][3];   // promedio por celda [fila:0=top,1=bot][col:0=L,1=C,2=R]
      bool   identifier;
      int    confidence;
  } detection_result_t;
  ```
- Ajustar inicializador en `stream_server.cc`:
  ```c
  g_detection_result = {"none", "forward", {0,0,0}, {{0}}, false, 0};
  ```

**Paso 5 — detection_responder.cc: análisis 3×2 con umbral combinado e histeresis**

Eliminar defines `EDGE_ROW_START/END` actuales y agregar:

```c
// Definición de grilla
#define GRID_ROW_TOP_START   0
#define GRID_ROW_TOP_END    47
#define GRID_ROW_BOT_START  48
#define GRID_ROW_BOT_END    95

// Umbrales de detección (AJUSTAR según iluminación del ambiente)
#define EDGE_DIFF_THR       10    // diferencia relativa mínima respecto al promedio global
#define EDGE_ABS_THR        80    // luminancia máxima absoluta para considerar "oscuro" (borde)

// Histeresis: N frames consecutivos para confirmar cambio de acción
#define ACTION_CONFIRM_N     3
```

> **Nota sobre umbrales:** `EDGE_DIFF_THR` y `EDGE_ABS_THR` dependen de la iluminación del ambiente (luz natural vs fluorescente puede requerir valores distintos). Se recomienda exponerlos como parámetros en `sdkconfig` o como `#define` en un header separado `config.h` para ajustar sin modificar la lógica.

Nueva función `analyze_grid()`:

```c
static void analyze_grid(const int8_t* image, int width, int height,
                          int8_t grid[2][3], int8_t dist[3], char* action) {

    // 1. Calcular promedio de luminancia por celda
    int col_w = width / 3;
    int row_ranges[2][2] = {
        {GRID_ROW_TOP_START, GRID_ROW_TOP_END},
        {GRID_ROW_BOT_START, GRID_ROW_BOT_END}
    };

    int sum[2][3] = {0};
    int count[2][3] = {0};

    for (int r = 0; r < 2; r++) {
        for (int y = row_ranges[r][0]; y <= row_ranges[r][1]; y++) {
            for (int c = 0; c < 3; c++) {
                int x_start = c * col_w;
                int x_end   = (c == 2) ? width : x_start + col_w;
                for (int x = x_start; x < x_end; x++) {
                    sum[r][c]   += (uint8_t)image[y * width + x];
                    count[r][c] += 1;
                }
            }
        }
        for (int c = 0; c < 3; c++) {
            grid[r][c] = (int8_t)(sum[r][c] / count[r][c]);
        }
    }

    // 2. Promedio global de las 6 celdas
    int global_sum = 0;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 3; c++)
            global_sum += grid[r][c];
    int global_mean = global_sum / 6;

    // 3. Detección con umbral COMBINADO: relativo + absoluto
    //    Esto evita falsos negativos cuando toda la imagen es borde
    bool edge_present[2][3];
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 3; c++) {
            edge_present[r][c] = (grid[r][c] < EDGE_ABS_THR) &&
                                 ((global_mean - grid[r][c]) >= EDGE_DIFF_THR);
        }
    }

    // 4. dist[c]: 2=cerca (bot), 1=lejos (top solo), 0=sin borde
    for (int c = 0; c < 3; c++) {
        if      (edge_present[1][c]) dist[c] = 2;
        else if (edge_present[0][c]) dist[c] = 1;
        else                         dist[c] = 0;
    }

    // 5. Lógica de acción (prioridad en fila inferior / borde cercano)
    const char* new_action;
    if (dist[1] == 2)                          new_action = "stop";
    else if (dist[0] == 2 && dist[2] == 2)    new_action = "stop";
    else if (dist[0] == 2 && dist[2] != 2)    new_action = "turn_right";
    else if (dist[2] == 2 && dist[0] != 2)    new_action = "turn_left";
    else if (dist[1] == 1)                     new_action = "stop";  // borde central lejos: reducir
    else if (dist[0] == 1)                     new_action = "turn_right";
    else if (dist[2] == 1)                     new_action = "turn_left";
    else                                        new_action = "forward";

    // 6. Histeresis: confirmar N frames antes de cambiar acción
    static char   s_last_action[16]  = "forward";
    static uint8_t s_confirm_count   = 0;

    if (strcmp(new_action, s_last_action) != 0) {
        s_confirm_count++;
        if (s_confirm_count >= ACTION_CONFIRM_N) {
            strncpy(s_last_action, new_action, sizeof(s_last_action) - 1);
            s_confirm_count = 0;
        }
        // mientras no se confirme, mantener la acción anterior
    } else {
        s_confirm_count = 0;
    }

    strncpy(action, s_last_action, 15);
    action[15] = '\0';
}
```

Actualizar `RespondToDetection()`:
- Llamar a `analyze_grid()` con la imagen del tensor de output
- Rellenar todos los campos de `g_detection_result` (grid, dist, action, edge legacy)
- Derivar `edge` legacy desde la fila inferior (columna con menor promedio en bot)
- Log `ESP_LOGI` solo cuando cambie el valor de `action` (comparar con último valor logueado)

**Paso 6 — stream_server.cc: JSON ampliado**
- Aumentar `char json[96]` → `char json[200]`
- Actualizar `snprintf` para incluir todos los campos nuevos:
  ```json
  {"edge":"left","action":"turn_right","dist":[2,1,0],"grid":[[50,20,30],[80,40,20]],"identifier":false,"confidence":0}
  ```
  (~120 chars típico → seguro con 200)

---

### Fase 3 — Servidor de control con timeout de seguridad (esp32_server)

**Paso 7 — web.cpp: parsear `action` + buffer mayor + timeout de seguridad**

- `s_auto_resp[128]` → `s_auto_resp[256]`
- Definir intervalo de polling y timeout:
  ```c
  #define AUTO_POLL_INTERVAL_MS   100   // GET a /detection cada 100 ms
  #define AUTO_SAFETY_TIMEOUT_MS  500   // si no hay respuesta válida en 500 ms → stop
  ```
- Agregar timestamp del último GET exitoso:
  ```c
  static uint32_t s_last_ok_ms = 0;
  ```
- En `autoTask`, cambiar lógica de parsing y agregar timeout de seguridad:
  ```c
  void autoTask(void *pvParam) {
      while (1) {
          uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

          int http_ret = http_get_detection(s_auto_resp, sizeof(s_auto_resp));

          if (http_ret == 0) {
              s_last_ok_ms = now;

              // Parsear "action" para control del motor
              char *action_ptr = strstr(s_auto_resp, "\"action\":\"");
              if (action_ptr) {
                  action_ptr += strlen("\"action\":\"");
                  if      (strncmp(action_ptr, "turn_left",  9) == 0) turnLeft();
                  else if (strncmp(action_ptr, "turn_right", 10) == 0) turnRight();
                  else if (strncmp(action_ptr, "stop",        4) == 0) motorStop();
                  else if (strncmp(action_ptr, "forward",     7) == 0) moveForward();
              }

              // Mantener parseo de "edge" solo para dashboard HTML
              // (sin cambios)

          } else {
              // GET fallido: verificar timeout de seguridad
              if ((now - s_last_ok_ms) >= AUTO_SAFETY_TIMEOUT_MS) {
                  ESP_LOGW("AUTO", "Timeout camara — deteniendo motor");
                  motorStop();
              }
          }

          vTaskDelay(pdMS_TO_TICKS(AUTO_POLL_INTERVAL_MS));
      }
  }
  ```

---

## Archivos a modificar

| Archivo | Cambios |
|---|---|
| `cam_arena/sdkconfig` | Habilitar `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` |
| `cam_arena/sdkconfig.defaults` | Ídem |
| `cam_arena/main/main_functions.cc` | `tensor_arena` estático, guard setup, heap monitor por tiempo real |
| `cam_arena/main/image_provider.cc` | Eliminar `MicroPrintf`, `fb_return` seguro en todos los paths |
| `cam_arena/main/detection_responder.cc` | Análisis 3×2, umbral combinado, histeresis N-frames |
| `cam_arena/main/stream_server.h` | Nueva `detection_result_t` con `action`, `dist[3]`, `grid[2][3]` |
| `cam_arena/main/stream_server.cc` | JSON ampliado a 200 bytes, inicializador actualizado |
| `esp32_server/main/web.cpp` | Buffer 256, parseo `action`, timeout de seguridad |

---

## Calibración de umbrales

Los valores `EDGE_DIFF_THR` y `EDGE_ABS_THR` en `detection_responder.cc` dependen del ambiente de iluminación. Para calibrar:

1. Colocar el vehículo sobre la plataforma y abrir el monitor serial
2. Añadir temporalmente un `ESP_LOGI` que imprima los 6 valores de `grid[r][c]` en cada frame
3. Observar los promedios con el borde visible y sin él:
   - Celdas **sin borde**: valores altos (madera clara, ej. 120-200)
   - Celdas **con borde**: valores bajos (borde oscuro, ej. 20-60)
4. Ajustar `EDGE_ABS_THR` al punto medio entre ambos rangos
5. Ajustar `EDGE_DIFF_THR` para que la diferencia relativa sea robusta ante variaciones de luz

> Se recomienda exponer ambos valores en `sdkconfig` como `CONFIG_EDGE_ABS_THR` y `CONFIG_EDGE_DIFF_THR` para ajustar sin recompilar.

---

## Verificación

1. **Compilar cam_arena** (`idf.py build`) sin errores
2. **Compilar esp32_server** (`idf.py build`) sin errores
3. **Flashear cam_arena** y observar monitor serial:
   - Cada 5 s debe aparecer línea `[HEAP] libre: XXXX | SPIRAM: XXXX`
   - Confirmar que los valores **no decrecen** después de 2 minutos → sin leak
   - Los logs de borde solo deben aparecer cuando cambia la acción, no cada frame
4. **Consultar endpoint** `curl http://<cam_ip>:81/detection` y verificar JSON con campos `action`, `dist`, `grid`
5. **Desconectar la cámara** con el modo AUTO activo → confirmar que el robot se detiene dentro de 500 ms
6. **Activar modo AUTO** y probar que el robot reacciona correctamente a `turn_left` / `turn_right` / `stop` / `forward`
7. **Ajustar umbrales** si la detección es inconsistente (ver sección de calibración)

---

## Decisiones de diseño

| Decisión | Motivo |
|---|---|
| `EXT_RAM_BSS_ATTR` para `tensor_arena` | Elimina fragmentación del heap; requiere activar `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` |
| Heap monitor por tiempo real (5 s) vs por iteraciones | A 30 fps, 500 iteraciones = 16 s; demasiado lento para detectar un leak |
| Umbral combinado (relativo + absoluto) | El umbral solo relativo falla cuando toda la imagen es borde; el absoluto evita este falso negativo |
| Histeresis N-frames en `action` | Evita oscilación del robot ante ruido de imagen o sombras transitorias |
| Timeout de seguridad 500 ms en `autoTask` | Si la cámara falla o el WiFi se interrumpe, el robot no continúa con el último comando indefinidamente |
| Campo `"edge"` legacy mantenido en JSON | El dashboard HTML lo sigue usando; no rompe compatibilidad |
| Log `ESP_LOGI` solo al cambiar `action` | Reduce carga de UART y facilita lectura del monitor serial |
| `AUTO_POLL_INTERVAL_MS = 100` | Latencia de reacción de 100 ms es suficiente para velocidades bajas de vehículo |
