/*
  Dekoder Protokol UART untuk JK BMS (Versi Gabungan)

  Tujuan Script Ini:
  1. Secara aktif meminta data status lengkap dari BMS secara berkala.
  2. Menerima seluruh paket data ke dalam buffer terlebih dahulu.
  3. Mengurai (decode) data dari buffer setelah paket diterima sepenuhnya.
  4. Mengirim data ke cloud MQTT

  Cara Penggunaan:
  - Hubungkan pin TX dari BMS ke RXD2 (GPIO 16).
  - Hubungkan pin RX dari BMS ke TXD2 (GPIO 17).
  - Pastikan GND terhubung.
  - Upload ke ESP32 dan buka Serial Monitor pada baud rate 115200.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <time.h>
#include "JkBmsDriver.h"
#include "PylontechEncoder.h"

// --- KONFIGURASI PIN ---
#define JK_RX_PIN 26
#define JK_TX_PIN 27

#define INV_RX_PIN 16
#define INV_TX_PIN 17

// --- OBJEK ---
// Driver untuk baca JK BMS
JkBmsDriver jkBms(Serial1, JK_RX_PIN, JK_TX_PIN);
// Encoder untuk Inverter (Versi Pylontech 2.0, Addr 12)
PylontechEncoder pylonEncoder(0x20, 0x12, 0x46);

// Buffer serial Inverter
byte invBuffer[512];
int invIdx = 0;
unsigned long invLastTime = 0;
unsigned long lastMsg = 0;

// --- KONFIGURASI HIVEMQ CLOUD ---
// Masukkan URL Cluster Anda (tanpa http/mqtt)
const char* mqtt_server = "37d12fc61b8641adb496ecc91a0d2539.s1.eu.hivemq.cloud"; 
// Port HiveMQ Cloud (Wajib 8883 untuk SSL)
const int mqtt_port = 8883;
// User & Pass yang dibuat di Access Management
const char* mqtt_user = "iot_batari";
const char* mqtt_pass = "@IoTbatari123";
const char* clientId = "BMSClient-eboat-01";

// --- KONFIGURASI WIFI ---
const int BUTTON_PIN = 0;
// Inisialisasi Object Bluetooth
BluetoothSerial SerialBT;
Preferences pref;
char inputSSID[50];
char inputPASS[50];
bool isWaitingForSSID = true; // Flag untuk tahu sedang nunggu apa
// BUFFER ambil data Serial Bluetooth
const int MAX_LEN = 50; // Sesuaikan panjang maksimal SSID/Password
static char buffer[MAX_LEN]; 
static int pos = 0;

// Gunakan WiFiClientSecure, bukan WiFiClient biasa
WiFiClientSecure espClient;
PubSubClient client(espClient);

void processInverterRequest() {
  // Validasi Header Pylontech (~20...)
  // SOI=0x7E, EOI=0x0D
  if (invBuffer[0] != 0x7E || invBuffer[invIdx-1] != 0x0D) return;

  // Ambil Command ID 2 (Byte ke-7 dan 8 dalam ASCII)
  char cid2_h = invBuffer[7];
  char cid2_l = invBuffer[8];
  
  bool response_needed = false;
  const uint8_t* frame = nullptr;

  // --- MAPPING COMMAND ---
  
  // CMD 0x60: Basic Info (Handshake)
  if (cid2_h == '6' && cid2_l == '0') {
      Serial.println("[INV] Req: Info (0x60)");
      frame = pylonEncoder.buildBasicInfoResponse();
      response_needed = true;
  }
  // CMD 0x61: Analog Data (Volt, Ampere, SOC, dll)
  else if ((cid2_h == '6' && cid2_l == '1') || (cid2_h == '4' && cid2_l == '2')) {
      Serial.println("[INV] Req: Analog Data (0x61)");
      
      // Ambil data terbaru dari JK Driver
      BmsSysData data = jkBms.getData();
      
      // Masukkan ke Encoder
      pylonEncoder.updateData(data);
      
      frame = pylonEncoder.buildAnalogResponse();
      response_needed = true;
  }
  // CMD 0x62: Alarm Info
  else if (cid2_h == '6' && cid2_l == '2') {
      Serial.println("[INV] Req: Alarm (0x62)");
      frame = pylonEncoder.buildAlarmInfoResponse();
      response_needed = true;
  }
  // CMD 0x63: Management Info (Limit Charge/Discharge)
  else if (cid2_h == '6' && cid2_l == '3') {
      Serial.println("[INV] Req: Management (0x63)");
      
      // Update data lagi untuk memastikan limit terbaru
      BmsSysData data = jkBms.getData();
      pylonEncoder.updateData(data);
      
      frame = pylonEncoder.buildManagementResponse();
      response_needed = true;
  }

  // --- KIRIM BALASAN ---
  if (response_needed && frame != nullptr) {
      int len = pylonEncoder.getFrameLength();
      Serial2.write(frame, len);
      
      // Debug: Tampilkan data yang dikirim
      // Serial.printf("Sent %d bytes to Inverter\n", len);
  }
}

// Fungsi terhubung ke internet
void connectToWiFi() {
    delay(10);
    SerialBT.print("Menghubungkan ke ");
    SerialBT.println(inputSSID);
    WiFi.begin(inputSSID, inputPASS);
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        SerialBT.print(".");
        counter++;
        if (counter > 20) {
            SerialBT.println("\n[GAGAL] Tidak bisa konek. Cek password/SSID.");
            inputSSID[0] = '\0'; // Reset SSID
            inputPASS[0] = '\0'; // Reset Password
            pos = 0; // Reset posisi buffer
            isWaitingForSSID = true;
            return;
        }
    }
    SerialBT.println("\nWiFi Terkoneksi!");
    // Buka memori permanen
    pref.begin("wifi_config", false); 
    pref.putString("ssid", inputSSID);
    pref.putString("pass", inputPASS);
    pref.end();
    delay(1000);
    ESP.restart();
}

void setup_wifi() {
    bool inputComplete = false;
    SerialBT.println("Silakan ketik SSID WiFi:");
    while (!inputComplete) {
        if (SerialBT.available()) {
            char c = SerialBT.read();
            // 1. Jika karakter yang dibaca adalah Enter
            if (c == '\n') {
                if (isWaitingForSSID) {
                    // --- SELESAI INPUT SSID ---
                    inputSSID[pos] = '\0'; // Tutup string dengan Null Terminator (PENTING!)
                    SerialBT.print("SSID: ");
                    SerialBT.println(inputSSID);
                    SerialBT.println("Sekarang Ketik PASSWORD:");
                    
                    // Reset posisi ke 0 untuk pengisian Password nanti
                    pos = 0;
                    // Ganti status ke mode Password
                    isWaitingForSSID = false; 
                    
                } else {
                    // --- SELESAI INPUT PASSWORD ---
                    inputPASS[pos] = '\0'; // Tutup string dengan Null Terminator (PENTING!)
                    SerialBT.print("Password: ");
                    SerialBT.println(inputPASS);
                    connectToWiFi();
                    
                    // Reset lagi untuk jaga-jaga mau input ulang
                    pos = 0;
                    isWaitingForSSID = true; // Balik ke awal
                    inputComplete = true; // Keluar dari loop input
                }
            } 
            // 2. Abaikan karakter '\r' (sisaan enter di Windows/Android)
            else if (c == '\r') { /* Do nothing */ }
            // 3. Jika bukan Enter, simpan hurufnya LANGSUNG ke variabel
            else {
                if (pos < 49) { // Pastikan tidak over limit (maks 50, sisa 1 buat null)
                    if (isWaitingForSSID) {
                    inputSSID[pos] = c; // Masuk langsung ke SSID
                    } else {
                    inputPASS[pos] = c; // Masuk langsung ke PASS
                    }
                    pos++;
                }
            }
        }
    }
}

// Fungsi MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Pesan masuk [");
    Serial.print(topic);
    Serial.print("]: ");
    char msg[100]; 
    if (length >= sizeof(msg)) {length = sizeof(msg) - 1;}
    for (int i = 0; i < length; i++) {
        msg[i] = (char)payload[i];
    }
    msg[length] = '\0';
    Serial.println(msg);
}

void reconnect() {
    Serial.print("Menghubungkan ke HiveMQ Cloud...");
    if (client.connect(clientId, mqtt_user, mqtt_pass)) {
        Serial.println("Berhasil!");
    } else {
        Serial.print("Gagal, rc=");
        Serial.print(client.state());
        Serial.println(" coba lagi 5 detik");
        delay(5000);
    }
}

// Fungsi ambil waktu sekarang
void getWaktuSaatIni(char* wadahWaktu, size_t ukuranMaksimal) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    snprintf(wadahWaktu, ukuranMaksimal, "Waktu_Belum_Sinkron");
    return;
  }
  // %Y = Tahun(4 digit), %m = Bulan, %d = Tanggal, %H = Jam, %M = Menit, %S = Detik
  strftime(wadahWaktu, ukuranMaksimal, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

// Fungsi untuk mengirim data BMS ke MQTT
void kirimBmsStatus(const char* topic_prefix) {
  char pesan[10];
  char timestamp[25];
  char topic[50];
  BmsSysData bms = jkBms.getData();
  pylonEncoder.updateData(bms);

  getWaktuSaatIni(timestamp, sizeof(timestamp));
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "timestamp");
  client.publish(topic, timestamp);
  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_voltage_v);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_total");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_current_a);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "arus");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%.1f", bms.soc_percent);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "soc");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%u", bms.avg_cycle_count);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "cycle");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.battery_type);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "bat_type");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_balance);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_bal");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_charge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_charge");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_discharge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_discharge");
  client.publish(topic, pesan);
  snprintf(pesan, sizeof(pesan), "%u", bms.alarm_message);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "alarm");
  client.publish(topic, pesan);
  for (int i = 0; i < 24; i++) {
      char pesanVcell[10];
      snprintf(topic, sizeof(topic), "%sv_sel_%d", topic_prefix, i + 1);
      snprintf(pesanVcell, sizeof(pesanVcell), "%.3f", bms.cell_voltages[i]);
      client.publish(topic, pesanVcell);
      delay(5); 
  }
  Serial.println("Publish Berhasil");
}

void resetWifi(){
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Tombol Reset ditekan. Tahan 3 detik...");
    delay(3000);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Menghapus data WiFi dari memori!");
      // Buka memori dengan mode read/write (false)
      pref.begin("wifi_config", false);
      pref.clear(); // Hapus SEMUA data di dalam "wifi_config"
      pref.end();
    
      Serial.println("Data terhapus. Silakan restart perangkat.");
      ESP.restart(); 
    }
  }
}

void setup_wifi_bt(){
    pref.begin("wifi_config", true);
    String savedSSID = pref.getString("ssid", "");
    String savedPASS = pref.getString("pass", "");
    pref.end();
    if (savedSSID != "") {
        WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
    } else {
        if (!SerialBT.isReady()){
            SerialBT.begin("Com260001");}
        if (SerialBT.hasClient()) {
            setup_wifi();} 
        else {Serial.println("Menunggu koneksi Bluetooth untuk konfigurasi WiFi...");}
    }
    configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

// void printBmsStatus() {
//     BmsSysData bms = jkBms.getData();
//     pylonEncoder.updateData(bms);
//     Serial.println("\n Status BMS:");
//     Serial.print("Tegangan Total: "); Serial.print(bms.total_voltage_v, 2); Serial.println(" V");
//     Serial.print("Arus: "); Serial.print(bms.total_current_a, 2); Serial.println(" A");
//     Serial.print("SOC: "); Serial.print(bms.soc_percent); Serial.println(" %");
//     Serial.print("Jumlah Siklus: "); Serial.println(bms.avg_cycle_count);
//     Serial.println("Tegangan per Sel:");
//     Serial.print("switch_balance: "); Serial.println(bms.switch_balance);
//     Serial.print("switch_charge: "); Serial.println(bms.switch_charge);
//     Serial.print("switch_discharge: "); Serial.println(bms.switch_discharge);
//     Serial.print("battery_type: "); Serial.println(bms.battery_type);
//     for (int i = 0; i < bms.num_cells; i++) {
//         Serial.print("  Sel-"); Serial.print(i + 1); Serial.print(": ");
//         Serial.print(bms.cell_voltages[i], 3); Serial.println(" V");
//     }
// }

// --- PROGRAM UTAMA ---
void setup() {
    Serial.begin(115200);
    // BMS Serial Init
    jkBms.begin();
    // Inverter Serial Init
    Serial2.begin(9600, SERIAL_8N1, INV_RX_PIN, INV_TX_PIN);
    // Button Reset
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    Serial.println("Communicator Dimulai.");
}
unsigned long lastSetupTime = 0;
unsigned long lastMQTTrec = 0;
void loop() {
    unsigned long now = millis();
    if (!WiFi.isConnected() && (now - lastSetupTime > 10000)) { // Cek setiap 10 detik
        lastSetupTime = now;
        setup_wifi_bt();}
    resetWifi();

    // Reconnect MQTT
    if (!client.connected() && (now - lastMQTTrec > 10000)) {
        lastMQTTrec = now;
        if(WiFi.isConnected()){reconnect();} else{Serial.println("WiFi belum terhubung, MQTT tidak bisa diinisialisasi.");}}
    else {
        client.loop();
        // Kirim data ke MQTT
        if (now - lastMsg > 20000) {lastMsg = now;  kirimBmsStatus("site1/b260001/");}}
    
    jkBms.loop();

    while (Serial2.available()) {
        invBuffer[invIdx++] = Serial2.read();
        invLastTime = now;
        if (invIdx >= 512) invIdx = 0;}

    if (invIdx > 0 && (now - invLastTime > 50)) {processInverterRequest(); invIdx = 0;}

}

