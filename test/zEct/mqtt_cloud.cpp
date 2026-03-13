#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h> // PENTING: Library untuk koneksi aman (SSL)
#include <PubSubClient.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "Batari Energy";
const char* password = "hidupitukeras";

// --- KONFIGURASI HIVEMQ CLOUD ---
// Masukkan URL Cluster Anda (tanpa http/mqtt)
const char* mqtt_server = "37d12fc61b8641adb496ecc91a0d2539.s1.eu.hivemq.cloud"; 
// Port HiveMQ Cloud (Wajib 8883 untuk SSL)
const int mqtt_port = 8883;
// User & Pass yang dibuat di Access Management
const char* mqtt_user = "iot_batari";
const char* mqtt_pass = "@IoTbatari123";

// Gunakan WiFiClientSecure, bukan WiFiClient biasa
WiFiClientSecure espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terkoneksi!");
  
  // --- BAGIAN PENTING (SSL) ---
  espClient.setInsecure(); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Pesan masuk [");
  Serial.print(topic);
  Serial.print("]: ");
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.println(msg);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke HiveMQ Cloud...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Connect menggunakan User & Password
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Berhasil!");
      client.subscribe("test/topic");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi 5 detik");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Kirim data
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;
    String pesan = "Halo dari ESP32 via Cloud: " + String(millis());
    client.publish("test/topic", pesan.c_str());
    Serial.println("Publish: " + pesan);
  }
}