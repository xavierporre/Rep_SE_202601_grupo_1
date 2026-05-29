# Lab 3 — TensorFlow Lite Micro: Detección de Personas en ESP32

Este laboratorio implementa un sistema de **detección de personas en tiempo real** usando TensorFlow Lite Micro sobre hardware Espressif. Se trabaja con dos proyectos independientes: inferencia sobre imágenes estáticas con reporte en PDF (`person_detection`) y detección continua desde cámara con LED indicador (`cam_integrado`).

---

## Estructura del laboratorio

```
Lab_3/
├── esp-tflite-micro/          # Librería TFLite Micro (componente IDF local)
└── projects_tf/
    ├── person_detection/      # Punto 2.1: inferencia sobre imágenes estáticas
    └── cam_integrado/         # Punto 2.2: inferencia continua con cámara OV3660
```

---

## Entorno de desarrollo

| Parámetro | Valor |
|-----------|-------|
| ESP-IDF | v6.1-dev (`$HOME/esp/idf/esp-idf`) |
| Python env | `idf6.1_py3.12_env` |
| Toolchain | `xtensa-esp-elf` esp-15.2.0_20251204 |

Variables de entorno necesarias:

```bash
export IDF_TOOLS_PATH="$HOME/esp/idf-tools"
export IDF_PATH="$HOME/esp/idf/esp-idf"
export IDF_PYTHON_ENV_PATH="$HOME/esp/idf-tools/python_env/idf6.1_py3.12_env"
export ESP_IDF_VERSION="6.1-dev"
export ESP_ROM_ELF_DIR="$HOME/esp/idf-tools/tools/esp-rom-elfs/20240305"
export PATH="$HOME/esp/idf-tools/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin:\
$HOME/esp/idf-tools/python_env/idf6.1_py3.12_env/bin:\
$IDF_PATH/tools:$PATH"
```

---

## Punto 2.1 — `person_detection` (ESP32-S3)

Inferencia sobre **10 imágenes estáticas** embebidas y **3 fotos del grupo** (Xavier, Martín, Vicente). Target: **ESP32-S3**, puerto `/dev/cu.usbmodem101`.

### Modelo y operaciones TFLite

El modelo cuantizado int8 de 250 KB detecta personas vs. no-personas. Operaciones registradas:
- `AveragePool2D`, `Conv2D`, `DepthwiseConv2D`, `Reshape`, `Softmax`

### Modo CLI (sin cámara)

En [`main/esp_main.h`](projects_tf/person_detection/main/esp_main.h) está habilitado:

```cpp
#define CLI_ONLY_INFERENCE 1
```

Para inferencia automática al arrancar sobre las 13 imágenes (10 estáticas + 3 del grupo), se llama a `run_inference_int8()` en secuencia desde `app_main`.

### Build y flash

```bash
cd projects_tf/person_detection
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

### Generación del informe PDF

El script [`generate_pdf.py`](projects_tf/person_detection/generate_pdf.py) captura automáticamente la salida serie, reconstruye las imágenes desde los archivos fuente y genera [`output.pdf`](projects_tf/person_detection/output.pdf) con:

- Una página por imagen: imagen reconstruida + barra de confianza + veredicto
- Página final: tabla resumen de los 13 resultados

```bash
cd projects_tf/person_detection
python3 generate_pdf.py
```

El PDF resultante queda en `output.pdf`.

---

## Punto 2.2 — `cam_integrado` (AI Thinker ESP32-CAM)

Inferencia continua desde cámara **OV3660** (compatible OV2640) con LED flash GPIO 4. Target: **ESP32 clásico**, adaptador USB-UART en `/dev/cu.usbserial-10`.

### Hardware

| Señal | GPIO |
|-------|------|
| LED flash (blanco) | 4 |
| PWDN cámara | 32 |
| XCLK | 0 |
| SIOD / SIOC | 26 / 27 |
| VSYNC / HREF / PCLK | 25 / 23 / 22 |
| PSRAM | 8 MB (modo Quad 80 MHz) |

### Lógica de detección

Umbral configurado al **75%** en [`main/detection_responder.cc`](projects_tf/cam_integrado/main/detection_responder.cc):

```cpp
#define PERSON_THRESHOLD  0.75f
```

- `persona >= 75%` → **LED ON** + log `[LED ON ] Persona detectada`
- `persona < 75%`  → **LED OFF** + log `[LED OFF] Sin persona`

Ciclo de inferencia: ~570 ms/frame.

### Build y flash

> ⚠️ Conectar **IO0 a GND** antes de flashear. Desconectar IO0 tras el flash y resetear.

```bash
cd projects_tf/cam_integrado
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-10 flash
idf.py -p /dev/cu.usbserial-10 monitor
```

### Salida serie esperada

```
I (895) cam_integrado: Inicializando modelo TFLite y camara OV2640...
I (945) camera: Detected OV3660 camera
I (1285) cam_integrado: Bucle de inferencia continua iniciado (umbral persona >= 75%)
Image Captured
I (2035) detection: [LED OFF] Sin persona        | persona=25%  no_persona=75%
Image Captured
I (2605) detection: [LED ON ] Persona detectada  | persona=81%  no_persona=19%
```

---

## Componentes y dependencias

| Componente | Fuente |
|------------|--------|
| `esp-tflite-micro` | Local (`../esp-tflite-micro`), override en `idf_component.yml` |
| `espressif/esp32-camera` | IDF Component Manager |
| `espressif/esp-nn` | Descargado automáticamente como dependencia transitiva |

---

## Resultados obtenidos

| Proyecto | Target | Imágenes | Resultado |
|---------|--------|----------|-----------|
| `person_detection` | ESP32-S3 | 10 estáticas + 3 fotos grupo | ✅ 13/13 inferencias OK, `output.pdf` generado |
| `cam_integrado` | ESP32-CAM (AI Thinker) | Cámara en tiempo real | ✅ LED funcional, ~570 ms/frame |
