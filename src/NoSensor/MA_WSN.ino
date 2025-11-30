#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Pinos para ESP32S
#define DHT_PIN 4           // Sensor DHT22 (GPIO4)
#define LED_PIN 21           // GPIO2: LED
#define MQ135_PIN 34        // GPIO34: Entrada analógica
#define BUZZER_PIN 13       // GPIO13: Buzzer
#define DHT_TYPE DHT22

const float TEMP_CRITICA = 30.0;

// Wi-Fi e MQTT
const char* ssid = "K01b3-2|";
const char* password = "@n0n1m0uS";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient ESP_MA;
PubSubClient client(ESP_MA);
DHT dht(DHT_PIN, DHT_TYPE);

// PWM para buzzer usando nova API (core 3.x)
const int pwmFreq = 2000;
const int pwmResolution = 8;
const int pwmChannel = 0;  // canal 0 (usado em ledcWriteTone)

void setup_wifi() {
  Serial.print("Conectando-se à rede Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar! Reiniciando...");
    ESP.restart();
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Tentando conexão com Servidor MQTT...");
    if (client.connect("ESP_MA")) {
      Serial.println("Conectado ao Servidor MQTT");
    } else {
      Serial.print("Falha na conexão MQTT, erro: ");
      Serial.println(client.state());
      Serial.println("Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

void beep(int frequency, int duration) {
  ledcWriteTone(pwmChannel, frequency);  // liga tom
  delay(duration);
  ledcWriteTone(pwmChannel, 0);          // desliga
}

void publishSensorData(float temp, float hum, float gas) {
  char buffer[50];

  dtostrf(temp, 4, 2, buffer);
  client.publish("sensor/temperatura", buffer);

  dtostrf(hum, 4, 1, buffer);
  client.publish("sensor/umidade", buffer);

  dtostrf(gas, 4, 2, buffer);
  client.publish("sensor/fumaca", buffer);

  Serial.println("📤 Dados publicados no MQTT!");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(LED_PIN, OUTPUT);
  pinMode(DHT_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // PWM para buzzer usando ledcAttach (nova API do core 3.x)
  ledcAttach(BUZZER_PIN, pwmFreq, pwmResolution);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  Serial.println("*********************************************");

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("🌡️ Temperatura: %.2f°C\n", temperature);
    Serial.printf("💧 Humidade: %.1f%%\n", humidity);

    if (temperature >= TEMP_CRITICA) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("🚨 Temperatura crítica!🚨");
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    Serial.println("⚠️ Erro ao ler DHT22!");
  }

  // Leitura do MQ-135
  int gasValue = analogRead(MQ135_PIN);
  float gasPercent = (gasValue / 4095.0) * 100.0;
  Serial.printf("🔥 Concentração de fumaça: %.2f%%\n", gasPercent);

  if (gasPercent > 10.0) {
    Serial.println("🚨 Concentração de fumaça elevada! 🚨");
    int melody[] = {261, 293, 329, 349, 392};
    for (int i = 0; i < 5; i++) {
      beep(melody[i], 200);
    }
  }

  publishSensorData(temperature, humidity, gasPercent);
  Serial.println("*********************************************");

  delay(500);
}