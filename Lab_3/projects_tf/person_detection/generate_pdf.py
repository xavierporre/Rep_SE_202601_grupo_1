#!/usr/bin/env python3
"""
generate_pdf.py
Captura los resultados de inferencia del ESP32-S3 por serial,
reconstruye las imágenes y genera output.pdf con imagen + resultado.
"""

import serial
import time
import re
import sys
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

# ── Configuración ──────────────────────────────────────────────────────────────
PORT        = "/dev/cu.usbmodem101"
BAUD        = 115200
TIMEOUT_S   = 120          # segundos esperando salida del ESP32
IMG_W, IMG_H = 96, 96

STATIC_IMAGES_DIR = os.path.join(os.path.dirname(__file__),
                                 "static_images", "sample_images")
FOTOS_DIR = os.path.join(os.path.dirname(__file__),
                         "..", "..", "..", "fotos")
OUTPUT_PDF = os.path.join(os.path.dirname(__file__), "output.pdf")

# ── Carga de imágenes ──────────────────────────────────────────────────────────

def load_static_image(idx: int) -> np.ndarray:
    """Lee imagen estática (96x96 uint8 raw) y devuelve array 2D."""
    path = os.path.join(STATIC_IMAGES_DIR, f"image{idx}")
    data = np.frombuffer(open(path, "rb").read(), dtype=np.uint8)
    return data.reshape(IMG_H, IMG_W)


def parse_cc_int8(path: str) -> np.ndarray:
    """Extrae el array int8 de un archivo .cc generado por el grupo."""
    with open(path, "r") as f:
        text = f.read()
    # Buscar todos los números (con signo) dentro del inicializador del array
    nums = re.findall(r"-?\d+", text)
    # Saltar los primeros números que sean parte de comentarios de dimensión (96, 96)
    # El array de datos comienza después de 'const int8_t ... = {'
    brace_pos = text.find("{")
    if brace_pos == -1:
        raise ValueError(f"No se encontró array en {path}")
    nums = re.findall(r"-?\d+", text[brace_pos:])
    arr = np.array([int(n) for n in nums], dtype=np.int8)
    # int8 → uint8: +128
    arr_uint8 = (arr.astype(np.int16) + 128).clip(0, 255).astype(np.uint8)
    return arr_uint8.reshape(IMG_H, IMG_W)


def load_group_images() -> list:
    """Devuelve lista de arrays uint8 96x96 para las 3 fotos del grupo."""
    images = []
    for i in range(1, 4):
        path = os.path.join(FOTOS_DIR, f"person_image_{i}.cc")
        if os.path.exists(path):
            images.append(parse_cc_int8(path))
        else:
            # Imagen en blanco si no existe
            images.append(np.zeros((IMG_H, IMG_W), dtype=np.uint8))
    return images


# ── Captura serial ─────────────────────────────────────────────────────────────

def capture_serial() -> list[str]:
    """Abre el puerto serial, resetea el ESP32 y captura hasta que aparece
    '=== Inferencia completada ===' o se agota el tiempo."""
    print(f"[serial] Abriendo {PORT} @ {BAUD} baud…")
    try:
        s = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"[ERROR] No se pudo abrir {PORT}: {e}")
        sys.exit(1)

    # Reset por DTR/RTS
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)
    time.sleep(0.5)
    s.reset_input_buffer()

    lines = []
    deadline = time.time() + TIMEOUT_S
    print("[serial] Esperando salida del ESP32…")
    while time.time() < deadline:
        raw = s.readline()
        if raw:
            line = raw.decode("utf-8", errors="replace").rstrip()
            lines.append(line)
            print(f"  {line}")
            if "Inferencia completada" in line:
                break
    s.close()
    print(f"[serial] Capturados {len(lines)} líneas.")
    return lines


# ── Parser de resultados ───────────────────────────────────────────────────────

def parse_results(lines: list[str]) -> list[dict]:
    """
    Devuelve lista de dicts con:
      { 'label': str, 'type': 'static'|'group',
        'idx': int,    'person': float, 'no_person': float }
    """
    results = []
    current = None
    re_static = re.compile(r"Imagen estatica (\d+)")
    re_group  = re.compile(r"Foto integrante (\d+)\s*\(([^)]*)\)")
    re_score  = re.compile(r"person score:(\d+)%.*?no person score\s*(\d+)%", re.I)

    for line in lines:
        m = re_static.search(line)
        if m:
            current = {"type": "static", "idx": int(m.group(1)), "label": f"Imagen estática {m.group(1)}"}
            continue
        m = re_group.search(line)
        if m:
            current = {"type": "group", "idx": int(m.group(1)),
                       "label": f"Foto integrante {m.group(1)} ({m.group(2).strip()})"}
            continue
        m = re_score.search(line)
        if m and current is not None:
            current["person"]    = float(m.group(1))
            current["no_person"] = float(m.group(2))
            results.append(current)
            current = None

    return results


# ── Generación del PDF ─────────────────────────────────────────────────────────

def build_pdf(results: list[dict], static_imgs: list, group_imgs: list):
    print(f"[pdf] Generando {OUTPUT_PDF} con {len(results)} páginas…")
    with PdfPages(OUTPUT_PDF) as pdf:
        for r in results:
            # Seleccionar imagen
            if r["type"] == "static":
                img = static_imgs[r["idx"]]
                img_title = r["label"]
            else:
                img = group_imgs[r["idx"] - 1]
                img_title = r["label"]

            person_pct    = r.get("person", 0.0)
            no_person_pct = r.get("no_person", 0.0)
            detected      = person_pct > no_person_pct

            fig, axes = plt.subplots(1, 2, figsize=(10, 5),
                                     gridspec_kw={"width_ratios": [1, 1]})
            fig.suptitle(img_title, fontsize=14, fontweight="bold")

            # ── Imagen ──
            ax_img = axes[0]
            ax_img.imshow(img, cmap="gray", vmin=0, vmax=255,
                          interpolation="nearest")
            ax_img.set_title("Imagen de entrada (96×96)")
            ax_img.axis("off")

            # ── Resultados ──
            ax_res = axes[1]
            ax_res.axis("off")

            verdict = "[SI] PERSONA DETECTADA" if detected else "[NO] SIN PERSONA"
            color   = "#2ecc71" if detected else "#e74c3c"

            ax_res.text(0.5, 0.75, verdict,
                        transform=ax_res.transAxes,
                        ha="center", va="center",
                        fontsize=15, fontweight="bold", color=color,
                        bbox=dict(boxstyle="round,pad=0.4", facecolor=color,
                                  alpha=0.15, edgecolor=color))

            ax_res.text(0.5, 0.50,
                        f"Persona:       {person_pct:.0f}%\n"
                        f"No persona:  {no_person_pct:.0f}%",
                        transform=ax_res.transAxes,
                        ha="center", va="center",
                        fontsize=13, family="monospace",
                        bbox=dict(boxstyle="round,pad=0.5", facecolor="#ecf0f1",
                                  edgecolor="#bdc3c7"))

            # Barra de confianza
            ax_bar = fig.add_axes([0.55, 0.15, 0.38, 0.08])
            bar_colors = ["#2ecc71", "#e74c3c"]
            ax_bar.barh(["Persona", "No persona"],
                        [person_pct, no_person_pct],
                        color=bar_colors, height=0.5)
            ax_bar.set_xlim(0, 100)
            ax_bar.set_xlabel("Puntuación (%)")
            ax_bar.set_title("Distribución de scores", fontsize=9)

            plt.tight_layout(rect=[0, 0, 1, 0.93])
            pdf.savefig(fig, dpi=150)
            plt.close(fig)
            print(f"  + Página: {img_title}  →  persona={person_pct:.0f}%  no_persona={no_person_pct:.0f}%")

        # Página de resumen
        fig_sum, ax_sum = plt.subplots(figsize=(10, 6))
        ax_sum.axis("off")
        ax_sum.set_title("Resumen de inferencias", fontsize=16, fontweight="bold", pad=15)

        table_data = []
        for r in results:
            det = "[SI]" if r.get("person", 0) > r.get("no_person", 0) else "[NO]"
            table_data.append([r["label"],
                                f"{r.get('person', 0):.0f}%",
                                f"{r.get('no_person', 0):.0f}%",
                                det])

        if table_data:
            tbl = ax_sum.table(
                cellText=table_data,
                colLabels=["Imagen", "Persona", "No persona", "Detectada"],
                cellLoc="center", loc="center",
                bbox=[0, 0, 1, 0.9]
            )
            tbl.auto_set_font_size(False)
            tbl.set_fontsize(10)
            for (row, col), cell in tbl.get_celld().items():
                if row == 0:
                    cell.set_facecolor("#2c3e50")
                    cell.set_text_props(color="white", fontweight="bold")
                elif row % 2 == 0:
                    cell.set_facecolor("#ecf0f1")

        pdf.savefig(fig_sum, dpi=150)
        plt.close(fig_sum)

    print(f"[pdf] ✅ output.pdf generado en: {OUTPUT_PDF}")


# ── Main ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=" * 60)
    print("  ESP32-S3 Person Detection → output.pdf")
    print("=" * 60)

    # 1. Cargar imágenes locales
    print("\n[img] Cargando imágenes estáticas…")
    static_imgs = [load_static_image(i) for i in range(10)]
    print("[img] Cargando fotos del grupo…")
    group_imgs  = load_group_images()

    # 2. Capturar serial
    print("\n[serial] Iniciando captura…")
    lines = capture_serial()

    # 3. Parsear
    results = parse_results(lines)
    print(f"\n[parse] {len(results)} resultados encontrados.")
    if not results:
        print("[WARN] No se encontraron resultados. Verifica la conexión y el firmware.")
        sys.exit(1)

    # 4. Generar PDF
    build_pdf(results, static_imgs, group_imgs)
