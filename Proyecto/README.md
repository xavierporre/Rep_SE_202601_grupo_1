# Proyecto — Robot de Ring

Robot que opera dentro de un ring rectangular (superficie blanca, bordes de
cinta negra) **sin cruzar nunca el borde**. Se controla desde un servidor web
WiFi con tres ordenes autonomas mas control manual:

| Orden | Comportamiento |
|---|---|
| **Buscar y destruir** (`find`) | Gira sobre el eje escaneando con la camara; al detectar el identificador (modelo TFLite `identificador`/embutido, confianza ≥ 75 %) carga de frente al 100 %. Si toca el borde, retrocede y vuelve a escanear. |
| **Patrullar** (`patrol`) | Recorre el perimetro siguiendo la cinta negra (cinta a la derecha, vuelta antihoraria) sin cruzarla. |
| **Retirada** (`retreat`) | Gira ~180°, avanza hasta el borde de enfrente, gira ~90° y avanza junto al borde hasta detenerse en una **esquina** del ring. |

Movimientos lineales al **100 %** de PWM; giros al **80 %**, siempre sobre el
eje (ruedas en sentidos opuestos).

## Arquitectura (dos placas)

```
┌────────────────────┐   UART1 115200 8N1    ┌──────────────────────┐
│ ESP32-CAM          │  D,action,d0,d1,d2,   │ ESP32-S3             │
│ (AI-Thinker OV2640)│  ident,conf\n  10 Hz  │                      │
│ robot_cam/         │ ────────────────────► │ robot_s3/            │
│ · TFLite Micro:    │  GPIO14 → GPIO18      │ · WiFi AP "AutoRC"   │
│   identificador    │  GPIO13 ← GPIO17      │ · Servidor HTTP :80  │
│   96x96 gris, 2cls │  GND   ↔ GND          │ · Maquina de estados │
│ · Borde: grilla 3x2│      (reservado)      │ · L298N (motores)    │
│ · SIN WiFi         │                       │ · LED WS2812 GPIO48  │
└────────────────────┘                       └──────────────────────┘
```

A diferencia de `Lab_3/projects_tf/Arena_1` (que usaba HTTP entre placas), aqui
la CAM **no usa WiFi**: envia sus resultados por **UART fisico**.

### Cableado

| ESP32-CAM | ESP32-S3 | Funcion |
|---|---|---|
| GPIO14 (TX) | GPIO18 (RX) | Datos CAM → S3 (obligatorio) |
| GPIO13 (RX) | GPIO17 (TX) | Reservado S3 → CAM (cablear igual) |
| GND | GND | Tierra comun (obligatorio) |

Ambas placas son logica 3.3 V: conexion directa, cables cortos y lejos de los
cables de motor. **Nunca usar GPIO12 de la CAM** (strapping MTDI: un nivel alto
en el arranque fija el flash a 1.8 V y la placa no inicia).

Pines L298N en el S3 (igual que el template de Arena_1): ENA=4, IN1=5, IN2=6,
ENB=7, IN3=15, IN4=16. PWM LEDC 5 kHz, 8 bits.

### Protocolo UART (CAM → S3)

```
D,<action>,<d0>,<d1>,<d2>,<ident>,<conf>\n     ej: D,turn_left,0,1,2,1,87
```

- `action`: sugerencia de la CAM (`forward|turn_left|turn_right|stop`)
- `d0,d1,d2`: borde por columna [izq, centro, der] — 0=nada, 1=lejos, 2=cerca
- `ident`: 1 si score del identificador ≥ 0.75
- `conf`: score en % (el umbral de carga `IDENT_CONF_THR` vive en el S3 y se
  ajusta sin reflashear la CAM)

La linea se repite a 10 Hz fijo (independiente de la latencia de inferencia).
Si el S3 no recibe datos validos por 500 ms (`UART_TIMEOUT_MS`), detiene los
motores y parpadea rojo hasta recuperar el enlace.

## Servidor web (S3, AP `AutoRC` / clave `12345678`, http://192.168.4.1)

- `/` — interfaz: botones de modo, d-pad manual, slider de velocidad, estado
- `/mode?set=manual|find|patrol|retreat` — cambia la orden activa
- `/status` — JSON: `{"mode","sub","action","dist":[..],"ident","conf","link","age_ms"}`
- `/forward /backward /left /right /stop /ping` — control manual (cualquier
  comando manual saca al robot del modo autonomo)

### LED de estado (WS2812)

| Color | Estado |
|---|---|
| Azul tenue | Manual en reposo |
| Cian | FIND: escaneando |
| Verde brillante | FIND: cargando al identificador |
| Amarillo | PATROL |
| Magenta | RETREAT en movimiento |
| Blanco tenue | RETREAT: en la esquina (hold) |
| Rojo fijo | Retroceso por borde |
| Rojo parpadeante | Enlace con la CAM perdido (motores detenidos) |

## Control por audio (MODE_AUDIO)

Modo adicional que usa el mismo algoritmo de detección de frecuencias del
Lab_1/Ejercicio_3 (muestreo ADC → DFT simple) para controlar la dirección del
robot en lugar de encender LEDs.

### Hardware requerido

Conectar el módulo **MAX9814** (micrófono con AGC y salida analógica) a
**GPIO1 del ESP32-S3** (ADC1_CH0, 12 bits, atenuación 12 dB).

Pines del MAX9814:

| MAX9814 | ESP32-S3 |
|---------|----------|
| VDD | 3.3 V |
| GND | GND |
| OUT | GPIO1 |
| GAIN | sin conectar (ganancia 40 dB por defecto) |

### Activación

Desde la interfaz web o via HTTP:

```
GET http://192.168.4.1/mode?set=audio
```

El botón **🎤 Audio** aparece en la fila de modos junto a MANUAL/FIND/PATROL/RETREAT.
El LED WS2812 se pone **naranja** mientras se detecta una frecuencia válida.

### Mapeo de frecuencias → dirección

| Banda | Rango | Dirección | PWM |
|-------|-------|-----------|-----|
| 0 | 150–450 Hz | Adelante | `SPEED_LINEAR` (100 %) |
| 1 | 550–750 Hz | Izquierda (giro eje) | `SPEED_TURN` (80 %) |
| 2 | 850–1000 Hz | Derecha (giro eje) | `SPEED_TURN` (80 %) |
| 3 | 1050–1300 Hz | Atrás | `SPEED_LINEAR` (100 %) |
| — | sin banda | Stop (tras 700 ms) | 0 |

Los gaps entre bandas (450–550 Hz, 750–850 Hz, 1000–1050 Hz) se ignoran para
evitar transiciones por ruido.

### Parámetros calibrables (`audio_ctrl.c`)

| Constante | Valor | Descripción |
|-----------|-------|-------------|
| `AUDIO_SAMPLES` | 256 | Muestras por ventana DFT |
| `AUDIO_FS` | 8000 Hz | Frecuencia de muestreo |
| `AUDIO_HOLD_MS` | 700 ms | Histéresis: mantiene el último comando si no llega señal |
| `MIC_CHANNEL` | `ADC_CHANNEL_0` | Canal ADC (GPIO1); cambiar si se usa otro pin |

### Notas de implementación

- La tarea `audio_ctrl` corre en FreeRTOS a prioridad 5 (debajo del lazo de
  control en 10), stack 8 KB.
- El lazo de control (`control_task`) omite el ciclo cuando el modo es AUDIO,
  igual que en MANUAL, para no interferir con los motores.
- El modo no usa datos de la CAM: si se activa AUDIO no importa que el enlace
  UART esté caído (no dispara la parada de seguridad).
- La DFT es O(N²): a 256 muestras y 8 kHz tarda ~32 ms de muestreo + ~15 ms de
  cómputo en el S3 → latencia total ≈ 50 ms por detección, suficiente para
  control interactivo.

## Compilar y flashear

Requiere ESP-IDF (instalado en `%USERPROFILE%\esp\idf\esp-idf`). El componente
TFLite es un submodulo de git que debe inicializarse una vez:

```bat
git submodule update --init Lab_3/esp-tflite-micro
```

Luego, desde un terminal con el entorno de IDF exportado (`export.bat`):

```bat
cd Proyecto/robot_s3
idf.py set-target esp32s3
idf.py build
idf.py -p <PUERTO_S3> flash monitor

cd ../robot_cam
idf.py set-target esp32
idf.py build
idf.py -p <PUERTO_FTDI> flash monitor
```

La ESP32-CAM se flashea con un adaptador USB-TTL en UART0 (U0T→RX, U0R→TX,
GND, 5V) manteniendo GPIO0 a GND durante el reset para entrar al bootloader.
Los cables de UART1 (GPIO13/14) no interfieren con el flasheo.

## Verificacion

En banco (ruedas en el aire):

1. **CAM sola**: el monitor muestra el arranque y lineas `[CAM] accion=...` al
   cambiar la deteccion; con un USB-TTL en GPIO14 se ven las lineas `D,...` a
   10 Hz; mostrar el identificador → `ident=1` y se enciende el LED flash;
   cinta negra bajo el lente → cambian los `dist`.
2. **S3 solo**: conectarse al AP, abrir la web; d-pad/slider/STOP funcionan;
   `/status` muestra `"link":false` y los modos autonomos mantienen los motores
   detenidos (timeout demostrado antes de existir el enlace).
3. **Enlazados**: `/status` muestra datos vivos; desconectar el cable TX → los
   motores se detienen en ≤ 500 ms y el LED parpadea rojo; reconectar → reanuda.
4. **Modos en seco**: FIND gira en el sitio → mostrar el identificador → carga
   → ocultarlo ~2 s → vuelve a escanear. Cinta cerca del lente → retroceso.
   RETREAT: verificar visualmente la secuencia 180° → avance → 90° → borde.

En el ring: PATROL 3 vueltas sin pisar la cinta; RETREAT desde el centro
termina detenido en una esquina; FIND con el objetivo dentro del ring
(escaneo→carga→retroceso→re-escaneo); objetivo FUERA del ring → el robot debe
frenar en la cinta y no cruzarla jamas.

## Calibracion (constantes a ajustar)

| Constante | Archivo | Valor inicial | Nota |
|---|---|---|---|
| `EDGE_ABS_THR` | `robot_cam/main/detection_responder.cc` | 130 | Umbral de luminancia de la cinta; recalibrar con la iluminacion real del ring |
| `TURN_180_MS` | `robot_s3/main/robot_state.h` | 1200 | Giro ~180°; depende de bateria y piso |
| `TURN_90_MS` | `robot_s3/main/robot_state.h` | 600 | Giro ~90° |
| `BACKOFF_MS` | `robot_s3/main/robot_state.h` | 400 | Retroceso tras tocar borde |
| `IDENT_CONF_THR` | `robot_s3/main/robot_state.h` | 75 | Subir si hay falsos positivos del modelo |
| `INFERENCE_EVERY_N` | `robot_cam/main/main_functions.cc` | 1 | Subir a 2–3 si `Invoke` tarda > ~400 ms (se loguea al arranque) para refrescar el borde mas rapido |

Otros puntos sensibles:

- **Angulo de montaje de la camara**: la mitad inferior del frame debe ver el
  piso a 10–30 cm; la superior, el horizonte (el mismo frame sirve para borde
  e identificador). Las bandas de filas se ajustan en `detection_responder.cc`.
- **Brownout de la CAM**: motores al 100 % en bloqueo pueden hundir los 5 V y
  reiniciar la CAM (el S3 frena solo por timeout). Agregar condensador o
  regulador 5 V dedicado si aparecen reinicios.
