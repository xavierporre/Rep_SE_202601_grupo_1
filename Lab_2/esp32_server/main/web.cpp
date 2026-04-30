/*
 * Control de Auto RC — ESP32-S3 + L298N
 * HTML del controlador embebido — acceder en http://192.168.4.1
 *
 * LED RGB integrado (GPIO 48):
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

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// ── LED ────────────────────────────────────────────────────
#define LED_PIN   48
#define LED_COUNT  1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
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

WebServer server(80);

// ── HTML embebido ──────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawhtml(
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
    <button class="db" id="btn-fwd"  data-cmd="forward">&#9650;</button>
    <div class="de"></div>
    <button class="db" id="btn-left" data-cmd="left">&#9664;</button>
    <button class="ds" onclick="forceStop()">STOP</button>
    <button class="db" id="btn-right"data-cmd="right">&#9654;</button>
    <div class="de"></div>
    <button class="db" id="btn-back" data-cmd="backward">&#9660;</button>
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
    cs.textContent='ONLINE'; cs.className='sv on';
  }catch(e){
    addLog('ERR '+cmd+': '+e.message,'err');
    const cs=document.getElementById('cs');
    cs.textContent='ERROR'; cs.className='sv off';
  }
}

function forceStop(){
  clearInterval(holdIv); holdIv=null;
  document.querySelectorAll('.db').forEach(b=>b.classList.remove('pressed'));
  sendCmd('stop');
}

document.querySelectorAll('.db[data-cmd]').forEach(btn=>{
  const cmd=btn.dataset.cmd;
  const start=e=>{
    e.preventDefault();
    if(btn.classList.contains('pressed'))return;
    btn.classList.add('pressed');
    sendCmd(cmd);
    holdIv=setInterval(()=>sendCmd(cmd),250);
  };
  const stop=e=>{
    e.preventDefault();
    btn.classList.remove('pressed');
    clearInterval(holdIv); holdIv=null;
    sendCmd('stop');
  };
  btn.addEventListener('mousedown',start);
  btn.addEventListener('touchstart',start,{passive:false});
  btn.addEventListener('mouseup',stop);
  btn.addEventListener('mouseleave',stop);
  btn.addEventListener('touchend',stop);
  btn.addEventListener('touchcancel',stop);
});

const km={ArrowUp:'btn-fwd',ArrowDown:'btn-back',ArrowLeft:'btn-left',ArrowRight:'btn-right',w:'btn-fwd',s:'btn-back',a:'btn-left',d:'btn-right'};
const held=new Set();
document.addEventListener('keydown',e=>{
  if(held.has(e.key))return;
  const id=km[e.key]||km[e.key.toLowerCase()];
  if(!id)return;
  e.preventDefault(); held.add(e.key);
  document.getElementById(id)?.dispatchEvent(new MouseEvent('mousedown'));
});
document.addEventListener('keyup',e=>{
  held.delete(e.key);
  const id=km[e.key]||km[e.key.toLowerCase()];
  if(id) document.getElementById(id)?.dispatchEvent(new MouseEvent('mouseup'));
});

addLog('Conectado al ESP32-S3. Usa los botones o WASD.','info');
</script>
</body>
</html>
)rawhtml";

// ── Helpers HTTP ───────────────────────────────────────────
void cors() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
}

int getSpd() {
  return server.hasArg("speed")
    ? constrain(server.arg("speed").toInt(), 0, 100)
    : 70;
}

// ── Helpers motor ──────────────────────────────────────────
// void setPWM(int s){ int v=map(s,0,100,0,255); ledcWrite(PWM_CH_A,v); ledcWrite(PWM_CH_B,v); }
// void motorStop()        { ledcWrite(PWM_CH_A,0);ledcWrite(PWM_CH_B,0);digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);digitalWrite(IN3,LOW);digitalWrite(IN4,LOW); }
// void moveForward(int s) { digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW); digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW);  setPWM(s); }
// void moveBackward(int s){ digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); setPWM(s); }
// void turnLeft(int s)    { digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW);  setPWM(s); }
// void turnRight(int s)   { digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); setPWM(s); }

// ── Handlers ───────────────────────────────────────────────
void hRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void hForward() {
  int s = getSpd();
  setLED(0, 180, 0);
  // moveForward(s);
  cors(); server.send(200, "text/plain", "forward:" + String(s));
  Serial.println("ADELANTE  spd=" + String(s));
}

void hBackward() {
  int s = getSpd();
  setLED(180, 0, 0);
  // moveBackward(s);
  cors(); server.send(200, "text/plain", "backward:" + String(s));
  Serial.println("ATRAS     spd=" + String(s));
}

void hLeft() {
  int s = getSpd();
  setLED(0, 0, 180);
  // turnLeft(s);
  cors(); server.send(200, "text/plain", "left:" + String(s));
  Serial.println("IZQUIERDA spd=" + String(s));
}

void hRight() {
  int s = getSpd();
  setLED(180, 180, 0);
  // turnRight(s);
  cors(); server.send(200, "text/plain", "right:" + String(s));
  Serial.println("DERECHA   spd=" + String(s));
}

void hStop() {
  setLED(0, 0, 0);
  // motorStop();
  cors(); server.send(200, "text/plain", "stop");
  Serial.println("STOP");
}

void hPing() {
  setLED(80, 0, 80); delay(120);
  setLED(0,  0,  0); delay(80);
  setLED(80, 0, 80); delay(120);
  setLED(0,  0,  0);
  cors(); server.send(200, "text/plain", "pong");
  Serial.println("PING -> pong");
}

void hOptions() { cors(); server.send(204); }

// ── Setup ──────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  led.begin();
  led.setBrightness(80);
  setLED(0, 0, 0);

  // Secuencia de arranque
  setLED(0, 180, 0); delay(200);
  setLED(180, 0, 0); delay(200);
  setLED(0, 0, 180); delay(200);
  setLED(180,180,0); delay(200);
  setLED(0, 0, 0);   delay(100);
  setLED(0, 0, 40);  // azul tenue = esperando

  // ── Descomentar con L298N ──────────────────────────────
  // pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  // pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  // ledcSetup(PWM_CH_A,1000,8); ledcAttachPin(ENA,PWM_CH_A);
  // ledcSetup(PWM_CH_B,1000,8); ledcAttachPin(ENB,PWM_CH_B);
  // motorStop();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  Serial.println("\n=============================");
  Serial.println("  AutoRC - ESP32-S3 listo");
  Serial.print  ("  SSID : "); Serial.println(SSID);
  Serial.print  ("  URL  : http://"); Serial.println(ip);
  Serial.println("=============================\n");

  // Rutas
  server.on("/",         HTTP_GET,     hRoot);
  server.on("/forward",  HTTP_GET,     hForward);
  server.on("/backward", HTTP_GET,     hBackward);
  server.on("/left",     HTTP_GET,     hLeft);
  server.on("/right",    HTTP_GET,     hRight);
  server.on("/stop",     HTTP_GET,     hStop);
  server.on("/ping",     HTTP_GET,     hPing);
  server.on("/forward",  HTTP_OPTIONS, hOptions);
  server.on("/backward", HTTP_OPTIONS, hOptions);
  server.on("/left",     HTTP_OPTIONS, hOptions);
  server.on("/right",    HTTP_OPTIONS, hOptions);
  server.on("/stop",     HTTP_OPTIONS, hOptions);

  server.begin();
  Serial.println("Servidor HTTP puerto 80 - listo\n");
}

// ── Loop ───────────────────────────────────────────────────
void loop() {
  server.handleClient();
}