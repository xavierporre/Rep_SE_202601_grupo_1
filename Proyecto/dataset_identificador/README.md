# dataset_identificador — recapturar y reentrenar el modelo del identificador

## Por que existe esta carpeta

El robot nunca tuvo realmente desplegado un modelo entrenado para el objeto
identificador: `Proyecto/robot_cam` sigue corriendo el modelo de ejemplo de
deteccion de personas de TFLite Micro (mismo archivo binario, mismo MD5). El
modelo "embutido" que se entreno en `Lab_2/modelos/mi_modelo_keras.h5` nunca
se convirtio ni se copio al firmware, y al evaluarlo igual resulto con muy
mala precision (~58-68%, casi azar) porque el dataset de 410 imagenes
(`Lab_3/projects_tf/identificador/imagenes_output/`) tiene muy baja calidad
(sobreexposicion, motion blur, ruido).

Esta carpeta junta todo lo necesario para: capturar un dataset nuevo y mejor
con la propia ESP32-CAM, combinarlo con las imagenes viejas, reentrenar, y
desplegar un modelo real en `robot_cam`.

## Contenido

- `cam_capture/` — firmware standalone para la ESP32-CAM. Solo captura
  frames 96x96 en escala de grises a pedido y los manda por UART0 (el mismo
  cable USB-TTL del flasheo) como texto hex. No toca `robot_cam` ni corre
  ningun modelo.
- `capture_tool.py` — script de host (tu computador) que pide la etiqueta
  (0/1), le pide una foto a la placa, y la guarda en `dataset/imagenes/`.
- `dataset/imagenes/` — todas las imagenes etiquetadas (las 410 viejas ya
  copiadas + las que captures ahora), formato `NNNNN_label0.png` /
  `NNNNN_label1.png` (label1 = identificador presente).
- `train_identificador.py` — reentrena un modelo CNN liviano con el dataset
  combinado, separando train/test de forma estratificada, con augmentation
  y guardando el mejor checkpoint (no el de la ultima epoca).
- `quantize_and_export.py` — cuantiza el modelo entrenado a int8 y genera
  `identificador_model_data.cc/.h` listos para copiar a `robot_cam/main/`.
- `modelo/` — se crea sola al entrenar/cuantizar (pesos, .h5, .tflite, .cc/.h).

## Paso 0 (opcional) — Diagnóstico de sesgo de fondo

Antes de capturar nuevas fotos, puedes confirmar visualmente si el modelo está
mirando el objeto o el fondo con GradCAM:

```bash
python3 diagnostico_sesgo.py
```

Genera mapas de calor superpuestos sobre 8 imágenes `label0` y 8 `label1`
aleatorias y los guarda en `modelo/gradcam/`. Colores:
- **Rojo/amarillo** = zona de alta activación (lo que el modelo "mira")
- **Azul** = zona ignorada

Si el calor aparece en el fondo y no sobre el identificador, el sesgo queda
confirmado y conviene priorizar la captura de nuevas `label1` con fondos
variados (ver Paso 1).

Requiere el modelo entrenado (`modelo/identificador_model.h5`) y `matplotlib`:

```bash
pip install matplotlib   # además de tensorflow pillow numpy
```

## Paso 1 — Flashear y capturar fotos nuevas

```bash
cd cam_capture
idf.py set-target esp32
idf.py -p <PUERTO_FTDI_CAM> build flash
```

Luego, en otra terminal (con `pyserial` y `pillow` instalados: `pip install
pyserial pillow`), **sin** dejar abierto `idf.py monitor` al mismo tiempo
(ambos pelean por el puerto):

```bash
python3 capture_tool.py --port <PUERTO_FTDI_CAM>
```

El script te pregunta la etiqueta antes de cada foto:
- `1` = el identificador esta visible frente a la camara
- `0` = no esta visible (fondo, piso, otra cosa)
- `q` = salir

Recomendaciones para que el dataset sirva de verdad (a diferencia del
anterior):
- Camara **quieta** al momento de la foto (sin motion blur).
- Iluminacion uniforme, sin contraluz de ventana ni saturacion de blancos.
- Variar **distancia** (incluyendo la distancia real esperada en combate),
  **angulo** y **fondo** entre fotos, para que el modelo generalice.
- Apuntar a varias decenas/cientos de fotos de cada clase (mientras mas,
  mejor; el dataset viejo aporta 410 de baja calidad, conviene que las
  nuevas de buena calidad sean varios cientos tambien).

## Paso 2 — Reentrenar

Necesitas un Python con TensorFlow que **funcione** (en Apple Silicon, usar
un Python arm64 nativo — Homebrew `python3.12`, no el `python3` de Anaconda
corriendo bajo Rosetta, que falla al importar `tensorflow.lite`):

```bash
/opt/homebrew/bin/python3.12 -m venv .venv
source .venv/bin/activate
pip install tensorflow pillow numpy
python3 train_identificador.py
```

Al final imprime el accuracy real en el test set (held-out, nunca visto en
entrenamiento). Si es menor a ~85%, el script avisa que no conviene
desplegar todavia — conviene capturar mas fotos variadas y reentrenar antes
de seguir.

## Paso 3 — Cuantizar y desplegar

Si el accuracy fue bueno:

```bash
python3 quantize_and_export.py
```

Esto genera `modelo/identificador_model_data.cc` y `.h`. El script verifica
que la cuantizacion de entrada (`scale`/`zero_point`) sea compatible con la
conversion que `image_provider.cc` ya hace (uint8 - 128); si lo es (deberia,
porque ambos asumen pixel/255.0 en \[0,1\]), no hace falta tocar
`image_provider.cc`.

Luego, a mano:

1. Copiar `modelo/identificador_model_data.cc` y `.h` a
   `Proyecto/robot_cam/main/` (puedes borrar `person_detect_model_data.cc/.h`
   o dejarlos sin usar).
2. En `Proyecto/robot_cam/main/main_functions.cc`:
   - Cambiar `#include "person_detect_model_data.h"` por
     `#include "identificador_model_data.h"`.
   - Cambiar `tflite::GetModel(g_person_detect_model_data)` por
     `tflite::GetModel(g_identificador_model_data)`.
3. En `Proyecto/robot_cam/main/CMakeLists.txt`, cambiar
   `"person_detect_model_data.cc"` por `"identificador_model_data.cc"` en la
   lista de `SRCS`.
4. `idf.py build` y flashear `robot_cam` normalmente.

## Notas

- `model_settings.h` ya tiene `kPersonIndex`/`kNotAPersonIndex` renombrados
  como comentario a "embutido"/"no_embutido" — son solo nombres de variable,
  no hace falta tocarlos (el indice 1 sigue siendo "clase positiva").
- Si tras desplegar el nuevo modelo el `conf` sigue bajo en campo real,
  repite el Paso 1 capturando mas variedad (sobre todo a la distancia real
  de combate) antes de tocar umbrales de nuevo.
