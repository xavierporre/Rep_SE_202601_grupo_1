#!/usr/bin/env python3
"""
prepare_project.py
Convierte mi_modelo_keras.h5 a TFLite float32, genera el C array,
y prepara las 10 imágenes estáticas (5 label0 + 5 label1) como raw binarios.
"""

import os
import sys
import struct
import shutil
import numpy as np

BASE   = os.path.dirname(os.path.abspath(__file__))
MODELOS_DIR = os.path.join(BASE, "..", "..", "..", "Lab_2", "modelos")
KERAS_MODEL = os.path.join(MODELOS_DIR, "mi_modelo_keras.h5")
IMGS_DIR    = os.path.join(BASE, "imagenes_output")
STATIC_DIR  = os.path.join(BASE, "static_images", "sample_images")
MAIN_DIR    = os.path.join(BASE, "main")
TFLITE_PATH = os.path.join(BASE, "embutido_model.tflite")

os.makedirs(STATIC_DIR, exist_ok=True)
os.makedirs(MAIN_DIR, exist_ok=True)

# ─── Paso 1: Convertir Keras → TFLite float32 ────────────────────────────────
print("\n[1] Convirtiendo modelo Keras → TFLite float32 …")
try:
    import tensorflow as tf
    print(f"    TensorFlow {tf.__version__}")
    model = tf.keras.models.load_model(KERAS_MODEL)
    model.summary()

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    # float32: sin cuantización, compatible directo con TFLite Micro
    tflite_model = converter.convert()

    with open(TFLITE_PATH, "wb") as f:
        f.write(tflite_model)
    print(f"    TFLite guardado: {TFLITE_PATH}  ({len(tflite_model):,} bytes)")

except ImportError:
    print("    [ERROR] tensorflow no disponible. Instala con: pip install tensorflow")
    sys.exit(1)
except Exception as e:
    print(f"    [ERROR] Conversión fallida: {e}")
    sys.exit(1)

# ─── Paso 2: Generar C array (equivalente a xxd -i) ──────────────────────────
print("\n[2] Generando embutido_model_data.cc / .h …")

with open(TFLITE_PATH, "rb") as f:
    data = f.read()

# Formatear como array C
hex_values = ", ".join(f"0x{b:02x}" for b in data)
# Dividir en líneas de 12 valores
chunks = [data[i:i+12] for i in range(0, len(data), 12)]
lines  = [",".join(f"0x{b:02x}" for b in chunk) for chunk in chunks]
hex_block = ",\n  ".join(lines)

cc_content = f"""// Generado automáticamente por prepare_project.py
// Modelo: mi_modelo_keras.h5 → TFLite float32

#include "embutido_model_data.h"

// Array del modelo TFLite
alignas(8) const unsigned char g_embutido_model_data[] = {{
  {hex_block}
}};
const int g_embutido_model_data_len = {len(data)};
"""

h_content = """// Generado automáticamente por prepare_project.py
#ifndef EMBUTIDO_MODEL_DATA_H_
#define EMBUTIDO_MODEL_DATA_H_

extern const unsigned char g_embutido_model_data[];
extern const int g_embutido_model_data_len;

#endif  // EMBUTIDO_MODEL_DATA_H_
"""

cc_path = os.path.join(MAIN_DIR, "embutido_model_data.cc")
h_path  = os.path.join(MAIN_DIR, "embutido_model_data.h")

with open(cc_path, "w") as f:
    f.write(cc_content)
with open(h_path, "w") as f:
    f.write(h_content)

print(f"    {cc_path}")
print(f"    {h_path}")

# ─── Paso 3: Verificar ops del modelo ────────────────────────────────────────
print("\n[3] Verificando ops del modelo TFLite …")
interpreter = tf.lite.Interpreter(model_path=TFLITE_PATH)
interpreter.allocate_tensors()
input_details  = interpreter.get_input_details()
output_details = interpreter.get_output_details()
print(f"    Input  shape: {input_details[0]['shape']}  dtype: {input_details[0]['dtype']}")
print(f"    Output shape: {output_details[0]['shape']}  dtype: {output_details[0]['dtype']}")

ops_set = set()
for d in interpreter.get_tensor_details():
    pass  # just checking allocation worked
# Get ops via _get_ops_details if available
try:
    ops = interpreter._get_ops_details()
    for op in ops:
        ops_set.add(op['op_name'])
    print(f"    Ops: {sorted(ops_set)}")
except:
    print("    (no se pudieron listar ops directamente)")

# ─── Paso 4: Seleccionar y guardar 10 imágenes estáticas ─────────────────────
print("\n[4] Seleccionando imágenes estáticas (5 label0 + 5 label1) …")

from PIL import Image

all_imgs = sorted(os.listdir(IMGS_DIR))
label0 = [f for f in all_imgs if "_label0" in f]
label1 = [f for f in all_imgs if "_label1" in f]

# Seleccionar 5 de cada clase
selected_label0 = label0[:5]
selected_label1 = label1[:5]

# Orden: imagen 0-4 = label0, imagen 5-9 = label1
selected = selected_label0 + selected_label1
labels   = [0]*5 + [1]*5

print(f"    Label 0 (no embutido): {selected_label0}")
print(f"    Label 1 (embutido):    {selected_label1}")

# Guardar registro de qué imágenes se eligieron
selected_info_path = os.path.join(BASE, "selected_images.txt")
with open(selected_info_path, "w") as f:
    for i, (fname, lbl) in enumerate(zip(selected, labels)):
        f.write(f"image{i}\t{fname}\tlabel{lbl}\n")
        print(f"    image{i} ← {fname}  (label{lbl})")

# Guardar como raw uint8 96×96
for i, fname in enumerate(selected):
    img = Image.open(os.path.join(IMGS_DIR, fname)).convert("L").resize((96,96))
    arr = np.array(img, dtype=np.uint8)
    assert arr.shape == (96, 96), f"Error tamaño: {arr.shape}"
    out_path = os.path.join(STATIC_DIR, f"image{i}")
    with open(out_path, "wb") as f:
        f.write(arr.tobytes())
    print(f"    → {out_path}  ({arr.nbytes} bytes)")

print(f"\n[OK] Preparación completada.")
print(f"     Modelo TFLite: {len(data):,} bytes")
print(f"     Imágenes:      {len(selected)} archivos raw 96x96 uint8")
print(f"\n     Siguiente paso: idf.py build")
