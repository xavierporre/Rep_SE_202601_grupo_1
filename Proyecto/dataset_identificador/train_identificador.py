#!/usr/bin/env python3
"""
train_identificador.py — reentrena el modelo del identificador usando el
dataset combinado (imagenes viejas + nuevas capturadas con cam_capture).

Lee todas las imagenes en dataset/imagenes/*.png (formato NNNNN_label0.png /
NNNNN_label1.png), separa train/test de forma estratificada, entrena una
CNN liviana con augmentation y early stopping (guardando el mejor checkpoint
por accuracy de validacion), y deja el modelo listo para cuantizar.

Requisitos: pip install tensorflow pillow numpy
(en Apple Silicon usar un Python arm64 nativo, no x86_64 vía Rosetta/Anaconda,
o tensorflow falla al importar tensorflow.lite)

Uso:
    python3 train_identificador.py
"""

import os
import sys
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models, regularizers
from PIL import Image

SEED = 42
tf.random.set_seed(SEED)
np.random.seed(SEED)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
IMG_DIR = os.path.join(BASE_DIR, "dataset", "imagenes")
OUT_DIR = os.path.join(BASE_DIR, "modelo")
os.makedirs(OUT_DIR, exist_ok=True)

IMG_W = IMG_H = 96


def load_dataset():
    files = sorted(f for f in os.listdir(IMG_DIR) if f.endswith(".png"))
    if not files:
        sys.exit(f"No se encontraron imagenes en {IMG_DIR}")

    X, y = [], []
    for f in files:
        if "_label1" in f:
            label = 1
        elif "_label0" in f:
            label = 0
        else:
            continue
        img = Image.open(os.path.join(IMG_DIR, f)).convert("L").resize((IMG_W, IMG_H))
        arr = np.array(img, dtype=np.float32) / 255.0
        X.append(arr.reshape(IMG_H, IMG_W, 1))
        y.append(label)

    X = np.array(X, dtype=np.float32)
    y = np.array(y, dtype=np.int64)
    print(f"Total imagenes: {len(y)}  (label0={np.sum(y==0)}  label1={np.sum(y==1)})")
    return X, y


def stratified_split(y, frac=0.85, seed=SEED):
    rng = np.random.RandomState(seed)
    idx0 = np.where(y == 0)[0]
    idx1 = np.where(y == 1)[0]
    rng.shuffle(idx0)
    rng.shuffle(idx1)

    def split(idx):
        n = int(len(idx) * frac)
        return idx[:n], idx[n:]

    tr0, te0 = split(idx0)
    tr1, te1 = split(idx1)
    train_idx = np.concatenate([tr0, tr1])
    test_idx = np.concatenate([te0, te1])
    rng.shuffle(train_idx)
    rng.shuffle(test_idx)
    return train_idx, test_idx


def make_model():
    l2 = regularizers.l2(1e-4)
    return models.Sequential([
        layers.Conv2D(16, (3, 3), padding="same", activation="relu",
                       kernel_regularizer=l2, input_shape=(IMG_H, IMG_W, 1)),
        layers.BatchNormalization(),
        layers.MaxPooling2D((2, 2)),

        layers.Conv2D(32, (3, 3), padding="same", activation="relu", kernel_regularizer=l2),
        layers.BatchNormalization(),
        layers.MaxPooling2D((2, 2)),

        layers.Conv2D(64, (3, 3), padding="same", activation="relu", kernel_regularizer=l2),
        layers.BatchNormalization(),
        layers.MaxPooling2D((2, 2)),

        layers.GlobalAveragePooling2D(),
        layers.Dropout(0.5),
        layers.Dense(32, activation="relu", kernel_regularizer=l2),
        layers.Dense(2, activation="softmax"),
    ])


def main():
    X, y = load_dataset()
    if len(y) < 200:
        print(f"AVISO: solo hay {len(y)} imagenes. Se recomienda tener varios cientos "
              "(idealmente >600-800) y buena variedad de angulos/distancias/iluminacion "
              "antes de confiar en el resultado.")

    train_idx, test_idx = stratified_split(y)
    X_train, y_train = X[train_idx], y[train_idx]
    X_test, y_test = X[test_idx], y[test_idx]
    print(f"Train: {len(y_train)}  Test: {len(y_test)}")

    model = make_model()
    model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=5e-4),
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])
    model.summary()

    data_aug = tf.keras.Sequential([
        layers.RandomFlip("horizontal"),
        layers.RandomRotation(0.15),                              # ±54° (antes 0.04)
        layers.RandomZoom(0.20),                                  # ±20% (antes 0.08)
        layers.RandomTranslation(0.10, 0.10),                     # ±10% desplazamiento
        layers.RandomBrightness(0.15, value_range=(0.0, 1.0)),
        layers.RandomContrast(0.15),
    ])

    def apply_cutout(image, cutout_frac=0.25):
        """Oculta un parche aleatorio para forzar invarianza de fondo."""
        H = tf.shape(image)[0]
        W = tf.shape(image)[1]
        cut_h = tf.cast(tf.cast(H, tf.float32) * cutout_frac, tf.int32)
        cut_w = tf.cast(tf.cast(W, tf.float32) * cutout_frac, tf.int32)
        y0 = tf.random.uniform((), 0, H - cut_h + 1, dtype=tf.int32)
        x0 = tf.random.uniform((), 0, W - cut_w + 1, dtype=tf.int32)
        rows = tf.range(H)
        cols = tf.range(W)
        mask = tf.cast(
            ~((rows >= y0)[:, None] & (rows < y0 + cut_h)[:, None] &
              (cols >= x0)[None, :] & (cols < x0 + cut_w)[None, :]),
            tf.float32
        )[:, :, None]
        return image * mask

    train_ds = tf.data.Dataset.from_tensor_slices((X_train, y_train))
    train_ds = train_ds.shuffle(len(X_train), seed=SEED).batch(16)
    train_ds = train_ds.map(lambda x, l: (data_aug(x, training=True), l))
    train_ds = train_ds.map(lambda x, l: (tf.vectorized_map(apply_cutout, x), l))
    train_ds = train_ds.prefetch(tf.data.AUTOTUNE)

    val_ds = tf.data.Dataset.from_tensor_slices((X_test, y_test)).batch(16)

    ckpt_path = os.path.join(OUT_DIR, "best.weights.h5")
    callbacks = [
        tf.keras.callbacks.EarlyStopping(monitor="val_accuracy", patience=20, restore_best_weights=True),
        tf.keras.callbacks.ModelCheckpoint(ckpt_path, monitor="val_accuracy", save_best_only=True, save_weights_only=True),
    ]

    model.fit(train_ds, validation_data=val_ds, epochs=80, callbacks=callbacks, verbose=2)
    model.load_weights(ckpt_path)

    print("\n=== Evaluacion final en test set (held-out) ===")
    loss, acc = model.evaluate(val_ds, verbose=0)
    print(f"Test accuracy: {acc:.4f}  Test loss: {loss:.4f}")

    preds = model.predict(X_test, verbose=0)
    pred_labels = np.argmax(preds, axis=1)
    tp = int(((pred_labels == 1) & (y_test == 1)).sum())
    tn = int(((pred_labels == 0) & (y_test == 0)).sum())
    fp = int(((pred_labels == 1) & (y_test == 0)).sum())
    fn = int(((pred_labels == 0) & (y_test == 1)).sum())
    print(f"TP={tp} TN={tn} FP={fp} FN={fn}")

    model_path = os.path.join(OUT_DIR, "identificador_model.h5")
    model.save(model_path)
    print(f"\nModelo guardado en {model_path}")

    if acc < 0.85:
        print("\nAVISO: accuracy en test < 85%. No se recomienda desplegar este modelo "
              "todavia: captura mas imagenes variadas con capture_tool.py y reentrena "
              "antes de cuantizar/desplegar en robot_cam.")
    else:
        print("\nListo para cuantizar: usa quantize_and_export.py para generar "
              "identificador_model_data.cc/.h")


if __name__ == "__main__":
    main()
