/*
  Practica: Arduino_Leer_DTH11_Make_Telegram
  Materia:  Desarrollo Sustentable - Instituto Tecnologico de Mazatlan
  Placa:    Arduino UNO R4 WiFi + sensor DHT11 (DATA en pin 2) + LED en pin 13

  Lee temperatura y humedad cada 2 s, muestra los datos en una pagina web
  local y, cuando la temperatura cruza el umbral (28 C), envia un POST JSON
  {"temperatura": valor} al Webhook de Make.com, que manda la alerta por Telegram.

  NOTA: por seguridad se quitaron la red WiFi, la contrasena y la ruta del
  webhook. Reemplaza los valores entre comillas antes de cargar el codigo.
*/

#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "NOMBRE_DE_TU_WIFI";          // red de 2.4 GHz
const char* password = "TU_PASSWORD";
const char* host = "hook.us2.make.com";          // zona us2 de Make
const char* webhookPath = "/TU_WEBHOOK_ID";      // parte despues del dominio

const int ledPin = 13;
const float UMBRAL_TEMP = 28.0;                  // grados Celsius
bool estadoCaliente = false;
float ultimaTemp = 0;
float ultimaHumedad = 0;
unsigned long ultimaLectura = 0;
const unsigned long INTERVALO_LECTURA = 2000;    // minimo recomendado para DHT11

WiFiServer server(80);
WiFiSSLClient client;

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Servidor listo en: http://");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
    ultimaLectura = millis();
    leerYEvaluar();
  }
  WiFiClient webClient = server.available();
  if (webClient) {
    while (webClient.connected() && webClient.available()) {
      char c = webClient.read();
      if (c == '\n') break;
    }
    enviarPagina(webClient);
    webClient.stop();
  }
}

void leerYEvaluar() {
  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();
  if (isnan(humedad) || isnan(temperatura)) {
    Serial.println("Error al leer el sensor DHT11");
    return;
  }
  ultimaTemp = temperatura;
  ultimaHumedad = humedad;

  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.print(" % | Temperatura: ");
  Serial.print(temperatura);
  Serial.print(" *C | ");

  bool estaCalienteAhora = (temperatura >= UMBRAL_TEMP);
  digitalWrite(ledPin, estaCalienteAhora ? HIGH : LOW);
  Serial.println(estaCalienteAhora ? "LED ENCENDIDO (temperatura alta)" : "LED apagado");

  // Solo se avisa a Make en el momento en que se cruza el umbral (flanco de subida)
  if (estaCalienteAhora && !estadoCaliente) {
    enviarAlertaMake(temperatura);
  }
  estadoCaliente = estaCalienteAhora;
}

void enviarPagina(WiFiClient &webClient) {
  webClient.println("HTTP/1.1 200 OK");
  webClient.println("Content-Type: text/html");
  webClient.println("Connection: close");
  webClient.println();
  webClient.println("<!DOCTYPE html><html><head>");
  webClient.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  webClient.println("<meta http-equiv='refresh' content='5'>");
  webClient.println("<style>body{font-family:sans-serif;text-align:center;margin-top:50px;}");
  webClient.println("h1{font-size:28px;}.valor{font-size:48px;font-weight:bold;}</style></head><body>");
  webClient.println("<h1>Monitor del cuarto</h1>");
  webClient.print("<p>Temperatura</p><p class='valor'>");
  webClient.print(ultimaTemp);
  webClient.println(" &deg;C</p>");
  webClient.print("<p>Humedad</p><p class='valor'>");
  webClient.print(ultimaHumedad);
  webClient.println(" %</p>");
  webClient.print("<p>Estado: <b>");
  webClient.print(estadoCaliente ? "CALIENTE (LED encendido)" : "Normal");
  webClient.println("</b></p></body></html>");
}

void enviarAlertaMake(float temp) {
  if (client.connect(host, 443)) {
    String jsonPayload = "{\"temperatura\":" + String(temp) + "}";
    client.println("POST " + String(webhookPath) + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonPayload.length()));
    client.println("Connection: close");
    client.println();
    client.println(jsonPayload);
    Serial.println("Alerta enviada a Make");
  } else {
    Serial.println("Fallo la conexion al webhook");
  }
  client.stop();
}
