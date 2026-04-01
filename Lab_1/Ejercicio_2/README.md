# Ejercicio 2

Aplicar operador Sobel horizontal y vertical a una imagen de 96x96 en el ESP32-S3 e imprimir el resultado por monitor serial. Luego graficar la salida utilizando Python.

## Estructura

```
Ejercicio_2/
├── solbel.ipynb          # Notebook con procesamiento de imagen y graficos
├── img/                  # Imagenes (original, redimensionada, resultado)
└── esp32/                # Proyecto ESP-IDF
    └── main/
        ├── sobel_main.c  # Aplica Sobel H y V, imprime por serial
        ├── image_data.h  # Header con datos de la imagen (generado)
        └── CMakeLists.txt
```

## Pasos para ejecutar

### 1. Redimensionar imagen y generar header C

Ejecutar las dos primeras celdas de codigo del notebook `solbel.ipynb`:

- **Celda 1**: carga la imagen original, la redimensiona a 96x96 en escala de grises y la guarda en `img/`.
- **Celda 2** (bajo "Generar header C"): genera `esp32/main/image_data.h` con los pixeles como array C.

### 2. Compilar y flashear el ESP32

```bash
cd Lab_1/Ejercicio_2/esp32
source /Users/xavierporre/esp/idf/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor | tee serial_output.txt
```

Esperar a que aparezca `Listo.` y salir del monitor con `Ctrl + ]`.

El archivo `serial_output.txt` queda guardado con la salida completa del Sobel.

### 3. Graficar resultados

Ejecutar la ultima celda del notebook `solbel.ipynb` (bajo "Google Colab"). Esta celda lee `esp32/serial_output.txt`, parsea las matrices Sobel y genera los graficos.

Para usar en **Google Colab**: subir el archivo `serial_output.txt` y ajustar la ruta en el `open()`.
