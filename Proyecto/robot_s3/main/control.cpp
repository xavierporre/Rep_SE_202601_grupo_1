#include "control.h"
#include "uart_link.h"

#include <cstring>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "control";

// ── Subestados ──────────────────────────────────────────────────────────
typedef enum {
    SUB_NONE = 0,
    // FIND (buscar y destruir)
    SUB_FD_SCAN,        // girar sobre el eje buscando el identificador
    SUB_FD_CHARGE,      // cargar de frente al identificador (100%)
    SUB_FD_AVOID,       // retroceso tras tocar el borde
    // PATROL
    SUB_PAT_RUN,        // seguir el borde (cinta a la derecha, vuelta CCW)
    SUB_PAT_AVOID,      // retroceso tras tocar el borde
    SUB_PAT_AVOID_TURN, // giro de recuperacion tras el retroceso
    SUB_PAT_SEARCH_TURN,// borde perdido: tramo de giro del arco de busqueda
    SUB_PAT_SEARCH_FWD, // borde perdido: tramo recto del arco de busqueda
    // RETREAT (a una esquina del ring rectangular)
    SUB_RT_TURN180,     // giro temporizado ~180°
    SUB_RT_DRIVE,       // avanzar hasta el borde de enfrente
    SUB_RT_TURN90,      // giro temporizado ~90° (borde queda a la derecha)
    SUB_RT_EDGE,        // avanzar junto al borde hasta la esquina
    SUB_RT_HOLD,        // detenido en la esquina
} sub_state_t;

static const char *sub_name(sub_state_t s) {
    switch (s) {
        case SUB_FD_SCAN:         return "scan";
        case SUB_FD_CHARGE:       return "charge";
        case SUB_FD_AVOID:        return "avoid";
        case SUB_PAT_RUN:         return "track";
        case SUB_PAT_AVOID:       return "avoid";
        case SUB_PAT_AVOID_TURN:  return "avoid_turn";
        case SUB_PAT_SEARCH_TURN: return "search_turn";
        case SUB_PAT_SEARCH_FWD:  return "search_fwd";
        case SUB_RT_TURN180:      return "turn180";
        case SUB_RT_DRIVE:        return "drive";
        case SUB_RT_TURN90:       return "turn90";
        case SUB_RT_EDGE:         return "edge";
        case SUB_RT_HOLD:         return "hold";
        default:                  return "manual";
    }
}

static const char *mode_name(robot_mode_t m) {
    switch (m) {
        case MODE_FIND:    return "find";
        case MODE_PATROL:  return "patrol";
        case MODE_RETREAT: return "retreat";
        default:           return "manual";
    }
}

// ── Estado compartido (httpd ↔ tarea de control) ───────────────────────
static portMUX_TYPE          s_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile robot_mode_t g_mode = MODE_MANUAL;
static volatile sub_state_t  g_sub  = SUB_NONE;
static uint32_t              g_sub_until     = 0;  // fin de maniobra temporizada
static uint32_t              g_last_ident_ms = 0;  // ultima deteccion en la carga
static volatile bool         g_link_ok       = false;
static bool                  g_link_was_lost = false;
static cam_data_t            g_cam;                // copia del ultimo dato usado
static uint32_t              g_cam_age_ms    = 0;

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void enter_sub(sub_state_t s) {
    g_sub = s;
}

static void enter_timed(sub_state_t s, uint32_t duration_ms) {
    g_sub       = s;
    g_sub_until = millis() + duration_ms;
}

void control_set_mode(robot_mode_t m) {
    portENTER_CRITICAL(&s_lock);
    g_mode = m;
    switch (m) {
        case MODE_FIND:    enter_sub(SUB_FD_SCAN);                  break;
        case MODE_PATROL:  enter_sub(SUB_PAT_RUN);                  break;
        case MODE_RETREAT: enter_timed(SUB_RT_TURN180, TURN_180_MS); break;
        default:           enter_sub(SUB_NONE);                     break;
    }
    g_last_ident_ms = 0;
    portEXIT_CRITICAL(&s_lock);
    motorStop();
    ESP_LOGI(TAG, "Modo → %s", mode_name(m));
}

robot_mode_t control_get_mode(void) {
    return g_mode;
}

// ── Comportamiento FIND: escanear → cargar → esquivar borde ────────────
static void tick_find(const cam_data_t *cam, uint32_t now, bool border_near) {
    if (border_near && g_sub != SUB_FD_AVOID) {
        enter_timed(SUB_FD_AVOID, BACKOFF_MS);
    }

    switch (g_sub) {
        case SUB_FD_AVOID:
            moveBackward(SPEED_LINEAR);
            setLED(180, 0, 0);
            if ((int32_t)(now - g_sub_until) >= 0) enter_sub(SUB_FD_SCAN);
            break;

        case SUB_FD_SCAN:
            turnRight(SPEED_TURN);
            setLED(0, 120, 120);
            if (cam->ident && cam->conf >= IDENT_CONF_THR) {
                g_last_ident_ms = now;
                enter_sub(SUB_FD_CHARGE);
                ESP_LOGI(TAG, "[FIND] identificador conf=%d%% → CARGA", cam->conf);
            }
            break;

        case SUB_FD_CHARGE:
            if (cam->ident && cam->conf >= IDENT_CONF_THR) g_last_ident_ms = now;
            if (now - g_last_ident_ms > IDENT_LOST_MS) {
                ESP_LOGI(TAG, "[FIND] identificador perdido → ESCANEO");
                enter_sub(SUB_FD_SCAN);
                break;
            }
            // Empujones laterales si un borde lateral queda cerca durante la carga
            if      (cam->dist[0] == 2) turnRight(SPEED_TURN);
            else if (cam->dist[2] == 2) turnLeft(SPEED_TURN);
            else                        moveForward(SPEED_LINEAR);
            setLED(0, 255, 0);
            break;

        default:
            enter_sub(SUB_FD_SCAN);
            break;
    }
}

// ── Comportamiento PATROL: cinta a la derecha, vuelta CCW ──────────────
static void tick_patrol(const cam_data_t *cam, uint32_t now, bool border_near) {
    if (border_near && g_sub != SUB_PAT_AVOID && g_sub != SUB_PAT_AVOID_TURN) {
        enter_timed(SUB_PAT_AVOID, BACKOFF_MS);
    }

    bool any_border = (cam->dist[0] | cam->dist[1] | cam->dist[2]) != 0;

    switch (g_sub) {
        case SUB_PAT_AVOID:
            moveBackward(SPEED_LINEAR);
            setLED(180, 0, 0);
            if ((int32_t)(now - g_sub_until) >= 0)
                enter_timed(SUB_PAT_AVOID_TURN, PATROL_TURN_MS);
            break;

        case SUB_PAT_AVOID_TURN:
            turnLeft(SPEED_TURN);
            setLED(180, 0, 0);
            if ((int32_t)(now - g_sub_until) >= 0) enter_sub(SUB_PAT_RUN);
            break;

        case SUB_PAT_RUN:
            setLED(150, 150, 0);
            if      (cam->dist[2] == 2) turnLeft(SPEED_TURN);       // borde der. cerca: alejarse
            else if (cam->dist[0] == 2) turnRight(SPEED_TURN);      // borde izq. cerca: alejarse
            else if (cam->dist[1] >= 1) turnLeft(SPEED_TURN);       // borde al frente: doblar la esquina
            else if (cam->dist[2] == 1) moveForward(SPEED_LINEAR);  // siguiendo la cinta a la derecha
            else if (cam->dist[0] == 1) turnRight(SPEED_TURN);      // cinta quedo a la izq.: reorientar
            else enter_timed(SUB_PAT_SEARCH_TURN, PATROL_TURN_MS);  // cinta perdida: arco de busqueda
            break;

        case SUB_PAT_SEARCH_TURN:
            turnRight(SPEED_TURN);
            setLED(150, 150, 0);
            if (any_border) { enter_sub(SUB_PAT_RUN); break; }
            if ((int32_t)(now - g_sub_until) >= 0)
                enter_timed(SUB_PAT_SEARCH_FWD, PATROL_FWD_MS);
            break;

        case SUB_PAT_SEARCH_FWD:
            moveForward(SPEED_LINEAR);
            setLED(150, 150, 0);
            if (any_border) { enter_sub(SUB_PAT_RUN); break; }
            if ((int32_t)(now - g_sub_until) >= 0)
                enter_timed(SUB_PAT_SEARCH_TURN, PATROL_TURN_MS);
            break;

        default:
            enter_sub(SUB_PAT_RUN);
            break;
    }
}

// ── Comportamiento RETREAT: 180° → borde → 90° → esquina → alto ────────
// Aqui el borde de frente es la CONDICION OBJETIVO, no un peligro:
// el robot gira en el sitio (seguro) o avanza esperando encontrarlo.
static void tick_retreat(const cam_data_t *cam, uint32_t now) {
    switch (g_sub) {
        case SUB_RT_TURN180:
            turnRight(SPEED_TURN);
            setLED(120, 0, 120);
            if ((int32_t)(now - g_sub_until) >= 0) enter_sub(SUB_RT_DRIVE);
            break;

        case SUB_RT_DRIVE:
            setLED(120, 0, 120);
            if (cam->dist[1] == 2 || (cam->dist[0] == 2 && cam->dist[2] == 2)) {
                motorStop();
                enter_timed(SUB_RT_TURN90, TURN_90_MS);
            } else {
                moveForward(SPEED_LINEAR);
            }
            break;

        case SUB_RT_TURN90:
            turnLeft(SPEED_TURN);   // la cinta queda a la derecha
            setLED(120, 0, 120);
            if ((int32_t)(now - g_sub_until) >= 0) enter_sub(SUB_RT_EDGE);
            break;

        case SUB_RT_EDGE:
            setLED(120, 0, 120);
            if (cam->dist[1] == 2) {        // borde de frente + borde al lado = esquina
                motorStop();
                enter_sub(SUB_RT_HOLD);
                ESP_LOGI(TAG, "[RETREAT] esquina alcanzada");
            } else if (cam->dist[2] == 2) { // pegado a la cinta derecha: corregir
                turnLeft(SPEED_TURN);
            } else {
                moveForward(SPEED_LINEAR);
            }
            break;

        case SUB_RT_HOLD:
            motorStop();
            setLED(50, 50, 50);
            break;

        default:
            enter_timed(SUB_RT_TURN180, TURN_180_MS);
            break;
    }
}

// ── Lazo de control (20 Hz) ─────────────────────────────────────────────
static void control_task(void *) {
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CTRL_TICK_MS));

        robot_mode_t mode = g_mode;
        uint32_t     now  = millis();

        cam_data_t cam;
        uint32_t   age   = 0;
        bool       have  = uart_link_get(&cam, &age);
        g_link_ok = have && (age <= UART_TIMEOUT_MS);
        if (have) {
            portENTER_CRITICAL(&s_lock);
            g_cam        = cam;
            g_cam_age_ms = age;
            portEXIT_CRITICAL(&s_lock);
        }

        // MANUAL: los handlers HTTP controlan los motores directamente
        if (mode == MODE_MANUAL) continue;

        // Parada de seguridad: sin datos frescos de la camara
        if (!g_link_ok) {
            motorStop();
            if ((now / 250) % 2) setLED(180, 0, 0);
            else                 setLED(0, 0, 0);
            if (!g_link_was_lost) {
                g_link_was_lost = true;
                ESP_LOGW(TAG, "[SAFE] enlace camara perdido — motores detenidos");
            }
            continue;
        }
        if (g_link_was_lost) {
            g_link_was_lost = false;
            ESP_LOGI(TAG, "[SAFE] enlace camara recuperado");
            // Reanudar desde un subestado seguro
            if      (mode == MODE_FIND)   enter_sub(SUB_FD_SCAN);
            else if (mode == MODE_PATROL) enter_sub(SUB_PAT_RUN);
            // RETREAT continua donde estaba (giros en el sitio son seguros)
        }

        // Override global de borde: nunca cruzar la cinta
        bool border_near = (cam.dist[1] == 2) ||
                           (cam.dist[0] == 2 && cam.dist[2] == 2);

        switch (mode) {
            case MODE_FIND:    tick_find(&cam, now, border_near);   break;
            case MODE_PATROL:  tick_patrol(&cam, now, border_near); break;
            case MODE_RETREAT: tick_retreat(&cam, now);             break;
            default: break;
        }
    }
}

int control_status_json(char *buf, size_t n) {
    portENTER_CRITICAL(&s_lock);
    robot_mode_t mode = g_mode;
    sub_state_t  sub  = g_sub;
    cam_data_t   cam  = g_cam;
    uint32_t     age  = g_cam_age_ms;
    portEXIT_CRITICAL(&s_lock);
    bool link = g_link_ok;

    return snprintf(buf, n,
        "{\"mode\":\"%s\",\"sub\":\"%s\",\"action\":\"%s\","
        "\"dist\":[%d,%d,%d],\"ident\":%d,\"conf\":%d,"
        "\"link\":%s,\"age_ms\":%lu}",
        mode_name(mode), mode == MODE_MANUAL ? "manual" : sub_name(sub),
        cam.action[0] ? cam.action : "none",
        (int)cam.dist[0], (int)cam.dist[1], (int)cam.dist[2],
        cam.ident ? 1 : 0, cam.conf,
        link ? "true" : "false", (unsigned long)age);
}

void control_start(void) {
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Lazo de control iniciado (%d ms)", CTRL_TICK_MS);
}
