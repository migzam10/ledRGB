#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char *ssid = "*****";
const char *password = "******";

const int pinRojo = D3;
const int pinVerde = D2;
const int pinAzul = D5;

// Variables de estado
int currentR = 0;
int currentG = 0;
int currentB = 0;
int currentBrightness = 255;
bool ledEncendido = false;

ESP8266WebServer server(80);

// HTML almacenado en Flash (PROGMEM) para evitar fragmentación de RAM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='es'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>LED RGB</title>
  <link href='https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/css/bootstrap.min.css' rel='stylesheet'>
  <style>
    .color-box { width: 50px; height: 50px; margin: 5px; cursor: pointer; border-radius: 8px; border: 1px solid #ccc; }
  </style>
</head>
<body>
  <div class='container mt-4' style='max-width: 600px;'>
    <h1>Control LED RGB</h1>
    
    <div class='mb-3'>
      <label class='form-label text-danger fw-bold'>Rojo</label>
      <input type='range' class='form-range' id='rojo' min='0' max='255' value='0' onchange='enviarDatos()'>
    </div>
    
    <div class='mb-3'>
      <label class='form-label text-success fw-bold'>Verde</label>
      <input type='range' class='form-range' id='verde' min='0' max='255' value='0' onchange='enviarDatos()'>
    </div>
    
    <div class='mb-3'>
      <label class='form-label text-primary fw-bold'>Azul</label>
      <input type='range' class='form-range' id='azul' min='0' max='255' value='0' onchange='enviarDatos()'>
    </div>
    
    <div class='mb-4'>
      <label class='form-label text-warning fw-bold'>Intensidad (Brillo)</label>
      <input type='range' class='form-range' id='brillo' min='0' max='255' value='255' onchange='enviarDatos()'>
    </div>

    <div class='d-flex gap-2 mb-4'>
      <button onclick='estadoLED(true)' class='btn btn-success w-50'>Encender</button>
      <button onclick='estadoLED(false)' class='btn btn-danger w-50'>Apagar</button>
    </div>
    
    <h4>Accesos Rápidos</h4>
    <div class='d-flex flex-wrap'>
       <div class='color-box' style='background-color:rgb(0, 204, 255)' onclick='setColor(0, 204, 255)'></div>
       <div class='color-box' style='background-color:rgb(255, 165, 0)' onclick='setColor(255, 165, 0)'></div>
       <div class='color-box' style='background-color:rgb(255, 0, 255)' onclick='setColor(255, 0, 255)'></div>
       <div class='color-box' style='background-color:rgb(179, 255, 0)' onclick='setColor(179, 255, 0)'></div>
       <div class='color-box' style='background-color:rgb(153, 0, 255)' onclick='setColor(153, 0, 255)'></div>
       <div class='color-box' style='background-color:rgb(255, 0, 98)' onclick='setColor(255, 0, 98)'></div>
    </div>
  </div>

  <script>
    // Setea los valores del input y envía
    function setColor(r, g, b) {
      document.getElementById('rojo').value = r;
      document.getElementById('verde').value = g;
      document.getElementById('azul').value = b;
      enviarDatos();
    }

    // Petición asíncrona para no recargar la web
    function enviarDatos() {
      const r = document.getElementById('rojo').value;
      const g = document.getElementById('verde').value;
      const b = document.getElementById('azul').value;
      const brillo = document.getElementById('brillo').value;
      
      fetch(`/color?rojo=${r}&verde=${g}&azul=${b}&brillo=${brillo}`, { method: 'POST' });
    }

    function estadoLED(encender) {
      fetch(encender ? '/encender' : '/apagar');
    }
  </script>
</body>
</html>
)rawliteral";

// Función centralizada para controlar el hardware
void actualizarHardware() {
  if (!ledEncendido) {
    analogWrite(pinRojo, 0);
    analogWrite(pinVerde, 0);
    analogWrite(pinAzul, 0);
    return;
  }
  
  // Calcula el PWM final aplicando la fórmula del brillo
  int rFinal = (currentR * currentBrightness) / 255;
  int gFinal = (currentG * currentBrightness) / 255;
  int bFinal = (currentB * currentBrightness) / 255;

  analogWrite(pinRojo, rFinal);
  analogWrite(pinVerde, gFinal);
  analogWrite(pinAzul, bFinal);
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleColor() {
  if (server.hasArg("rojo") && server.hasArg("verde") && server.hasArg("azul") && server.hasArg("brillo")) {
    currentR = server.arg("rojo").toInt();
    currentG = server.arg("verde").toInt();
    currentB = server.arg("azul").toInt();
    currentBrightness = server.arg("brillo").toInt();
    
    ledEncendido = true; // Forzamos el encendido al enviar un nuevo color
    actualizarHardware();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleEncender() {
  ledEncendido = true;
  actualizarHardware();
  server.send(200, "text/plain", "Encendido");
}

void handleApagar() {
  ledEncendido = false;
  actualizarHardware();
  server.send(200, "text/plain", "Apagado");
}

void setup() {
  Serial.begin(115200);
  pinMode(pinRojo, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinAzul, OUTPUT);

  actualizarHardware(); // Apaga los pines al iniciar

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP());

  // Iniciar mDNS
  if (MDNS.begin("ledrgb")) {
    Serial.println("mDNS iniciado. Navega a http://ledrgb.local");
  }

  // Rutas
  server.on("/", HTTP_GET, handleRoot);
  server.on("/color", HTTP_POST, handleColor);
  server.on("/encender", HTTP_GET, handleEncender);
  server.on("/apagar", HTTP_GET, handleApagar);

  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  MDNS.update();
  server.handleClient();
}