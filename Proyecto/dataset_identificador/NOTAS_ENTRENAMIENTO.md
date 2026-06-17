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

## Si el intento 3 también falla

Si el test accuracy sigue bajo ~70-75% después del fix arquitectónico, el problema es el dataset:
- Capturar más fotos, especialmente a la **distancia real de combate** y con **fondos variados**.
- Objetivo: ≥300 imágenes de cada clase con buena variedad de ángulo/distancia/iluminación.
- Las fotos con sobreexposición, motion blur o labels incorrectas ya se limpiaron del set anterior, pero conviene seguir siendo estricto al capturar.
