/*
 * robot_state — constantes, tipos y API de motores compartidos
 *
 * Robot de ring: superficie blanca con bordes de cinta negra.
 * El ESP32-S3 recibe por UART1 (TX=GPIO17, RX=GPIO18) los datos de la
 * ESP32-CAM: deteccion de borde (grilla 3x2) + identificador (modelo TFLite).
 */

#ifndef ROBOT_S3_ROBOT_STATE_H_
#define ROBOT_S3_ROBOT_STATE_H_

#include <stdint.h>
#include <stdbool.h>

// ── Velocidades (requisito del proyecto) ───────────────────────────────
#define SPEED_LINEAR     100   // % PWM movimientos lineales (adelante/atras)
#define SPEED_TURN        80   // % PWM giros sobre el eje (ruedas opuestas) — reducido a 80% para dar tiempo a la CAM

// ── Temporizacion ──────────────────────────────────────────────────────
#define CTRL_TICK_MS      50   // periodo del lazo de control (20 Hz)
#define UART_TIMEOUT_MS  500   // sin datos de camara → parada de seguridad
#define AVOID_TURN_MS      400   // giro de evasion tras tocar el borde (tope maximo)
#define AVOID_TURN_MIN_MS  150   // giro minimo antes de poder cortar por sensor
#define TURN_180_MS     1200   // giro ~180° a SPEED_TURN — CALIBRAR en el robot
#define TURN_90_MS       600   // giro ~90° a SPEED_TURN  — CALIBRAR en el robot
#define RETREAT_DRIVE_TIMEOUT_MS  6000  // tope para encontrar el borde de enfrente en RETREAT (atasco)
#define RETREAT_EDGE_TIMEOUT_MS   6000  // tope para llegar a la esquina siguiendo el borde en RETREAT
#define RETREAT_MIN_DRIVE_MS       600  // avance minimo en DRIVE antes de aceptar el borde objetivo (despegarse del borde)
#define RETREAT_EDGE_STREAK          3  // frames consecutivos con borde antes de darlo por valido (anti-rebote)
#define PATROL_TURN_MS   200   // tramo de giro del arco de busqueda (patrulla)
#define PATROL_FWD_MS    350   // tramo recto del arco de busqueda (patrulla)
#define FIND_LOOK_MS    2500   // tiempo parado buscando el identificador (~1 inferencia)
#define FIND_TURN_MS     600   // giro breve entre miradas (~1/4 vuelta)
#define PATROL_NUDGE_PERIOD_TICKS 4  // cada cuantos ticks (50 ms) corregir hacia la cinta lejana
#define PATROL_STEER_PERIOD       3  // ventana de ticks del avance-con-pulsos en SUB_PAT_RUN
#define PATROL_STEER_TURN         2  // ticks de la ventana dedicados a girar (el resto avanza)

// ── Identificador (modelo embutido en la CAM) ──────────────────────────
#define IDENT_CONF_THR    50   // confianza minima (%) para iniciar la carga
#define IDENT_LOST_MS   2000   // sin deteccion durante la carga → volver a escanear

// ── Modos de operacion (ordenes del servidor web) ──────────────────────
typedef enum {
    MODE_MANUAL = 0,   // control manual desde la web (d-pad)
    MODE_FIND,         // buscar y destruir: escanear → cargar al identificador
    MODE_PATROL,       // patrullar el perimetro siguiendo la cinta (CCW)
    MODE_RETREAT,      // retirada: ir a una esquina del ring y mantenerse
    MODE_AUDIO,        // control por frecuencia de audio (microfono en GPIO1)
} robot_mode_t;

// Ultimo dato recibido de la camara por UART
typedef struct {
    char   action[16];  // sugerencia de la CAM: forward|turn_left|turn_right|stop
    int8_t dist[3];     // [izq, centro, der]: 0=sin borde, 1=lejos, 2=cerca
    bool   ident;       // identificador detectado (score >= 0.50 en la CAM)
    int    conf;        // confianza 0..100
} cam_data_t;

// ── Motores y LED (implementados en web.cpp, pines L298N del template) ─
#ifdef __cplusplus
extern "C" {
#endif
void setPWM(int spd100);
void motorStop(void);
void moveForward(int s);
void moveBackward(int s);
void turnLeft(int s);     // giro sobre el eje: A atras, B adelante
void turnRight(int s);    // giro sobre el eje: A adelante, B atras
void setLED(uint8_t r, uint8_t g, uint8_t b);
#ifdef __cplusplus
}
#endif

#endif  // ROBOT_S3_ROBOT_STATE_H_
