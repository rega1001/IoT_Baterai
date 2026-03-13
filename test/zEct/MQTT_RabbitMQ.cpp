#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "Batari Energy";
const char* password = "hidupitukeras";

// Ganti dengan IP Komputer Anda yang menjalankan RabbitMQ
const char* mqtt_server = "192.168.18.134"; 
const int mqtt_port = 1883;
const char* mqtt_user = "iot_batari";
const char* mqtt_pass = "batari123";

WiFiClient espClient;
PubSubClient client(espClient);

// --- FUNCTION PROTOTYPES (Penting di C++ PlatformIO) ---
// Di Arduino IDE ini tidak wajib, di C++ standar ini wajib dideklarasikan
// sebelum dipanggil, atau letakkan fungsi setup_wifi di atas setup()
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();

void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // Fungsi yang dipanggil saat ada pesan masuk
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Contoh: Kirim data setiap 2 detik
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    String pesan = "Data dummy dari ESP32: " + String(random(20, 30));
    
    // Convert String ke Char Array untuk publish
    client.publish("rumah/sensor/suhu", pesan.c_str());
    Serial.println("Pesan terkirim: " + pesan);
  }
}

// --- IMPLEMENTASI FUNGSI ---

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi terkoneksi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
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

  // Contoh logika kontrol
  if (String(topic) == "rumah/lampu") {
    if (msg == "ON") {
      // Nyalakan Lampu (misal GPIO 2)
      // digitalWrite(2, HIGH);
      Serial.println("-> Lampu dinyalakan!");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("terhubung!");
      // Subscribe ulang topik di sini
      client.subscribe("rumah/#"); 
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}