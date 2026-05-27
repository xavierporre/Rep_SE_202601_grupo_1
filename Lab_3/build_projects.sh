#!/bin/bash
# Script para configurar y compilar proyectos de TensorFlow Lite en ESP32
# Este script establece las rutas de componentes y construye los proyectos

LAB3_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELLO_WORLD_PATH="$LAB3_PATH/projects_tf/hello_world"
PERSON_DETECTION_PATH="$LAB3_PATH/projects_tf/person_detection"

# Crear un componente wrapper para los components de TF Lite Micro
# que los proyectos puedan encontrar

# Configurar variable de entorno para que idf encuentre componentes locales
export EXTRA_COMPONENT_DIRS="$LAB3_PATH:$LAB3_PATH/esp-tflite-micro"

echo "=========================================="
echo "Compilando hello_world..."
echo "=========================================="
cd "$HELLO_WORLD_PATH"
idf.py build || { echo "Error compilando hello_world"; exit 1; }

echo ""
echo "=========================================="
echo "Compilando person_detection..."
echo "=========================================="
cd "$PERSON_DETECTION_PATH"
idf.py build || { echo "Error compilando person_detection"; exit 1; }

echo ""
echo "✓ Ambos proyectos compilados exitosamente"
