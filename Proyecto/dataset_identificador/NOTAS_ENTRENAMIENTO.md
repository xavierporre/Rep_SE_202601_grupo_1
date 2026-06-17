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

## Si el intento 2 también falla

Si el test accuracy sigue bajo ~70-75% después del fix arquitectónico, el problema es el dataset:
- Capturar más fotos, especialmente a la **distancia real de combate** y con **fondos variados**.
- Objetivo: ≥300 imágenes de cada clase con buena variedad de ángulo/distancia/iluminación.
- Las fotos con sobreexposición, motion blur o labels incorrectas ya se limpiaron del set anterior, pero conviene seguir siendo estricto al capturar.
