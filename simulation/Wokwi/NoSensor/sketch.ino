#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h> 

// Definição dos pinos
#define DHT_PIN 23      // Pino do sensor DHT22
#define LED_PIN 18      // Pino do LED
#define PIR_PIN 13      // Pino do sensor PIR
#define MQ135_PIN 4     // Pino do sensor de gás MQ-2 (ADC)
#define BUZZER_PIN 12   // Pino do buzzer
#define DHT_TYPE DHT22  // Tipo do sensor DHT

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient ESP_MA;
PubSubClient client(ESP_MA);
DHT dht(DHT_PIN, DHT_TYPE);
int pirState = LOW;

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
  tone(BUZZER_PIN, frequency, duration);
  delay(duration + 50);
  noTone(BUZZER_PIN);
}

void publishSensorData(float temp, float hum, float gas, bool movimento) {
  // Converte e publica os dados
  char buffer[50];

  dtostrf(temp, 4, 2, buffer);
  client.publish("sensor/temperatura", buffer);

  dtostrf(hum, 4, 1, buffer);
  client.publish("sensor/umidade", buffer);

  dtostrf(gas, 4, 2, buffer);
  client.publish("sensor/fumaca", buffer);

  const char* movStr = movimento ? "1" : "0";
  client.publish("sensor/movimento", movStr);

  Serial.println("📤 Dados publicados no MQTT!");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setup_wifi(); 
  client.setServer(mqtt_server, 1883);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
   client.loop();

  // Leitura do sensor DHT22
  float temperature = dht.readTemperature(); 
  float humidity = dht.readHumidity();
  
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("🌡️ Temperatura: %.2f°C\n", temperature);
    Serial.printf("💧 Umidade: %.1f%%\n", humidity);
  } else {
    Serial.println("⚠️ Erro ao ler DHT22!");
  }

  // Leitura do sensor PIR
  int pirVal = digitalRead(PIR_PIN);
  bool movimento = (pirVal == HIGH);
  if (pirVal == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    if (pirState == LOW) {
      Serial.println("⚠️ Alerta! Movimento detectado ⚠️");
      pirState = HIGH;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    if (pirState == HIGH) {
      Serial.println("✅ Ausência de Movimento!");
      pirState = LOW;
    }
  }

  // Leitura do sensor de Gás MQ-2
  int gasValue = analogRead(MQ135_PIN);
  float gasPercent = (gasValue / 4095.0) * 100.0;
  Serial.printf("🔥 Concentração de fumaça: %.2f%%\n", gasPercent);

  if (gasPercent > 30.0) {
    Serial.println("⚠️ Alerta! Concentração de fumaça elevada! ⚠️");
    int melody[] = {261, 293, 329, 349, 392};
    for (int i = 0; i < 5; i++) {
      beep(melody[i], 200);
    }
  }
  publishSensorData(temperature, humidity, gasPercent, movimento);
  Serial.println("*********************************************");
  delay(3000);
}
