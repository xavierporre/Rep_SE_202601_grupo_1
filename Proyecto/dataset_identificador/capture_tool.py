#!/usr/bin/env python3
"""
capture_tool.py — herramienta de captura de dataset para el identificador.

Se conecta a la ESP32-CAM corriendo el firmware cam_capture/ por el mismo
cable USB-TTL usado para flashear, y por cada foto:
  1. Pide al usuario la etiqueta (0 = sin identificador, 1 = con identificador)
  2. Envia 'c' a la placa para pedir una captura
  3. Recibe el frame 96x96 en escala de grises (hex) y lo guarda como PNG
     en dataset/imagenes/, con la misma convencion de nombres que el dataset
     viejo (NNNNN_label0.png / NNNNN_label1.png) para que ambos se puedan
     usar juntos al entrenar.

Requisitos: pip install pyserial pillow

Uso:
    python3 capture_tool.py --port /dev/cu.usbserial-XXXX
"""

import argparse
import os
import sys
import time

import serial
from PIL import Image

IMG_W = 96
IMG_H = 96
IMG_BYTES = IMG_W * IMG_H

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATASET_DIR = os.path.join(BASE_DIR, "dataset", "imagenes")


def next_index(dataset_dir):
    os.makedirs(dataset_dir, exist_ok=True)
    existing = [f for f in os.listdir(dataset_dir) if f.endswith(".png")]
    if not existing:
        return 0
    indices = []
    for f in existing:
        try:
            indices.append(int(f.split("_")[0]))
        except ValueError:
            continue
    return (max(indices) + 1) if indices else 0


def read_until(ser, marker, timeout=5.0):
    """Lee lineas hasta encontrar `marker` o agotar el timeout. Devuelve la linea encontrada o None."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("utf-8", errors="ignore").strip()
        if text:
            print(f"  [cam] {text}" if text != marker else "")
        if marker in text:
            return text
    return None


def capture_one(ser):
    ser.reset_input_buffer()
    # La consola UART de ESP-IDF es line-buffered por defecto: el firmware no
    # ve el caracter hasta que llega un '\n'.
    ser.write(b"c\n")

    start = read_until(ser, "---IMG-START---", timeout=5.0)
    if start is None:
        print("  [ERROR] no llego ---IMG-START--- (revisa la conexion/firmware)")
        return None

    hex_line = ser.readline().decode("utf-8", errors="ignore").strip()
    end = ser.readline().decode("utf-8", errors="ignore").strip()
    if "---IMG-END---" not in end:
        print(f"  [ERROR] no llego ---IMG-END--- correctamente (recibido: {end!r})")
        return None

    if len(hex_line) != IMG_BYTES * 2:
        print(f"  [ERROR] tamano inesperado: {len(hex_line)} caracteres hex (esperaba {IMG_BYTES*2})")
        return None

    raw = bytes.fromhex(hex_line)
    img = Image.frombytes("L", (IMG_W, IMG_H), raw)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Puerto serie de la ESP32-CAM (ej. /dev/cu.usbserial-XXXX)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default=DATASET_DIR, help="Carpeta de salida para las imagenes")
    args = ap.parse_args()

    print(f"Conectando a {args.port} @ {args.baud}...")
    ser = serial.Serial(args.port, args.baud, timeout=2)
    time.sleep(2.0)  # esperar reset de la placa al abrir el puerto
    ser.reset_input_buffer()

    idx = next_index(args.out)
    print(f"Carpeta de salida: {args.out}")
    print(f"Siguiente indice: {idx:05d}")
    print()
    print("Coloca el objeto identificador frente a la camara (o quitalo) y presiona Enter.")
    print("Etiquetas: 1 = CON identificador visible, 0 = SIN identificador, q = salir\n")

    count0 = count1 = 0
    try:
        while True:
            label = input(f"[{idx:05d}] Etiqueta (0/1/q): ").strip().lower()
            if label == "q":
                break
            if label not in ("0", "1"):
                print("  Ingresa 0, 1 o q.")
                continue

            img = capture_one(ser)
            if img is None:
                print("  Captura fallida, intenta de nuevo.")
                continue

            fname = f"{idx:05d}_label{label}.png"
            path = os.path.join(args.out, fname)
            img.save(path)
            print(f"  Guardado: {fname}")

            idx += 1
            if label == "0":
                count0 += 1
            else:
                count1 += 1
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    print(f"\nListo. Nuevas capturas: {count0} label0, {count1} label1.")


if __name__ == "__main__":
    main()
