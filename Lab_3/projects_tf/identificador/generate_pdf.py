#!/usr/bin/env python3
"""
generate_pdf.py  —  Identificador Lab 3 · Punto 3
Clasifica 10 imágenes (5 con embutido + 5 sin embutido) usando
el modelo cnn_dropout.onnx del Lab 2 y genera output.pdf con
imagen + resultado de clasificación por página.
"""

import os
import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from PIL import Image

# ── Rutas ─────────────────────────────────────────────────────────────────────
BASE         = os.path.dirname(os.path.abspath(__file__))
ONNX_MODEL   = os.path.join(BASE, "..", "..", "..", "Lab_2", "modelos", "cnn_dropout.onnx")
IMGS_DIR     = os.path.join(BASE, "imagenes_output")
OUTPUT_PDF   = os.path.join(BASE, "output.pdf")
IMG_W, IMG_H = 96, 96

# ── Selección de imágenes (5 label0 + 5 label1) ───────────────────────────────
def select_images():
    all_imgs = sorted(os.listdir(IMGS_DIR))
    label0 = [f for f in all_imgs if "_label0" in f][:5]
    label1 = [f for f in all_imgs if "_label1" in f][:5]
    selected = label0 + label1
    gt_labels = [0]*5 + [1]*5
    print(f"[imgs] Sin embutido (label0): {label0}")
    print(f"[imgs] Con embutido (label1): {label1}")
    return selected, gt_labels

# ── Preprocesado de imagen ─────────────────────────────────────────────────────
def preprocess(path: str) -> tuple[np.ndarray, np.ndarray]:
    """Devuelve (array_display uint8 96x96, array_model float32 1x1x96x96)."""
    img = Image.open(path).convert("L").resize((IMG_H, IMG_W))
    arr_uint8 = np.array(img, dtype=np.uint8)
    # Normalizar a [0, 1] float32 tal como en el entrenamiento
    arr_f32 = arr_uint8.astype(np.float32) / 255.0
    # PyTorch NCHW: (1, 1, 96, 96)
    model_input = arr_f32.reshape(1, 1, IMG_H, IMG_W)
    return arr_uint8, model_input

# ── Inferencia ONNX ───────────────────────────────────────────────────────────
def load_model():
    try:
        import onnxruntime as ort
    except ImportError:
        print("[ERROR] Instala onnxruntime: pip install onnxruntime")
        sys.exit(1)
    sess = ort.InferenceSession(ONNX_MODEL)
    print(f"[model] Cargado: {os.path.basename(ONNX_MODEL)}")
    return sess

def infer(sess, model_input: np.ndarray) -> tuple[float, float]:
    """Devuelve (prob_embutido, prob_no_embutido) en [0, 100] %."""
    import onnxruntime as ort
    input_name = sess.get_inputs()[0].name
    logit = sess.run(None, {input_name: model_input})[0][0][0]
    # Sigmoid sobre el logit binario
    prob = 1.0 / (1.0 + np.exp(-float(logit)))
    return prob * 100.0, (1.0 - prob) * 100.0

# ── Generación del PDF ────────────────────────────────────────────────────────
def build_pdf(results: list[dict]):
    print(f"\n[pdf] Generando {OUTPUT_PDF} …")
    with PdfPages(OUTPUT_PDF) as pdf:
        for r in results:
            img_disp   = r["img_disp"]
            pct_emb    = r["pct_embutido"]
            pct_no     = r["pct_no_embutido"]
            pred_label = 1 if pct_emb >= pct_no else 0
            gt_label   = r["gt_label"]
            fname      = r["fname"]
            correct    = (pred_label == gt_label)

            gt_str   = "Con embutido"  if gt_label == 1 else "Sin embutido"
            pred_str = "Con embutido"  if pred_label == 1 else "Sin embutido"
            verdict  = "✓ CORRECTO" if correct else "✗ INCORRECTO"
            v_color  = "#27ae60" if correct else "#c0392b"
            bar_colors = ["#e74c3c", "#3498db"]  # embutido=rojo, no_embutido=azul

            fig = plt.figure(figsize=(11, 5))
            fig.suptitle(f"{fname}   |   GT: {gt_str}", fontsize=13, fontweight="bold")

            # ── Imagen
            ax_img = fig.add_axes([0.03, 0.08, 0.38, 0.80])
            ax_img.imshow(img_disp, cmap="gray", vmin=0, vmax=255, interpolation="nearest")
            ax_img.set_title("Imagen de entrada (96×96 escala de grises)", fontsize=9)
            ax_img.axis("off")

            # ── Panel resultados
            ax_res = fig.add_axes([0.44, 0.45, 0.52, 0.45])
            ax_res.axis("off")
            ax_res.text(0.5, 0.82, f"Predicción: {pred_str}",
                        transform=ax_res.transAxes, ha="center", va="center",
                        fontsize=13, fontweight="bold")
            ax_res.text(0.5, 0.55, verdict,
                        transform=ax_res.transAxes, ha="center", va="center",
                        fontsize=14, fontweight="bold", color=v_color,
                        bbox=dict(boxstyle="round,pad=0.4", facecolor=v_color,
                                  alpha=0.12, edgecolor=v_color))
            ax_res.text(0.5, 0.22,
                        f"P(embutido)     = {pct_emb:5.1f}%\n"
                        f"P(no embutido) = {pct_no:5.1f}%",
                        transform=ax_res.transAxes, ha="center", va="center",
                        fontsize=12, family="monospace",
                        bbox=dict(boxstyle="round,pad=0.5", facecolor="#ecf0f1",
                                  edgecolor="#bdc3c7"))

            # ── Barra de confianza
            ax_bar = fig.add_axes([0.46, 0.10, 0.48, 0.28])
            bars = ax_bar.barh(["Embutido", "No embutido"],
                               [pct_emb, pct_no],
                               color=bar_colors, height=0.5)
            for bar, val in zip(bars, [pct_emb, pct_no]):
                ax_bar.text(min(val + 1.5, 95), bar.get_y() + bar.get_height()/2,
                            f"{val:.1f}%", va="center", fontsize=10)
            ax_bar.set_xlim(0, 100)
            ax_bar.set_xlabel("Probabilidad (%)", fontsize=9)
            ax_bar.axvline(50, color="gray", linestyle="--", linewidth=0.8, alpha=0.6)
            ax_bar.set_title("Distribución de probabilidades", fontsize=9)

            pdf.savefig(fig, dpi=150)
            plt.close(fig)
            print(f"  + {fname:<30} embutido={pct_emb:5.1f}%  →  {pred_str}  [{verdict}]")

        # ── Página resumen
        fig_sum, ax = plt.subplots(figsize=(11, 6))
        ax.axis("off")
        ax.set_title("Resumen de clasificaciones — Modelo cnn_dropout (Lab 2)",
                     fontsize=14, fontweight="bold", pad=15)

        table_data = []
        correct_count = 0
        for r in results:
            pred = 1 if r["pct_embutido"] >= r["pct_no_embutido"] else 0
            corr = pred == r["gt_label"]
            correct_count += int(corr)
            table_data.append([
                r["fname"],
                "Con embutido" if r["gt_label"] == 1 else "Sin embutido",
                f"{r['pct_embutido']:.1f}%",
                f"{r['pct_no_embutido']:.1f}%",
                "Con embutido" if pred == 1 else "Sin embutido",
                "✓" if corr else "✗",
            ])

        tbl = ax.table(
            cellText=table_data,
            colLabels=["Imagen", "GT (real)", "P(embutido)", "P(no embutido)", "Predicción", "OK"],
            cellLoc="center", loc="center",
            bbox=[0, 0.05, 1, 0.85],
        )
        tbl.auto_set_font_size(False)
        tbl.set_fontsize(9)
        for (row, col), cell in tbl.get_celld().items():
            if row == 0:
                cell.set_facecolor("#2c3e50")
                cell.set_text_props(color="white", fontweight="bold")
            elif row % 2 == 0:
                cell.set_facecolor("#ecf0f1")
            # Colorear columna OK
            if row > 0 and col == 5:
                cell.set_facecolor("#d5f5e3" if cell.get_text().get_text() == "✓" else "#fadbd8")

        ax.text(0.5, 0.01, f"Exactitud: {correct_count}/{len(results)} ({correct_count/len(results)*100:.0f}%)",
                transform=ax.transAxes, ha="center", fontsize=12, fontweight="bold")

        pdf.savefig(fig_sum, dpi=150)
        plt.close(fig_sum)

    print(f"\n[pdf] ✅  output.pdf generado: {OUTPUT_PDF}")
    print(f"     Exactitud final: {correct_count}/{len(results)} ({correct_count/len(results)*100:.0f}%)")


# ── Main ──────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 62)
    print("  Identificador Lab 3 — Clasificación con cnn_dropout.onnx")
    print("=" * 62)

    selected_fnames, gt_labels = select_images()
    sess = load_model()

    results = []
    print("\n[infer] Ejecutando inferencia …")
    for fname, gt in zip(selected_fnames, gt_labels):
        path = os.path.join(IMGS_DIR, fname)
        img_disp, model_input = preprocess(path)
        pct_emb, pct_no = infer(sess, model_input)
        results.append({
            "fname": fname,
            "gt_label": gt,
            "img_disp": img_disp,
            "pct_embutido": pct_emb,
            "pct_no_embutido": pct_no,
        })
        pred = "embutido" if pct_emb >= pct_no else "no_embutido"
        gt_s = "label1" if gt == 1 else "label0"
        print(f"  {fname:<28} embutido={pct_emb:5.1f}%  no_embutido={pct_no:5.1f}%  → {pred}  (GT:{gt_s})")

    build_pdf(results)
