#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include "DHT.h"
#define DHTTYPE DHT11  // DHT 11

#define dht_dpin 4
#define LDR_PIN A0  // Pin para la fotocelda
#define LED_PIN 5   // Pin D1 para el LED
DHT dht(dht_dpin, DHTTYPE);

#include "secrets.h"

// Conexión a Wifi
const char ssid[] = "ARRIS-B7AD";
const char pass[] = "Humesanma1111";

// Usuario uniandes
#define HOSTNAME "jd.garciaa1"

// Conexión a Mosquitto
const char MQTT_HOST[] = "iotlab.virtual.uniandes.edu.co";
const int MQTT_PORT = 8082;
const char MQTT_USER[] = "jd.garciaa1";
const char MQTT_PASS[] = "202423665";
const char MQTT_SUB_TOPIC[] = HOSTNAME "/";
//Tópico al que se enviarán los datos de humedad
const char MQTT_PUB_TOPIC1[] = "humedad/cali/" HOSTNAME;
//Tópico al que se enviarán los datos de temperatura
const char MQTT_PUB_TOPIC2[] = "temperatura/cali/" HOSTNAME;
//Tópico al que se enviarán los datos de luminosidad
const char MQTT_PUB_TOPIC3[] = "luminosidad/cali/" HOSTNAME;

//////////////////////////////////////////////////////

#if (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_FINGERPRINT)) or (defined(CHECK_FINGERPRINT) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT) and defined(CHECK_FINGERPRINT))
#error "cant have both CHECK_CA_ROOT and CHECK_PUB_KEY enabled"
#endif

BearSSL::WiFiClientSecure net;
PubSubClient client(net);

time_t now;
unsigned long lastMillis = 0;

//Función que conecta el node a través del protocolo MQTT
void mqtt_connect() {
  while (!client.connected()) {
    Serial.print("Time: ");
    Serial.print(ctime(&now));
    Serial.print("MQTT connecting ... ");
    if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected.");
    } else {
      Serial.println("Problema con la conexión, revise los valores de las constantes MQTT");
      Serial.print("Código de error = ");
      Serial.println(client.state());
      if (client.state() == MQTT_CONNECT_UNAUTHORIZED) {
        ESP.deepSleep(0);
      }
      delay(5000);
    }
  }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

//Configura la conexión del node MCU a Wifi y a Mosquitto
void setup() {
  Serial.begin(115200);

  // Configuración de pines adicionales
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println();
  Serial.println();
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_WRONG_PASSWORD) {
      Serial.print("\nProblema con la conexión, revise los valores de las constantes ssid y pass");
      ESP.deepSleep(0);
    } else if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.print("\nNo se ha logrado conectar con la red, resetee el node y vuelva a intentar");
      ESP.deepSleep(0);
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected!");

  Serial.print("Setting time using SNTP");
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  net.setInsecure();

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(receivedCallback);
  mqtt_connect();
  dht.begin();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Checking wifi");
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
      Serial.print(".");
      delay(10);
    }
    Serial.println("connected");
  } else {
    if (!client.connected()) {
      mqtt_connect();
    } else {
      client.loop();
    }
  }

  now = time(nullptr);
  //Lee los datos del sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Lectura de la fotocelda y mapeo a porcentaje
  int ldrRaw = analogRead(LDR_PIN);
  int luz = map(ldrRaw, 0, 1023, 0, 100);

  // Lógica para encender el LED si la luminosidad es menor a 50
  if (luz < 50) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  //JSON para humedad
  String json = "{\"value\": " + String(h) + "}";
  char payload1[json.length() + 1];
  json.toCharArray(payload1, json.length() + 1);

  //JSON para temperatura
  json = "{\"value\": " + String(t) + "}";
  char payload2[json.length() + 1];
  json.toCharArray(payload2, json.length() + 1);

  //JSON para luminosidad
  json = "{\"value\": " + String(luz) + "}";
  char payload3[json.length() + 1];
  json.toCharArray(payload3, json.length() + 1);

  //Si los valores recolectados no son indefinidos, se envían a los tópicos correspondientes
  if (!isnan(h) && !isnan(t)) {
    client.publish(MQTT_PUB_TOPIC1, payload1, false);
    client.publish(MQTT_PUB_TOPIC2, payload2, false);
    client.publish(MQTT_PUB_TOPIC3, payload3, false);
  }

  //Imprime en el monitor serial la información recolectada
  Serial.print(MQTT_PUB_TOPIC1);
  Serial.print(" -> ");
  Serial.println(payload1);
  Serial.print(MQTT_PUB_TOPIC2);
  Serial.print(" -> ");
  Serial.println(payload2);
  Serial.print(MQTT_PUB_TOPIC3);
  Serial.print(" -> ");
  Serial.println(payload3);

  delay(5000);
}