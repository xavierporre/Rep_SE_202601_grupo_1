#!/usr/bin/env python3
"""
quantize_and_export.py — cuantiza modelo/identificador_model.h5 a TFLite
int8 y genera identificador_model_data.cc/.h listos para copiar a
Proyecto/robot_cam/main/.

Usa imagenes reales del dataset (en [0,1] float, igual que en entrenamiento)
como representative_dataset, para que la cuantizacion resultante sea
compatible con la conversion XOR 0x80 que ya hace image_provider.cc
(uint8 0..255 -> int8 restando 128), es decir scale ~ 1/255, zero_point ~ -128.

Uso:
    python3 quantize_and_export.py
"""

import os
import sys
import numpy as np
import tensorflow as tf
from PIL import Image

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
IMG_DIR = os.path.join(BASE_DIR, "dataset", "imagenes")
MODEL_PATH = os.path.join(BASE_DIR, "modelo", "identificador_model.h5")
OUT_DIR = os.path.join(BASE_DIR, "modelo")

IMG_W = IMG_H = 96
ARRAY_NAME = "g_identificador_model_data"


def representative_dataset():
    files = sorted(f for f in os.listdir(IMG_DIR) if f.endswith(".png"))
    rng = np.random.RandomState(0)
    rng.shuffle(files)
    for f in files[:150]:
        img = Image.open(os.path.join(IMG_DIR, f)).convert("L").resize((IMG_W, IMG_H))
        arr = np.array(img, dtype=np.float32) / 255.0
        arr = arr.reshape(1, IMG_H, IMG_W, 1)
        yield [arr]


def main():
    if not os.path.exists(MODEL_PATH):
        sys.exit(f"No existe {MODEL_PATH}. Corre train_identificador.py primero.")

    model = tf.keras.models.load_model(MODEL_PATH)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    tflite_path = os.path.join(OUT_DIR, "identificador_model.tflite")
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)
    print(f"TFLite int8 guardado: {tflite_path} ({len(tflite_model):,} bytes)")

    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    in_detail = interpreter.get_input_details()[0]
    out_detail = interpreter.get_output_details()[0]
    in_scale, in_zero = in_detail["quantization"]
    print(f"Input  shape={in_detail['shape']} dtype={in_detail['dtype']} "
          f"scale={in_scale:.6f} zero_point={in_zero}")
    print(f"Output shape={out_detail['shape']} dtype={out_detail['dtype']}")

    expected_scale = 1.0 / 255.0
    if abs(in_scale - expected_scale) > 0.0015 or in_zero != -128:
        print("\nAVISO: la cuantizacion de entrada no coincide exactamente con "
              f"scale={expected_scale:.6f} zero_point=-128 (la convencion uint8-128 "
              "que usa image_provider.cc). Revisa si hace falta ajustar el "
              "preprocesamiento en robot_cam antes de desplegar este modelo.")
    else:
        print("\nOK: la cuantizacion de entrada es compatible con la conversion "
              "XOR 0x80 existente en image_provider.cc, no hace falta tocarla.")

    # Generar C array (equivalente a xxd -i)
    data = tflite_model
    chunks = [data[i:i + 12] for i in range(0, len(data), 12)]
    lines = [",".join(f"0x{b:02x}" for b in chunk) for chunk in chunks]
    hex_block = ",\n  ".join(lines)

    cc_content = f"""// Generado automaticamente por quantize_and_export.py
// Modelo: identificador_model.h5 -> TFLite int8

#include "identificador_model_data.h"

alignas(8) const unsigned char {ARRAY_NAME}[] = {{
  {hex_block}
}};
const int {ARRAY_NAME}_len = {len(data)};
"""

    h_content = f"""// Generado automaticamente por quantize_and_export.py
#ifndef IDENTIFICADOR_MODEL_DATA_H_
#define IDENTIFICADOR_MODEL_DATA_H_

extern const unsigned char {ARRAY_NAME}[];
extern const int {ARRAY_NAME}_len;

#endif  // IDENTIFICADOR_MODEL_DATA_H_
"""

    cc_path = os.path.join(OUT_DIR, "identificador_model_data.cc")
    h_path = os.path.join(OUT_DIR, "identificador_model_data.h")
    with open(cc_path, "w") as f:
        f.write(cc_content)
    with open(h_path, "w") as f:
        f.write(h_content)

    print(f"\nGenerado:\n  {cc_path}\n  {h_path}")
    print(f"\nSiguiente paso: copiar ambos archivos a Proyecto/robot_cam/main/, "
          f"reemplazando person_detect_model_data.cc/.h, y actualizar "
          f"main_functions.cc para incluir identificador_model_data.h y usar "
          f"{ARRAY_NAME} en vez de g_person_detect_model_data.")


if __name__ == "__main__":
    main()
