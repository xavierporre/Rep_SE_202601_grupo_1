#include <Wifi.h>
#include <WebServer.h>

const char* ap_ssid = "SumotronRC";
const char* ap_pass = "embutidos";

const int output1 = 1; //motor1 connceted to GPIO1 --- motor delantero positivo
const int output2 = 2; //motor1 connceted to GPIO2 --- motor delantero invertido

const int output4 = 4; //motor2 connceted to GPIO4 --- motor derecho positivo
const int output5 = 5; //motor2 connceted to GPIO5 --- motor derecho invertido

const int output6 = 6; //motor3 connected to GPIO6 --- motor izquierdo positivo
const int output7 = 7; //motor3 connected to GPIO7 --- motor izquierdo invertido

String output1State = "off";
String output2State = "off";
String output4State = "off";
String output5State = "off";
String output6State = "off";
String output7State = "off";

WebServer server(80);

void advance() {
  output1State = "on";
  digitalWrite(output1, HIGH);
  digitalWrite(output2, LOW);
  digitalWrite(output4, HIGH);
  digitalWrite(output5, LOW);
  digitalWrite(output6, HIGH);
  digitalWrite(output7, LOW);
  handleRoot();
}

void reverse() {
  output1State = "off";
  digitalWrite(output1, LOW);
  digitalWrite(output2, HIGH);
  digitalWrite(output4, LOW);
  digitalWrite(output5, HIGH);
  digitalWrite(output6, LOW);
  digitalWrite(output7, HIGH);
  handleRoot();
}

void left(int reverse) {
  output1State = "on";
  digitalWrite(output4, reverse == 1 ? LOW : HIGH);
  digitalWrite(output5, reverse == 1 ? HIGH : LOW);
  digitalWrite(output6, reverse == 1 ? HIGH : LOW);
  digitalWrite(output7, reverse == 1 ? LOW : HIGH);
  handleRoot();
}

void right(int reverse) {
  output1State = "on";
  digitalWrite(output4, reverse == 1 ? HIGH : LOW);
  digitalWrite(output5, reverse == 1 ? LOW : HIGH);
  digitalWrite(output6, reverse == 1 ? LOW : HIGH);
  digitalWrite(output7, reverse == 1 ? HIGH : LOW);
  handleRoot();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

  html += "<p> Forward </p>";
  if (output1State == "off") {
    html += "<p><a href=\"/1/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/1/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "<p>GPIO 2 - State " + output2State + "</p>";
  if (output2State == "off") {
    html += "<p><a href=\"/2/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/2/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "<p>GPIO 5 - State " + output5State + "</p>";
  if (output5State == "off") {
    html += "<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

}