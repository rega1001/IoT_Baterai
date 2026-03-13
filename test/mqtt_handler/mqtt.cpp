#include <Arduino.h>
#include "CommsManager.h" // Panggil library kita

// --- KONFIGURASI PENGGUNA ---
const char* mqtt_server = "37d12fc61b8641adb496ecc91a0d2539.s1.eu.hivemq.cloud"; 
const int   mqtt_port   = 8883;
const char* mqtt_user   = "iot_batari";
const char* mqtt_pass   = "@IoTbatari123";
const char* clientId    = "BMSClient-eboat-01";
const int   BUTTON_PIN  = 0;
const char* device_name = "BySense-0001"; 
const char* inv_name    = "Inv-0001";
const char* site_name   = "";

CommsManager comms(mqtt_server, mqtt_port, mqtt_user, mqtt_pass, clientId, BUTTON_PIN, device_name);

// Fungsi penerima pesan MQTT (Callback)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Pesan masuk [%s]: ", topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // 1. Mulai jaringan (WiFi/BT/MQTT)
  comms.begin();

  // 2. Daftarkan fungsi callback ke MQTT
  comms.getMqttClient().setCallback(mqttCallback);

  Serial.println("Sistem Utama Siap!");
}

void loop() {
  // Wajib panggil ini agar WiFi, BT, dan Reconnect MQTT berjalan di background
  comms.loop();

  // --- LOGIKA UTAMA PROGRAM ANDA DI SINI ---
  static unsigned long lastKirim = 0;
  
  // Pastikan kita hanya mengirim (publish) saat MQTT terkoneksi
  if (millis() - lastKirim > 15000) {
    lastKirim = millis();
    
    if (comms.getMqttClient().connected()) {
      if (site_name == "") { site_name = comms.getSiteName().c_str(); }
      // Kirim data ke MQTT
    }
  }
}