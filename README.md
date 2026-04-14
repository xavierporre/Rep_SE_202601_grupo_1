# Rep_SE_202601_grupo_1

Repositorio del curso ICC4200 - Sistemas Embebidos 202610

## Integrantes

- Xavier Porre
- Martin Feres
- Vicente Abalos

## Configuracion de ESP-IDF

### 1. Preparar estructura de carpetas

```bash
mkdir ~/esp && mkdir ~/esp/idf && mkdir ~/esp/idf-tools && mkdir ~/esp/backup && mkdir ~/esp/projects
```

### 2. Clonar ESP-IDF

```bash
cd ~/esp/idf
git clone --recursive https://github.com/espressif/esp-idf.git
```

### 3. Instalar ESP-IDF

```bash
cd ~/esp/idf/esp-idf
./install.sh
. ./export.sh
```

### 4. Configurar aliases y variables de entorno

Agregar en `~/.zprofile`:

```bash
export IDF_TOOLS_PATH="$HOME/esp/idf-tools"
export PATH="$HOME/esp/idf/esp-idf/tools:$PATH"
export IDF_PATH="$HOME/esp/idf/esp-idf"
```

Agregar en `~/.zshrc`:

```bash
alias get_esp32='. $HOME/esp/idf/esp-idf/export.sh'
```

Aplicar cambios:

```bash
source ~/.zprofile && source ~/.zshrc
```

### 5. Compilar y flashear un proyecto

```bash
source ~/.zprofile && source ~/.zshrc && get_esp32
cd <ruta_del_proyecto>
idf.py set-target esp32s3
idf.py build && idf.py flash monitor
```

Para salir del monitor serial: `Ctrl + ]`

## Estructura del repositorio

```
Rep_SE_202601_grupo_1
|- Lab_1
|   |- Ejercicio_1
|   |   |- README.md
|   |   |- output.pdf
|   |   |- codigo del ejercicio
|   |- Ejercicio_2
|   |- Ejercicio_3
|- Lab_2
|- Lab_3
|- Proyecto
```
