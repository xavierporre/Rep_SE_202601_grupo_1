/*
 * Control de Auto RC — ESP32-S3
 * LED RGB integrado (GPIO 48, WS2812)
 * Librería requerida: Adafruit NeoPixel
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN   48
#define LED_COUNT  1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define COLOR_OFF      led.Color(0,   0,   0)
#define COLOR_FORWARD  led.Color(0,   180, 0)
#define COLOR_BACKWARD led.Color(180, 0,   0)
#define COLOR_LEFT     led.Color(0,   0,   180)
#define COLOR_RIGHT    led.Color(180, 180, 0)
#define COLOR_PING     led.Color(80,  0,   80)

void setLED(uint32_t color) {
  led.setPixelColor(0, color);
  led.show();
}

const char* SSID     = "AutoRC";
const char* PASSWORD = "12345678";
WebServer server(80);

void cors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
}

int getSpd() {
  return server.hasArg("speed") ? constrain(server.arg("speed").toInt(), 0, 100) : 70;
}

void hForward()  { int s=getSpd(); setLED(COLOR_FORWARD);  cors(); server.send(200,"text/plain","forward:"  +String(s)); Serial.println("ADELANTE  spd="+String(s)); }
void hBackward() { int s=getSpd(); setLED(COLOR_BACKWARD); cors(); server.send(200,"text/plain","backward:" +String(s)); Serial.println("ATRAS     spd="+String(s)); }
void hLeft()     { int s=getSpd(); setLED(COLOR_LEFT);     cors(); server.send(200,"text/plain","left:"     +String(s)); Serial.println("IZQUIERDA spd="+String(s)); }
void hRight()    { int s=getSpd(); setLED(COLOR_RIGHT);    cors(); server.send(200,"text/plain","right:"    +String(s)); Serial.println("DERECHA   spd="+String(s)); }
void hStop()     { setLED(COLOR_OFF); cors(); server.send(200,"text/plain","stop"); Serial.println("STOP"); }

void hPing() {
  setLED(COLOR_PING); delay(120);
  setLED(COLOR_OFF);  delay(80);
  setLED(COLOR_PING); delay(120);
  setLED(COLOR_OFF);
  cors();
  server.send(200, "text/plain", "pong");
  Serial.println("PING -> pong");
}

void hOptions() { cors(); server.send(204); }

void setup() {
  Serial.begin(115200);
  delay(500);

  led.begin();
  led.setBrightness(80);
  setLED(COLOR_OFF);

  // Secuencia de arranque
  setLED(COLOR_FORWARD);  delay(200);
  setLED(COLOR_BACKWARD); delay(200);
  setLED(COLOR_LEFT);     delay(200);
  setLED(COLOR_RIGHT);    delay(200);
  setLED(COLOR_OFF);      delay(100);

  // Azul tenue = esperando conexion
  setLED(led.Color(0, 0, 40));

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  Serial.println("\n=============================");
  Serial.println("  AutoRC - ESP32-S3 listo");
  Serial.print  ("  SSID : "); Serial.println(SSID);
  Serial.print  ("  IP   : "); Serial.println(ip);
  Serial.println("=============================\n");

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

void loop() {
  server.handleClient();
}