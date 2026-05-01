/*
 * Control de Auto RC — ESP32-S3 + L298N
 * HTML del controlador embebido — acceder en http://192.168.4.1
 *
 * LED RGB integrado WS2812 (GPIO 48) via espressif/led_strip:
 *   Adelante  → Verde
 *   Atras     → Rojo
 *   Izquierda → Azul
 *   Derecha   → Amarillo
 *   Stop      → Apagado
 *
 * Pines L298N (descomentar cuando conectes el puente H):
 *   ENA → GPIO 4   ENB → GPIO 7
 *   IN1 → GPIO 5   IN3 → GPIO 15
 *   IN2 → GPIO 6   IN4 → GPIO 16
 */

#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdlib>
#include "led_strip.h"

static const char* TAG = "AutoRC";

// ── LED WS2812 integrado ───────────────────────────────────
#define LED_GPIO    48
#define LED_RMT_CH   0

static led_strip_handle_t strip;

void ledInit() {
  led_strip_config_t cfg = {
    .strip_gpio_num = LED_GPIO,
    .max_leds = 1,
    .led_model = LED_MODEL_WS2812,
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    .flags = {
      .invert_out = false,
    },
  };

  led_strip_rmt_config_t rmt = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,
    .mem_block_symbols = 64,
    .flags = {
      .with_dma = false,
    },
  };

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &strip));
  led_strip_clear(strip);
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  led_strip_set_pixel(strip, 0, r, g, b);
  led_strip_refresh(strip);
}

// ── Pines L298N ────────────────────────────────────────────
// #define ENA  4
// #define IN1  5
// #define IN2  6
// #define ENB  7
// #define IN3  15
// #define IN4  16
// #define PWM_CH_A 0
// #define PWM_CH_B 1

// ── WiFi ───────────────────────────────────────────────────
const char* SSID     = "AutoRC";
const char* PASSWORD = "12345678";

// ── HTML embebido ──────────────────────────────────────────
const char INDEX_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Auto RC</title>
<style>
  :root{--bg:#0a0a0f;--panel:#13131a;--panel2:#1c1c26;--acc:#00e5ff;--btn:#1e1e2e;--btnH:#2a2a3e;--btnA:#00e5ff22;--txt:#e8e8f0;--muted:#6b6b80;--green:#00ff88;--red:#ff4466;}
  *{box-sizing:border-box;margin:0;padding:0;}
  html,body{height:100%;background:var(--bg);color:var(--txt);font-family:'Courier New',monospace;overflow:hidden;}
  .app{max-width:420px;margin:0 auto;padding:1.2rem 1rem;height:100vh;display:flex;flex-direction:column;gap:.85rem;}
  .header{text-align:center;}
  .header h1{font-size:1.15rem;letter-spacing:.15em;color:var(--acc);text-transform:uppercase;font-weight:700;}
  .header p{font-size:.65rem;color:var(--muted);letter-spacing:.1em;margin-top:2px;}
  .sbar{display:flex;gap:8px;}
  .si{flex:1;background:var(--panel);border:1px solid #2a2a3a;border-radius:8px;padding:.5rem;text-align:center;}
  .sl{font-size:.56rem;color:var(--muted);letter-spacing:.07em;text-transform:uppercase;}
  .sv{font-size:.8rem;font-weight:700;margin-top:2px;}
  .sv.on{color:var(--green);} .sv.off{color:var(--red);} .sv.dir{color:var(--acc);}
  .dpad{display:grid;grid-template-columns:1fr 1fr 1fr;gap:9px;width:216px;margin:0 auto;}
  .db{background:var(--btn);border:1px solid #2a2a3a;border-radius:12px;color:var(--txt);cursor:pointer;font-size:1.45rem;height:68px;display:flex;align-items:center;justify-content:center;transition:background .07s,border-color .07s;user-select:none;-webkit-user-select:none;touch-action:manipulation;-webkit-tap-highlight-color:transparent;}
  .db:hover{background:var(--btnH);border-color:var(--acc);}
  .db.pressed{background:var(--btnA);border-color:var(--acc);box-shadow:0 0 14px #00e5ff44;}
  .de{visibility:hidden;}
  .ds{background:var(--panel);border:1px solid #2a2a3a;border-radius:50%;cursor:pointer;font-size:.5rem;color:var(--muted);letter-spacing:.05em;text-transform:uppercase;display:flex;align-items:center;justify-content:center;height:68px;touch-action:manipulation;}
  .ds:active{border-color:var(--red);color:var(--red);}
  .spd{background:var(--panel);border:1px solid #2a2a3a;border-radius:10px;padding:.75rem .9rem;}
  .sph{display:flex;justify-content:space-between;align-items:center;margin-bottom:.5rem;}
  .sph span{font-size:.6rem;color:var(--muted);letter-spacing:.07em;text-transform:uppercase;}
  .sph strong{font-size:.88rem;color:var(--acc);}
  input[type=range]{width:100%;accent-color:var(--acc);cursor:pointer;}
  .spll{display:flex;justify-content:space-between;font-size:.56rem;color:var(--muted);margin-top:4px;}
  .logw{flex:1;display:flex;flex-direction:column;min-height:0;}
  .logl{font-size:.56rem;color:var(--muted);letter-spacing:.07em;text-transform:uppercase;margin-bottom:4px;}
  .log{flex:1;background:var(--panel);border:1px solid #2a2a3a;border-radius:10px;padding:.6rem .8rem;overflow-y:auto;font-size:.64rem;color:var(--muted);min-height:55px;}
  .ll{margin-bottom:2px;line-height:1.5;}
  .ll.ok{color:var(--green);} .ll.err{color:var(--red);} .ll.info{color:var(--acc);}
</style>
</head>
<body>
<div class="app">
  <div class="header">
    <h1>&#9632; AUTO CONTROL</h1>
    <p>RC CAR &mdash; WIRELESS CONTROLLER</p>
  </div>
  <div class="sbar">
    <div class="si"><div class="sl">Conexion</div><div class="sv off" id="cs">OFFLINE</div></div>
    <div class="si"><div class="sl">Direccion</div><div class="sv dir" id="ds2">STOP</div></div>
    <div class="si"><div class="sl">Velocidad</div><div class="sv dir" id="ss">70%</div></div>
  </div>
  <div class="dpad">
    <div class="de"></div>
    <button class="db" id="btn-fwd"   data-cmd="forward">&#9650;</button>
    <div class="de"></div>
    <button class="db" id="btn-left"  data-cmd="left">&#9664;</button>
    <button class="ds" onclick="forceStop()">STOP</button>
    <button class="db" id="btn-right" data-cmd="right">&#9654;</button>
    <div class="de"></div>
    <button class="db" id="btn-back"  data-cmd="backward">&#9660;</button>
    <div class="de"></div>
  </div>
  <div class="spd">
    <div class="sph"><span>Velocidad (PWM)</span><strong id="sl2">70%</strong></div>
    <input type="range" id="spd-slider" min="0" max="100" value="70" step="5" oninput="updateSpd(this.value)"/>
    <div class="spll"><span>0%</span><span>25%</span><span>50%</span><span>75%</span><span>100%</span></div>
  </div>
  <div class="logw">
    <div class="logl">Log</div>
    <div class="log" id="log"></div>
  </div>
</div>
<script>
let spd=70,holdIv=null;
function addLog(msg,t='info'){
  const log=document.getElementById('log');
  const d=document.createElement('div');
  d.className='ll '+t;
  d.textContent='['+new Date().toTimeString().slice(0,8)+'] '+msg;
  log.appendChild(d);
  if(log.children.length>60)log.removeChild(log.firstChild);
  log.scrollTop=log.scrollHeight;
}
function updateSpd(v){
  spd=parseInt(v);
  document.getElementById('sl2').textContent=spd+'%';
  document.getElementById('ss').textContent=spd+'%';
}
async function sendCmd(cmd){
  try{
    await fetch('/'+cmd+'?speed='+spd,{method:'GET',cache:'no-store'});
    addLog('-> '+cmd.toUpperCase()+' spd='+spd+'%','ok');
    document.getElementById('ds2').textContent=cmd==='stop'?'STOP':cmd.toUpperCase();
    const cs=document.getElementById('cs');
    cs.textContent='ONLINE';cs.className='sv on';
  }catch(e){
    addLog('ERR '+cmd+': '+e.message,'err');
    const cs=document.getElementById('cs');
    cs.textContent='ERROR';cs.className='sv off';
  }
}
function forceStop(){
  clearInterval(holdIv);holdIv=null;
  document.querySelectorAll('.db').forEach(b=>b.classList.remove('pressed'));
  sendCmd('stop');
}
document.querySelectorAll('.db[data-cmd]').forEach(btn=>{
  const cmd=btn.dataset.cmd;
  const start=e=>{e.preventDefault();if(btn.classList.contains('pressed'))return;btn.classList.add('pressed');sendCmd(cmd);holdIv=setInterval(()=>sendCmd(cmd),250);};
  const stop=e=>{e.preventDefault();btn.classList.remove('pressed');clearInterval(holdIv);holdIv=null;sendCmd('stop');};
  btn.addEventListener('mousedown',start);btn.addEventListener('touchstart',start,{passive:false});
  btn.addEventListener('mouseup',stop);btn.addEventListener('mouseleave',stop);
  btn.addEventListener('touchend',stop);btn.addEventListener('touchcancel',stop);
});
const km={ArrowUp:'btn-fwd',ArrowDown:'btn-back',ArrowLeft:'btn-left',ArrowRight:'btn-right',w:'btn-fwd',s:'btn-back',a:'btn-left',d:'btn-right'};
const held=new Set();
document.addEventListener('keydown',e=>{if(held.has(e.key))return;const id=km[e.key]||km[e.key.toLowerCase()];if(!id)return;e.preventDefault();held.add(e.key);document.getElementById(id)?.dispatchEvent(new MouseEvent('mousedown'));});
document.addEventListener('keyup',e=>{held.delete(e.key);const id=km[e.key]||km[e.key.toLowerCase()];if(id)document.getElementById(id)?.dispatchEvent(new MouseEvent('mouseup'));});
addLog('Conectado al ESP32-S3. Usa los botones o WASD.','info');
</script>
</body>
</html>
)rawhtml";

// ── Helpers HTTP ───────────────────────────────────────────
void cors(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin",  "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
}

int getSpd(httpd_req_t *req) {
  char buf[10] = {0};
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf) - 1) == ESP_OK) {
    char value[5] = {0};
    if (httpd_query_key_value(buf, "speed", value, sizeof(value) - 1) == ESP_OK) {
      int s = atoi(value);
      return (s < 0) ? 0 : (s > 100) ? 100 : s;
    }
  }
  return 70;
}

// ── Helpers motor (descomentar con L298N) ──────────────────
// void setPWM(int s){int v=map(s,0,100,0,255);ledcWrite(PWM_CH_A,v);ledcWrite(PWM_CH_B,v);}
// void motorStop()        {ledcWrite(PWM_CH_A,0);ledcWrite(PWM_CH_B,0);digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);digitalWrite(IN3,LOW);digitalWrite(IN4,LOW);}
// void moveForward(int s) {digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW);digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW); setPWM(s);}
// void moveBackward(int s){digitalWrite(IN1,LOW);digitalWrite(IN2,HIGH);digitalWrite(IN3,LOW);digitalWrite(IN4,HIGH);setPWM(s);}
// void turnLeft(int s)    {digitalWrite(IN1,LOW);digitalWrite(IN2,HIGH);digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW); setPWM(s);}
// void turnRight(int s)   {digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW);digitalWrite(IN3,LOW);digitalWrite(IN4,HIGH);setPWM(s);}

// ── Handlers ───────────────────────────────────────────────
static esp_err_t hRoot(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
  return ESP_OK;
}

static esp_err_t hForward(httpd_req_t *req) {
  int s = getSpd(req);
  setLED(0, 180, 0);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  char buf[50];
  snprintf(buf, sizeof(buf), "forward:%d", s);
  httpd_resp_send(req, buf, strlen(buf));
  ESP_LOGI(TAG, "ADELANTE  spd=%d", s);
  return ESP_OK;
}

static esp_err_t hBackward(httpd_req_t *req) {
  int s = getSpd(req);
  setLED(180, 0, 0);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  char buf[50];
  snprintf(buf, sizeof(buf), "backward:%d", s);
  httpd_resp_send(req, buf, strlen(buf));
  ESP_LOGI(TAG, "ATRAS     spd=%d", s);
  return ESP_OK;
}

static esp_err_t hLeft(httpd_req_t *req) {
  int s = getSpd(req);
  setLED(0, 0, 180);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  char buf[50];
  snprintf(buf, sizeof(buf), "left:%d", s);
  httpd_resp_send(req, buf, strlen(buf));
  ESP_LOGI(TAG, "IZQUIERDA spd=%d", s);
  return ESP_OK;
}

static esp_err_t hRight(httpd_req_t *req) {
  int s = getSpd(req);
  setLED(180, 180, 0);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  char buf[50];
  snprintf(buf, sizeof(buf), "right:%d", s);
  httpd_resp_send(req, buf, strlen(buf));
  ESP_LOGI(TAG, "DERECHA   spd=%d", s);
  return ESP_OK;
}

static esp_err_t hStop(httpd_req_t *req) {
  setLED(0, 0, 0);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "stop", 4);
  ESP_LOGI(TAG, "STOP");
  return ESP_OK;
}

static esp_err_t hOptions(httpd_req_t *req) {
  cors(req);
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static esp_err_t hPing(httpd_req_t *req) {
  setLED(80, 0, 80);
  vTaskDelay(120 / portTICK_PERIOD_MS);
  setLED(0, 0, 0);
  vTaskDelay(80 / portTICK_PERIOD_MS);
  setLED(80, 0, 80);
  vTaskDelay(120 / portTICK_PERIOD_MS);
  setLED(0, 0, 0);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "pong", 4);
  ESP_LOGI(TAG, "PING -> pong");
  return ESP_OK;
}

// ── App ────────────────────────────────────────────────────
extern "C" void app_main() {
  ESP_LOGI(TAG, "Inicializando...");
  
  // Inicializar NVS (requerido por WiFi)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGI(TAG, "Borrando NVS...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  
  ledInit();

  // Secuencia de arranque
  setLED(0, 180, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  setLED(180, 0, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  setLED(0, 0, 180);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  setLED(180, 180, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  setLED(0, 0, 0);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  setLED(0, 0, 40);

  // WiFi
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_config_t wifi_cfg = {};
  strcpy((char *)wifi_cfg.ap.ssid, SSID);
  strcpy((char *)wifi_cfg.ap.password, PASSWORD);
  wifi_cfg.ap.ssid_len = strlen(SSID);
  wifi_cfg.ap.channel = 6;
  wifi_cfg.ap.max_connection = 4;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(ap_netif, &ip_info);

  ESP_LOGI(TAG, "=============================");
  ESP_LOGI(TAG, "  AutoRC - ESP32-S3 listo");
  ESP_LOGI(TAG, "  SSID : %s", SSID);
  ESP_LOGI(TAG, "  URL  : http://" IPSTR, IP2STR(&ip_info.ip));
  ESP_LOGI(TAG, "=============================");

  // HTTP Server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = hRoot, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_root);

  httpd_uri_t uri_forward = {.uri = "/forward", .method = HTTP_GET, .handler = hForward, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_forward);

  httpd_uri_t uri_backward = {.uri = "/backward", .method = HTTP_GET, .handler = hBackward, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_backward);

  httpd_uri_t uri_left = {.uri = "/left", .method = HTTP_GET, .handler = hLeft, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_left);

  httpd_uri_t uri_right = {.uri = "/right", .method = HTTP_GET, .handler = hRight, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_right);

  httpd_uri_t uri_stop = {.uri = "/stop", .method = HTTP_GET, .handler = hStop, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_stop);

  httpd_uri_t uri_ping = {.uri = "/ping", .method = HTTP_GET, .handler = hPing, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_ping);

  httpd_uri_t uri_options = {.uri = "/*", .method = HTTP_OPTIONS, .handler = hOptions, .user_ctx = NULL};
  httpd_register_uri_handler(server, &uri_options);

  ESP_LOGI(TAG, "Servidor HTTP listo en puerto 80");
}