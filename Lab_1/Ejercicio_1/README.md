# Ejercicio 1

Modificacion del ejemplo Hello World de ESP-IDF para imprimir los nombres del grupo.

## Como correr

```bash
source ~/.zprofile && source ~/.zshrc && get_esp32 && idf.py set-target esp32s3 && idf.py build && idf.py flash -p /dev/cu.usbmodem101 monitor
```

Para salir del monitor serial: `Ctrl + ]`
