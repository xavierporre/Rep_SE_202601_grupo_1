# Rep_SE_202601_grupo_1

Repositorio del curso ICC4200 - Sistemas Embebidos 202610

## Integrantes

- Xavier Porre
- Martin Feres
- Vicente Abalos

---

## Descripción general del proyecto

El proyecto (`Proyecto/`) es un **robot de combate para ring**: opera dentro de un
ring rectangular de superficie blanca con bordes de cinta negra, y su regla
fundamental es **no cruzar nunca el borde**. El robot se gobierna desde un
**servidor web WiFi** que ofrece control manual más cuatro modos autónomos.

El sistema se reparte en **dos placas** que se comunican por un enlace serial
físico (UART): una **ESP32-CAM** que actúa como los "ojos" (corre un modelo de IA
y detecta el borde de la cinta) y una **ESP32-S3** que actúa como el "cerebro"
(servidor web, máquina de estados, motores y LED de estado).

| Modo | Comportamiento |
|---|---|
| **Manual** | Control directo desde la web (d-pad + slider de velocidad). |
| **Find** (buscar y destruir) | Gira sobre su eje escaneando con la cámara; al detectar el identificador (modelo TFLite, confianza ≥ `IDENT_CONF_THR`) carga de frente al 100 %. Si toca el borde, retrocede y vuelve a escanear. |
| **Patrol** (patrullar) | Recorre el perímetro siguiendo la cinta negra (cinta a la derecha, vuelta antihoraria) sin cruzarla. |
| **Retreat** (retirada) | Gira ~180°, avanza hasta el borde de enfrente, gira ~90° y bordea hasta detenerse en una **esquina** del ring. |
| **Audio** (opcional) | Controla la dirección según la frecuencia captada por un micrófono (MAX9814). |

Movimientos lineales al **100 %** de PWM; giros al **80 %**, siempre sobre el eje
(ruedas en sentidos opuestos).

> Para el detalle fino (calibración, verificación end-to-end, protocolo extendido)
> ver la especificación técnica en [Proyecto/README.md](Proyecto/README.md).

---

## Arquitectura del sistema (dos placas)

```
┌────────────────────┐   UART1 115200 8N1    ┌──────────────────────┐
│ ESP32-CAM          │  D,action,d0,d1,d2,   │ ESP32-S3             │
│ (AI-Thinker OV2640)│  ident,conf\n  10 Hz  │                      │
│ robot_cam/         │ ────────────────────► │ robot_s3/            │
│ · TFLite Micro:    │  GPIO14 → GPIO18      │ · WiFi AP "AutoRC"   │
│   identificador    │  GPIO13 ← GPIO17      │ · Servidor HTTP :80  │
│   96x96 gris, 2cls │  GND   ↔ GND          │ · Máquina de estados │
│ · Borde: grilla 3x2│      (reservado)      │ · L298N (motores)    │
│ · SIN WiFi         │                       │ · LED WS2812 GPIO48  │
└────────────────────┘                       └──────────────────────┘
```

La **ESP32-CAM no usa WiFi**: envía sus resultados de visión por **UART físico** a
la ESP32-S3. Esto desacopla la latencia de inferencia del control en tiempo real y
deja el WiFi libre para el servidor web.

### Cableado entre placas

| ESP32-CAM | ESP32-S3 | Función |
|---|---|---|
| GPIO14 (TX) | GPIO18 (RX) | Datos CAM → S3 (obligatorio) |
| GPIO13 (RX) | GPIO17 (TX) | Reservado S3 → CAM (cablear igual) |
| GND | GND | Tierra común (obligatorio) |

Ambas placas son lógica 3.3 V (conexión directa). **Nunca usar GPIO12 de la CAM**
(pin de strapping MTDI: un nivel alto en el arranque fija el flash a 1.8 V y la
placa no inicia).

### Protocolo UART (CAM → S3)

```
D,<action>,<d0>,<d1>,<d2>,<ident>,<conf>\n     ej: D,turn_left,0,1,2,1,87
```

- `action`: sugerencia de la CAM (`forward|turn_left|turn_right|stop`)
- `d0,d1,d2`: borde por columna [izq, centro, der] — `0`=nada, `1`=lejos, `2`=cerca
- `ident`: `1` si el score del identificador supera el umbral en la CAM
- `conf`: score en % (el umbral de carga `IDENT_CONF_THR` vive en el S3 y se ajusta
  sin reflashear la CAM)

La línea se repite a **10 Hz fijo** (independiente de la latencia de inferencia).
Si el S3 no recibe datos válidos por **500 ms** (`UART_TIMEOUT_MS`), detiene los
motores y parpadea en rojo hasta recuperar el enlace.

---

## Descripción física del robot

### Componentes confirmados (deducidos del código/configuración)

| Componente | Detalle | Referencia |
|---|---|---|
| **Controladora** | ESP32-S3 — AP WiFi, servidor HTTP, control de motores, LED y audio | `Proyecto/robot_s3/` |
| **Cámara / visión** | ESP32-CAM **AI-Thinker** con sensor **OV2640**, capturando a 96×96 en escala de grises | `robot_cam/sdkconfig`: `CONFIG_CAMERA_MODULE_AI_THINKER=y`, `CONFIG_OV2640_SUPPORT=y` |
| **LED flash de la CAM** | GPIO4 de la ESP32-CAM, se enciende al detectar el identificador | `robot_cam/main/` |
| **Driver de motores** | **L298N** (doble puente H), tracción diferencial de 2 ruedas (motores DC) | `robot_s3/main/web.cpp` |
| **LED de estado** | LED RGB direccionable **WS2812** (1 píxel) en GPIO48 | `robot_s3/main/web.cpp` |
| **Micrófono (opcional)** | Módulo **MAX9814** (AGC, salida analógica) → GPIO1 (ADC1_CH0) | `robot_s3/main/audio_ctrl.c` |
| **Enlace entre placas** | UART1 a 115200 8N1 (ver tabla de cableado arriba) | — |

**Pines del driver L298N en la ESP32-S3** (PWM por LEDC, 5 kHz / 8 bits):

| Función | Pin | | Función | Pin |
|---|---|---|---|---|
| ENA (PWM motor A) | GPIO4 | | ENB (PWM motor B) | GPIO7 |
| IN1 (dirección A) | GPIO5 | | IN3 (dirección B) | GPIO15 |
| IN2 (dirección A) | GPIO6 | | IN4 (dirección B) | GPIO16 |

Los movimientos se implementan como combinaciones de IN1–IN4 + PWM en las funciones
`moveForward` / `moveBackward` / `turnLeft` / `turnRight` / `motorStop`
([web.cpp](Proyecto/robot_s3/main/web.cpp)). Los giros son **sobre el eje** (ruedas en
sentidos opuestos).

---

## Modelo de IA para detección del identificador

El robot necesita reconocer un "identificador" (objetivo) dentro del ring. Para ello
se entrenó un **clasificador binario** (identificador presente / ausente) que corre
embebido en la ESP32-CAM. Todo el pipeline de entrenamiento vive en
[Proyecto/dataset_identificador/](Proyecto/dataset_identificador/) y el camino de
iteraciones está documentado en
[NOTAS_ENTRENAMIENTO.md](Proyecto/dataset_identificador/NOTAS_ENTRENAMIENTO.md).

### Arquitectura (CNN ligera, TensorFlow/Keras)

Red convolucional propia, deliberadamente pequeña para caber en un microcontrolador:

```
Entrada 96×96×1 (gris)
 → Conv2D(16, 3×3) + BatchNorm + ReLU → MaxPool 2×2
 → Conv2D(32, 3×3) + BatchNorm + ReLU → MaxPool 2×2
 → Conv2D(64, 3×3) + BatchNorm + ReLU → MaxPool 2×2
 → GlobalAveragePooling2D
 → Dropout(0.5)
 → Dense(32, ReLU)
 → Dense(2, softmax)        # [no_identificador, identificador]
```

≈ **23 000 parámetros**, regularización L2 (1e-4) en los kernels.

### Métodos y técnicas usadas en el desarrollo

- **Diagnóstico y corrección de overfitting**: la versión inicial usaba `Flatten`
  (≈318 K parámetros) y memorizaba el train set. Se reemplazó por
  **`GlobalAveragePooling2D`**, que colapsa los mapas 12×12×64 a 64 valores por canal
  y reduce el modelo a ~23 K parámetros — apropiado para el tamaño del dataset.
- **Aumentación de datos**: `RandomFlip`, `RandomRotation`, `RandomZoom`,
  `RandomBrightness` y `RandomContrast` para generalizar a distintas vistas e
  iluminaciones.
- **Diagnóstico de sesgo con GradCAM**: el script `diagnostico_sesgo.py` genera mapas
  de activación (GradCAM sobre la capa `conv2d_2`). Reveló que el modelo se fijaba en
  un único rasgo/ángulo del objeto → se corrigió **capturando imágenes dirigidas** por
  bloques de ángulo y distancia.
- **Configuración de entrenamiento**: optimizador **Adam (LR = 5e-4)**, pérdida
  `sparse_categorical_crossentropy`, batch 16, hasta 160 épocas, con callbacks
  `EarlyStopping` / `ModelCheckpoint` / `ReduceLROnPlateau` monitoreando
  **`val_accuracy`** (no `val_loss`, que seleccionaba checkpoints conservadores con
  alto FN).

### Dataset

- **977 imágenes** PNG de 96×96 en escala de grises (569 sin identificador / 408 con
  identificador).
- Split estratificado **85/15** → 829 entrenamiento / 148 test.
- Captura propia con firmware dedicado (`cam_capture/`) + herramienta de etiquetado
  (`capture_tool.py`) que recibe los frames de la CAM por UART.

### Cuantización y exportación a microcontrolador

`quantize_and_export.py` realiza **cuantización post-entrenamiento a int8**
(`tf.lite.Optimize.DEFAULT` + dataset representativo de 150 imágenes), genera el
`.tflite` (~36 KB) y lo convierte a un **array C** (`identificador_model_data.cc/.h`)
que se embebe directamente en el firmware de la CAM. El escalado int8 (`uint8 − 128`)
coincide con la conversión del `image_provider` en la CAM.

### Resultado final

**86.49 % de exactitud en test** (TP=58, TN=70, FP=16, FN=4). FN muy bajo: el modelo
casi nunca pierde el identificador cuando está presente (preferible en combate frente
a un falso positivo).

| Intento | Fecha | Dataset | Test acc. | Resultado |
|---|---|---|---|---|
| 1 | 2026-06-16 | 455 img | 66.7 % | ✗ overfitting (Flatten, 318 K params) |
| 2 | 2026-06-17 | 635 img | 78.1 % | ✗ exceso de falsos positivos |
| 3 | 2026-06-17 | 811 img | 85.25 % | ✓ aprobado (sesgo de orientación detectado) |
| 4 | 2026-06-20 | 977 img | **86.49 %** | ✓ **modelo final** |

**Archivos clave** ([Proyecto/dataset_identificador/](Proyecto/dataset_identificador/)):

| Archivo | Función |
|---|---|
| `train_identificador.py` | Entrenamiento de la CNN |
| `quantize_and_export.py` | Cuantización int8 y exportación a array C |
| `diagnostico_sesgo.py` | Análisis GradCAM de sesgo |
| `capture_tool.py` + `cam_capture/` | Captura y etiquetado del dataset |
| `modelo/identificador_model.tflite` + `*_data.cc/.h` | Modelo cuantizado y array C embebido |

---

## Uso de la ESP32-CAM

La ESP32-CAM (`Proyecto/robot_cam/`) es el subsistema de **percepción** del robot.
Por cada frame realiza dos tareas y publica el resultado al S3 por UART.

### ¿Para qué se usa?

1. **Correr el modelo `identificador`** (clasificar si el objetivo está en el encuadre).
2. **Detectar el borde de cinta negra** en una grilla 3×2 para no cruzar el límite.

Ambos resultados alimentan la máquina de estados del S3, que decide cómo moverse.

### Cómo se integra el modelo entrenado

- El **array C** generado en el entrenamiento (`identificador_model_data.cc/.h`) se
  embebe en el firmware de la CAM; `main_functions.cc` lo carga con **TFLite Micro**
  (`tflite::GetModel(g_identificador_model_data)`).
- `model_settings.h` define la forma de entrada/salida: **96×96×1, 2 clases**.
- El _tensor arena_ se aloja en **SPIRAM** (PSRAM); la frecuencia de inferencia se
  regula con `INFERENCE_EVERY_N` (subir a 2–3 si `Invoke()` tarda demasiado, para que
  la detección de borde se refresque más rápido).

### Cómo se ocupa (flujo por frame)

1. **Inicialización de cámara** (`app_camera_esp.c`): sensor OV2640, 96×96 en escala
   de grises, 2 frame buffers en PSRAM, pines de la placa AI-Thinker.
2. **Inferencia**: se ejecuta el modelo sobre el frame → score del identificador.
3. **Detección de borde** (`detection_responder.cc`): divide el frame en una **grilla
   3×2** y mide luminancia en la fila inferior contra `EDGE_ABS_THR` para clasificar
   cada columna en 0/1/2 (nada/lejos/cerca).
4. **Publicación** (`uart_comm.c`): arma la trama `D,action,d0,d1,d2,ident,conf` y la
   envía por UART a 10 Hz.

> Montaje de la cámara: la mitad inferior del frame debe ver el piso a 10–30 cm (para
> el borde) y la superior el horizonte (para el identificador). El mismo frame sirve
> para ambas tareas.

---

## Servidor web en ESP32-S3

La ESP32-S3 (`Proyecto/robot_s3/`) levanta un **punto de acceso WiFi** y un servidor
HTTP que es la interfaz de control del robot, además de albergar la máquina de estados.

### Infraestructura

- **AP WiFi**: SSID `AutoRC`, clave `12345678`.
- **Servidor HTTP**: `http://192.168.4.1` (puerto 80).
- **Frontend**: HTML/JS embebido como string en
  [web.cpp](Proyecto/robot_s3/main/web.cpp) (no usa SPIFFS). La interfaz tiene d-pad,
  slider de velocidad, botones de modo, un log en vivo y soporte de teclado (WASD).
- **Comunicación**: la web usa `fetch` y hace **polling de `/status` cada 500 ms** (no
  hay WebSocket ni SSE).

### Rutas HTTP (objetivo y cómo se realiza)

| Ruta | Objetivo | Cómo se realiza |
|---|---|---|
| `GET /` | Servir la interfaz | Devuelve el HTML embebido (`hRoot`) |
| `GET /forward` `/backward` `/left` `/right` (`?speed=0-100`) | Control manual | `ensureManual()` saca del modo autónomo y comanda el L298N; responde `text/plain` |
| `GET /stop` | Detener motores | `motorStop()` |
| `GET /ping` | Probar conexión | Parpadea el LED y responde `pong` |
| `GET /mode?set=manual\|find\|patrol\|retreat\|audio` | Cambiar de modo | Llama a `control_set_mode()` y reinicia la máquina de estados |
| `GET /status` | Telemetría | JSON `{mode,sub,action,dist[],ident,conf,link,age_ms}` (`control_status_json`) |
| `OPTIONS *` | Preflight CORS | Responde cabeceras CORS vacías |

### Máquina de estados / rutinas autónomas

El lazo de control corre a **20 Hz** (`control_task`, `CTRL_TICK_MS = 50`) en
[control.cpp](Proyecto/robot_s3/main/control.cpp); los modos y subestados se definen en
[robot_state.h](Proyecto/robot_s3/main/robot_state.h). En cada tick lee el último dato de
la cámara y aplica un **override global de borde**: si la cinta está al frente o a ambos
lados, fuerza la maniobra de evasión (excepto donde es seguro o se está comprometido).

- **FIND** (buscar y destruir):
  `LOOK` (parado observando, `FIND_LOOK_MS = 2500`) → `SCAN` (gira escaneando,
  `FIND_TURN_MS = 600`) → `CHARGE` (al superar `IDENT_CONF_THR`, carga de frente al
  100 %; si pierde el identificador por `IDENT_LOST_MS = 2000` vuelve a `LOOK`) →
  `AVOID` (retrocede/gira si toca el borde). El override de borde actúa salvo en `LOOK`
  (robot quieto, sin riesgo) y `CHARGE` (carga comprometida). *Cambio reciente (commit
  `59ec49a7`): se quitaron los empujones laterales durante la carga; el borde lo maneja
  la transición a `AVOID`.*
- **PATROL**: sigue la cinta a la derecha (vuelta antihoraria) avanzando con pulsos de
  corrección; al detectar una esquina (borde de frente sostenido) gira ~90° y continúa;
  si pierde la cinta ejecuta un arco de búsqueda.
- **RETREAT**: gira ~180° → avanza hasta el borde de enfrente → gira ~90° → bordea la
  cinta hasta detenerse en una **esquina** (`HOLD`). Tiene timeouts anti-atasco.
- **Seguridad**: si el enlace UART con la CAM cae > 500 ms, detiene los motores y
  parpadea en rojo hasta recuperarlo.

### LED de estado (WS2812, GPIO48)

| Color | Estado |
|---|---|
| Azul tenue | Manual en reposo |
| Cian | FIND: escaneando |
| Azul | FIND: mirando quieto |
| Verde brillante | FIND: cargando al identificador |
| Amarillo | PATROL |
| Magenta | RETREAT en movimiento |
| Blanco tenue | RETREAT: en la esquina (hold) |
| Rojo fijo | Retroceso por borde |
| Rojo parpadeante | Enlace con la CAM perdido (motores detenidos) |
| Naranja | Modo audio activo |

### Constantes de temporización (en `robot_state.h`)

| Constante | Valor | Uso |
|---|---|---|
| `CTRL_TICK_MS` | 50 | Periodo del lazo de control (20 Hz) |
| `UART_TIMEOUT_MS` | 500 | Sin datos de la CAM → parada de seguridad |
| `FIND_LOOK_MS` | 2500 | Tiempo parado observando en FIND |
| `FIND_TURN_MS` | 600 | Giro breve entre miradas |
| `IDENT_CONF_THR` | 50 | Confianza % para iniciar la carga (calibrable) |
| `IDENT_LOST_MS` | 2000 | Sin detección durante la carga → volver a escanear |
| `AVOID_TURN_MS` | 400 | Giro de evasión tras tocar el borde |
| `TURN_180_MS` / `TURN_90_MS` | 1200 / 600 | Giros temporizados (calibrar en el robot) |

---

## Control por audio (modo opcional)

Modo adicional que controla la dirección del robot según la **frecuencia** captada por
un micrófono **MAX9814** (en GPIO1, ADC1_CH0). Reutiliza el detector de frecuencias del
Lab_1 (muestreo ADC → DFT simple): distintas bandas de frecuencia se mapean a adelante /
izquierda / derecha / atrás. El detalle completo (bandas, parámetros calibrables y notas
de implementación) está en [Proyecto/README.md](Proyecto/README.md#control-por-audio-mode_audio).

---

## Configuracion de ESP-IDF

### 1. Preparar estructura de carpetas

```bash
mkdir ~/esp && mkdir ~/esp/idf && mkdir ~/esp/idf-tools && mkdir ~/esp/backup && mkdir ~/esp/projects
```

### 2. Clonar ESP-IDF

```bash
cd ~/esp/idf
git clone --recursive https://github.com/espressif/esp-idf.git
```

### 3. Instalar ESP-IDF

```bash
cd ~/esp/idf/esp-idf
./install.sh
. ./export.sh
```

### 4. Configurar aliases y variables de entorno

Agregar en `~/.zprofile`:

```bash
export IDF_TOOLS_PATH="$HOME/esp/idf-tools"
export PATH="$HOME/esp/idf/esp-idf/tools:$PATH"
export IDF_PATH="$HOME/esp/idf/esp-idf"
```

Agregar en `~/.zshrc`:

```bash
alias get_esp32='. $HOME/esp/idf/esp-idf/export.sh'
```

Aplicar cambios:

```bash
source ~/.zprofile && source ~/.zshrc
```

### 5. Compilar y flashear un proyecto

```bash
source ~/.zprofile && source ~/.zshrc && get_esp32
cd <ruta_del_proyecto>
idf.py set-target esp32s3
idf.py build && idf.py flash monitor
```

Para salir del monitor serial: `Ctrl + ]`

## Compilar y flashear el robot

El componente TFLite es un submódulo de git que debe inicializarse una vez:

```bash
git submodule update --init Lab_3/esp-tflite-micro
```

Luego, con el entorno de ESP-IDF exportado:

```bash
# ESP32-S3 (control + web)
cd Proyecto/robot_s3
idf.py set-target esp32s3
idf.py build && idf.py -p <PUERTO_S3> flash monitor

# ESP32-CAM (visión)
cd ../robot_cam
idf.py set-target esp32
idf.py build && idf.py -p <PUERTO_FTDI> flash monitor
```

La ESP32-CAM se flashea con un adaptador USB-TTL en UART0 manteniendo GPIO0 a GND
durante el reset. Ver detalle y guía de verificación en
[Proyecto/README.md](Proyecto/README.md).

## Estructura del repositorio

```
Rep_SE_202601_grupo_1
|- Lab_1
|   |- Ejercicio_1
|   |- Ejercicio_2
|   |- Ejercicio_3
|- Lab_2
|- Lab_3
|- Proyecto
|   |- README.md                 # Especificación técnica detallada
|   |- robot_s3/                 # Firmware ESP32-S3: web, máquina de estados, motores
|   |- robot_cam/                # Firmware ESP32-CAM: cámara, inferencia, detección de borde
|   |- dataset_identificador/    # Pipeline de IA: dataset, entrenamiento, cuantización
```
