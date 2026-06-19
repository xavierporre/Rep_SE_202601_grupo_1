# Notas de entrenamiento — identificador

## Intento 1 (2026-06-16) — FALLO por overfitting

**Dataset**: 455 imágenes (235 label0, 220 label1) → 386 train / 69 test  
**Resultado**: train accuracy ~92%, **test accuracy 66.7%** → reprobado (umbral 85%)

### Causa raíz: demasiados parámetros para el dataset

La arquitectura original tenía:
```
Flatten()          → 9216 valores
Dense(32)          → 9216 × 32 + 32 = 294,944 parámetros
```
Total modelo: **318,754 parámetros** entrenando con 386 muestras.  
Relación param/muestra ≈ 825:1 → el modelo memoriza el train set en lugar de generalizar.

### Fix aplicado en `train_identificador.py`

Reemplazar `Flatten` por `GlobalAveragePooling2D`:
```python
# ANTES (overfitting severo)
layers.Flatten(),
layers.Dropout(0.4),
layers.Dense(32, activation="relu", kernel_regularizer=l2),

# DESPUÉS (correcto)
layers.GlobalAveragePooling2D(),
layers.Dropout(0.5),
layers.Dense(32, activation="relu", kernel_regularizer=l2),
```

`GlobalAveragePooling2D` colapsa 12×12×64 → 64 valores promediados por canal.  
`Dense(32)` pasa de 294K parámetros a **2,080** parámetros.  
Total modelo nuevo: ~**23K parámetros** — mucho más apropiado para 386 muestras.

---

## Intento 2 (2026-06-17) — 78.1%, aún bajo el umbral

**Dataset**: 635 imágenes (320 label0, 315 label1) → 539 train / 96 test  
**Resultado**: **test accuracy 78.1%** (mejor val_accuracy época 34, EarlyStopping disparó en época 54)  
`TP=43 TN=32 FP=16 FN=5`

### Por qué no se ejecutan todas las épocas

`EarlyStopping(patience=20)`: si el `val_accuracy` no mejora durante 20 épocas consecutivas, el entrenamiento se detiene. Es el comportamiento esperado — en este caso el mejor fue época 34, y se detuvo en 34+20=54.

### Análisis del resultado

- El overfitting se resolvió (train ≈ val accuracy).
- **FN=5** (buena sensibilidad): casi nunca falla cuando el identificador está presente.
- **FP=16** (el problema): clasifica como "con identificador" cuando no está — 33% de FPR en clase 0.
- El modelo tiene sesgo hacia la clase positiva porque las `label0` no cubren suficientemente la variedad de fondos reales del ring.

### Para pasar al 85%

Capturar 150-200 imágenes `label0` más variadas:
- Piso del ring desde distintos ángulos
- Bordes y cinta negra en el encuadre
- El ring vacío desde la posición real de combate
- Sin el identificador pero con objetos en el fondo

---

## Intento 3 (2026-06-17) — **85.25% APROBADO** ✓

**Dataset**: 811 imágenes (493 label0, 318 label1) → 689 train / 122 test  
**Resultado**: **test accuracy 85.25%** (val_accuracy=0.8525 en época 80, todas las épocas completadas)  
`TP=37 TN=67 FP=7 FN=11`

- FP=7 (falsos positivos, bajo)
- FN=11 (falsos negativos, aceptable para uso en combate — mejor perder alguna detección que generar falsas)
- EarlyStopping no disparó — el modelo siguió mejorando hasta la época 80

**Siguiente paso**: `python3 quantize_and_export.py` → copiar `.cc/.h` a `robot_cam/main/` → compilar y flashear.

---

## Diagnóstico en campo (2026-06-19) — sesgo de orientación detectado

Tras integrar el Intento 3 en el robot completo se observó:
- El objeto tardaba **varios segundos** en detectarse aunque estuviera a 30 cm.
- Rendimiento variable según posición y ángulo en el ring.

Se desarrolló `diagnostico_sesgo.py` (GradCAM sobre la capa `conv2d_2`) para visualizar qué zonas activan el modelo.

### Resultado del diagnóstico

- El calor **cae sobre el objeto** en la mayoría de imágenes `label1` → **sesgo de fondo descartado**.
- Sin embargo, el calor se concentra en **un único rasgo/zona del identificador** en casi todos los mapas. Solo las imágenes 00113, 00486, 00528 muestran calor disperso sobre el objeto completo.
- **Causa raíz**: sesgo de orientación. Las 318 imágenes `label1` se capturaron casi todas desde el mismo ángulo/distancia. El modelo aprendió el rasgo más discriminativo en esa vista, no el objeto en general.

### Cambios aplicados en `train_identificador.py`

Aumentación reforzada para atacar el sesgo de orientación:

```python
# ANTES
layers.RandomFlip("horizontal"),
layers.RandomRotation(0.04),
layers.RandomZoom(0.08),

# DESPUÉS
layers.RandomFlip("horizontal_and_vertical"),   # cubre vista desde arriba/abajo
layers.RandomRotation(0.15),                    # ±54° (antes ±14.4°)
layers.RandomZoom(0.20),                        # ±20% (antes ±8%)
layers.RandomTranslation(0.10, 0.10),           # nuevo: desplazamiento ±10%
# + apply_cutout(0.25) aplicado por imagen en el pipeline .map()
```

### Para el Intento 4

Capturar **100–150 imágenes `label1` nuevas** variando ángulo y distancia en bloques:

| Bloque | Descripción |
|--------|-------------|
| A | Vista frontal directa, ~20 cm |
| B | Cámara inclinada ~30° desde arriba, ~20 cm |
| C | Identificador girado ~45° en su eje vertical |
| D | Identificador girado ~90° (cara lateral) |
| E | Distancia ~40 cm, frontal |
| F | Distancia ~15 cm, frontal |
| G | Iluminación diferente (sombra lateral) |

Las imágenes 00113, 00486, 00528 del dataset actual (calor disperso) son el modelo de captura a replicar.

No hace falta añadir más `label0` (FP=7 es bajo, 493 imágenes son suficientes).

**Verificación post-reentrenamiento**: correr `diagnostico_sesgo.py` y comprobar que el calor ya no se concentra en un único punto del objeto.

---

