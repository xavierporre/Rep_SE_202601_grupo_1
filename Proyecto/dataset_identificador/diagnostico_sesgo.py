#!/usr/bin/env python3
"""
diagnostico_sesgo.py — Visualiza dónde mira el modelo con GradCAM.

Carga el modelo entrenado (.h5) y genera mapas de calor sobre las imágenes
del test set para confirmar si el modelo mira el objeto o el fondo.

Si los mapas de calor se concentran en el fondo (y no sobre el identificador),
la hipótesis de sesgo de fondo queda confirmada.

Uso:
    python3 diagnostico_sesgo.py

Salida:
    modelo/gradcam/  ← PNG con mapa de calor superpuesto para cada imagen
                       Nombre: NNNNN_label1_grad.png  /  NNNNN_label0_grad.png

Requisitos: pip install tensorflow pillow numpy matplotlib
"""

import os
import sys
import numpy as np
import tensorflow as tf
from PIL import Image
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.cm as cm

SEED = 42
tf.random.set_seed(SEED)
np.random.seed(SEED)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
IMG_DIR  = os.path.join(BASE_DIR, "dataset", "imagenes")
OUT_DIR  = os.path.join(BASE_DIR, "modelo", "gradcam")
MODEL_H5 = os.path.join(BASE_DIR, "modelo", "identificador_model.h5")

IMG_W = IMG_H = 96
LAST_CONV = "conv2d_2"      # nombre de la última capa conv en make_model()
N_LABEL0  = 8               # cuántas label0 analizar
N_LABEL1  = 8               # cuántas label1 analizar


def load_samples(n0=N_LABEL0, n1=N_LABEL1, seed=SEED):
    rng = np.random.RandomState(seed)
    files = sorted(f for f in os.listdir(IMG_DIR) if f.endswith(".png"))
    l0 = [f for f in files if "_label0" in f]
    l1 = [f for f in files if "_label1" in f]
    # tomar un subconjunto al azar para no sesgar la selección hacia índices bajos
    rng.shuffle(l0); rng.shuffle(l1)
    selected = [(f, 0) for f in l0[:n0]] + [(f, 1) for f in l1[:n1]]
    imgs, labels, names = [], [], []
    for fname, lbl in selected:
        img = Image.open(os.path.join(IMG_DIR, fname)).convert("L").resize((IMG_W, IMG_H))
        arr = np.array(img, dtype=np.float32) / 255.0
        imgs.append(arr.reshape(IMG_H, IMG_W, 1))
        labels.append(lbl)
        names.append(fname)
    return np.array(imgs, dtype=np.float32), np.array(labels), names


def gradcam(model, image, pred_class):
    """Devuelve mapa de calor (H×W float [0,1]) para la clase predicha."""
    inp = tf.cast(image[None], tf.float32)           # (1, H, W, 1)
    conv_out = None

    with tf.GradientTape() as tape:
        x = inp
        for layer in model.layers:
            x = layer(x)
            if layer.name == LAST_CONV:
                conv_out = x
                tape.watch(conv_out)                 # registrar ANTES de capas siguientes
        preds = x
        loss = preds[:, pred_class]

    grads = tape.gradient(loss, conv_out)            # (1, h, w, C)
    pooled = tf.reduce_mean(grads, axis=(1, 2))      # (1, C)
    heatmap = tf.reduce_sum(conv_out[0] * pooled[0], axis=-1)  # (h, w)
    heatmap = tf.nn.relu(heatmap)
    heatmap = heatmap / (tf.reduce_max(heatmap) + 1e-8)
    return heatmap.numpy()


def overlay(image_gray_01, heatmap):
    """Combina imagen grayscale y mapa de calor en RGB PIL."""
    img_rgb = np.stack([image_gray_01] * 3, axis=-1)         # (H,W,3) float
    heat_resized = np.array(
        Image.fromarray((heatmap * 255).astype(np.uint8)).resize(
            (IMG_W, IMG_H), Image.BILINEAR
        )
    ) / 255.0                                                 # (H,W) float
    colormap = matplotlib.colormaps["jet"]
    heat_rgb = colormap(heat_resized)[:, :, :3]               # (H,W,3)
    blended = 0.55 * img_rgb + 0.45 * heat_rgb
    blended = np.clip(blended, 0.0, 1.0)
    return Image.fromarray((blended * 255).astype(np.uint8))


def main():
    if not os.path.exists(MODEL_H5):
        sys.exit(f"No se encontró el modelo en {MODEL_H5}\n"
                 "Ejecuta primero train_identificador.py.")

    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"Cargando modelo desde {MODEL_H5} ...")
    model = tf.keras.models.load_model(MODEL_H5)

    try:
        model.get_layer(LAST_CONV)
    except ValueError:
        # Mostrar capas disponibles si el nombre cambió
        conv_layers = [l.name for l in model.layers if "conv" in l.name]
        sys.exit(f"Capa '{LAST_CONV}' no encontrada. Capas conv disponibles: {conv_layers}\n"
                 f"Actualiza la variable LAST_CONV en este script.")

    images, labels, names = load_samples()
    preds_all = model.predict(images, verbose=0)
    pred_labels = np.argmax(preds_all, axis=1)
    confidences = np.max(preds_all, axis=1)

    print(f"\nGenerando {len(images)} mapas de calor en {OUT_DIR}/")
    for i, (img, true_lbl, fname) in enumerate(zip(images, labels, names)):
        pred_lbl = int(pred_labels[i])
        conf = float(confidences[i])
        hmap = gradcam(model, img, pred_lbl)
        vis = overlay(img[:, :, 0], hmap)

        correct = "OK" if pred_lbl == true_lbl else "ERR"
        stem = os.path.splitext(fname)[0]
        out_name = f"{stem}_pred{pred_lbl}_conf{int(conf*100):03d}_{correct}_grad.png"
        vis.save(os.path.join(OUT_DIR, out_name))

    # Resumen de errores de clasificación en la muestra analizada
    n_err = int(np.sum(pred_labels != labels))
    print(f"\nResumen de la muestra analizada:")
    print(f"  Correctas : {len(images) - n_err}/{len(images)}")
    print(f"  Errores   : {n_err}/{len(images)}")
    print(f"\nMapas guardados en {OUT_DIR}/")
    print("\nInterpretación:")
    print("  - Rojo/amarillo = zona de alta activación (lo que el modelo 'mira')")
    print("  - Azul = zona de baja activación (ignorada por el modelo)")
    print("  - Si el calor está en el FONDO y no en el objeto → sesgo de fondo confirmado")
    print("  - Archivos con sufijo _ERR son predicciones incorrectas")


if __name__ == "__main__":
    main()
