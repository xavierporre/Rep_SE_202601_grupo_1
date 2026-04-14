# Audio (Ejercicio 3)

Este proyecto captura señal por ADC, estima la frecuencia dominante (DFT/"FFT" conceptual) y enciende LEDs según bandas.

## Ejecución (ESP-IDF)

Desde esta carpeta:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Parámetros relevantes

En el código (ver [main/audio.c](main/audio.c)):

- `SAMPLING_FREQUENCY` ($f_s$): frecuencia de muestreo (Hz).
- `SAMPLES` ($N$): número de muestras por ventana.
- `ADC_BITWIDTH_*` y atenuación: configuración del ADC.

La señal se analiza por **ventanas** de duración:

$$T=\frac{N}{f_s}$$

Y la resolución de frecuencia (separación entre bins) viene dada por:

$$\Delta f = \frac{f_s}{N}$$


---

## Análisis: efecto de modificar la frecuencia de muestreo ($f_s$)

Modificar $f_s$ cambia **tres cosas** clave:

1) **Frecuencia máxima sin aliasing (Nyquist)**

Para una señal real, solo puedes medir sin aliasing hasta:

$$f_{max} = \frac{f_s}{2}$$

- Si bajas $f_s$, reduces el rango de frecuencias medibles.
- Si tu micrófono/entrada contiene componentes por encima de $f_s/2$, aparecerán “dobladas” (aliasing) dentro del rango.

2) **Resolución espectral (si $N$ se mantiene fijo)**

$$\Delta f = \frac{f_s}{N}$$

- Subir $f_s$ **empeora** la resolución (bins más separados) si $N$ es fijo.
- Bajar $f_s$ **mejora** la resolución (bins más juntos) si $N$ es fijo.

3) **Tiempo de ventana y latencia**

$$T=\frac{N}{f_s}$$

- Subir $f_s$ hace la ventana **más corta** (menos latencia, pero menos información por ventana).
- Bajar $f_s$ hace la ventana **más larga** (más latencia, pero más “promedio” temporal).

### Ejemplo con tus valores

Con `SAMPLES=256` y `SAMPLING_FREQUENCY=8000`:

- Ventana: $T = 256/8000 \approx 0.032\,s$ (32 ms)
- Resolución: $\Delta f = 8000/256 = 31.25\,Hz$
- Nyquist: $f_{max}=4000\,Hz$

Si cambias a $f_s=4000$ manteniendo $N=256$:

- $T\approx 64\,ms$ (más latencia)
- $\Delta f = 15.625\,Hz$ (mejor resolución)
- $f_{max}=2000\,Hz$ (ojo si quieres detectar >2 kHz)

---

## Análisis: efecto de modificar el número de muestras ($N$) en la FFT/DFT

Cambiar $N$ impacta principalmente:

1) **Resolución de frecuencia**

$$\Delta f = \frac{f_s}{N}$$

- A mayor $N$, mejor resolución (bins más finos).
- A menor $N$, peor resolución (más error al estimar la frecuencia dominante).

2) **Duración de ventana**

$$T=\frac{N}{f_s}$$

- A mayor $N$, mayor ventana (más latencia, respuesta más lenta).
- A menor $N$, menor ventana (más rápida, pero más “nerviosa”).

3) **Costo computacional**

- FFT: $O(N\log N)$ (escala razonable al subir $N$).
- DFT (como en el código): $O(N^2)$ (subir $N$ puede volverse muy lento).

### Ejemplo rápido

Con $f_s=8000$:

- $N=256 \Rightarrow \Delta f=31.25\,Hz$
- $N=512 \Rightarrow \Delta f=15.625\,Hz$
- $N=1024 \Rightarrow \Delta f=7.8125\,Hz$

Pero la ventana también crece:

- $N=256 \Rightarrow T\approx 32\,ms$
- $N=1024 \Rightarrow T\approx 128\,ms$

---

## Análisis: efecto de modificar la resolución del ADC

La resolución del ADC (bits) afecta sobre todo a:

1) **Cuantización y ruido**

Con $B$ bits, el número de niveles es $2^B$ y el tamaño de paso ideal (LSB) es aproximadamente:

$$\mathrm{LSB} \approx \frac{V_{FS}}{2^B}$$

- Más bits → menor error de cuantización → mejor “detalle” en amplitud.
- Menos bits → más ruido de cuantización → más difícil detectar picos si la señal es pequeña.

El SNR ideal por cuantización (aprox.) crece con los bits:

$$\mathrm{SNR}_{ideal} \approx 6.02B + 1.76\,\mathrm{dB}$$

2) **Estabilidad de la detección de frecuencia**

Aunque el ADC no cambia directamente $\Delta f$, sí cambia el **ruido de la señal**, lo que puede:

- Levantar el “piso” del espectro.
- Hacer que el bin máximo salte entre vecinos (más inestable).

3) **Centrado/offset en tu código**

En el código se centra con `value - 2048`, que asume 12 bits (0..4095).

- Si cambias a 11 bits (0..2047), el centro pasa a 1024.
- Si cambias a 10 bits (0..1023), el centro pasa a 512.

Si no ajustas ese centrado, introduces un gran componente DC que puede empeorar la estimación.


---
