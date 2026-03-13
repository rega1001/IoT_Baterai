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

#include <HardwareSerial.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <time.h>

// --- Pengaturan ---
#define RX_PIN 26 // RXD2e
#define TX_PIN 27 // TXD2
#define BUFFER_SIZE 512

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

// --- Buffer untuk menampung data serial ---
uint8_t receive_buffer[BUFFER_SIZE];
int buffer_index = 0;
unsigned long last_uint8_t_received_time = 0;
const int packet_timeout = 50; // Anggap paket selesai jika ada jeda 50ms

// --- Perintah untuk Meminta Semua Data ---
uint8_t request_frame[] = { 0x4E, 0x57, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x01, 0x29 };
unsigned long last_request_time = 0;
unsigned long lastMsg = 0;
unsigned long lastprint = 0;

// --- Struktur Data untuk Menampung Status BMS ---
struct BmsStatus {
  float cell_voltages[24];
  float total_voltage;
  float current;
  uint8_t soc_percent;
  uint8_t num_cells;         
  uint16_t cycle_count;
  uint8_t battery_type;
  bool switch_balance;
  bool switch_charge;
  bool switch_discharge;
  uint16_t alarm_message;
};
BmsStatus bms;

// --- FUNGSI-FUNGSI PENDUKUNG ---
// Fungsi untuk menggabungkan dua uint8_t menjadi integer (16-bit, Big Endian)
uint16_t uint8_tsToUint16(uint8_t msb, uint8_t lsb) {
  return (uint16_t)((msb << 8) | lsb);
}

// Fungsi untuk mengurai data BMS dari buffer
void decodeBmsData(uint8_t* payload, int length) {
  int i = 0;
  while (i < length) {
    uint8_t register_id = payload[i];
    i++;

    // Switch-case untuk mengurai setiap register berdasarkan ID-nya
    switch (register_id) {
      // --- Register yang Kita Proses & Tampilkan ---
      case 0x79: { // Tegangan setiap sel
        int data_len = payload[i];
        i++;
        bms.num_cells = data_len / 3;
        for (int j = 0; j < bms.num_cells; j++) {
          uint16_t cell_voltage_mv = uint8_tsToUint16(payload[i+1], payload[i+2]);
          bms.cell_voltages[j] = cell_voltage_mv / 1000.0;
          i += 3;
        }
        break;
      }
      case 0x83: { // Tegangan Total
        uint16_t total_voltage_mv = uint8_tsToUint16(payload[i], payload[i+1]);
        bms.total_voltage = total_voltage_mv / 100.0;
        i += 2;
        break;
      }
      case 0x84: { // Arus
        uint16_t raw_cur = uint8_tsToUint16(payload[i], payload[i+1]);
        if (raw_cur & 0x8000) {
            bms.current = (raw_cur & 0x7FFF) / 100.0; // Charging
        } else {
            bms.current = (raw_cur & 0x7FFF) / -100.0; // Discharging
        }
        i += 2;
        break;
      }
      case 0x85: { // State of Charge (SOC)
        bms.soc_percent = payload[i];
        i += 1;
        break;
      }
      case 0x87: { // Jumlah Siklus
        bms.cycle_count = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      }
      case 0x9D: { // Balance Switch (RW)
        bms.switch_balance = payload[i];
        i += 1;
        break;
      }
      case 0xAB: { // Charge Switch (RW)
        bms.switch_charge = payload[i];
        i += 1;
        break;
      }
      case 0xAC: { // Discharge Switch (RW)
        bms.switch_discharge = payload[i];
        i += 1;
        break;
      }
      case 0xAF: { // Battery Type (RW)
        bms.battery_type = payload[i];
        i += 1;
        break;
      }
      case 0x8B:{ // Pesan Alarm
        bms.alarm_message = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      }
      case 0x80: { // Suhu Power Tube
        // int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        // bms.power_tube_temp_c = temp;
        i += 2;
        break;
      }
      case 0x81: { // Suhu Balancing Board
        // int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        // bms.balancing_board_temp_c = temp;
        i += 2;
        break;
      }
      case 0x82: { // Suhu Baterai (dari sensor eksternal)
        // int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        // bms.cell_temp_c = temp;
        i += 2;
        break;
      }
      case 0x8C:{ // Status Baterai
        // bms.battery_status = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x90:{ /*Cell OVP (V)*/
        // bms.cell_ovp = uint8_tsToUint16(payload[i], payload[i+1])/1000.0;
        i += 2;
        break;
      } 
      case 0x91:{ /*Cell OVPR (V)*/
        // bms.cell_ovpr = uint8_tsToUint16(payload[i], payload[i+1])/1000.0;
        i += 2;
        break;
      } 
      case 0x93:{ /*Cell UVP (V)*/
        // bms.supv = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x94:{ /*Cell UVPR (V)*/
        // bms.murv = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x97:{ /*Continued Discharge Curr. (A)*/
        // bms.do_value = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x98:{ /*Discharge OCP Delay (s)*/
        // bms.do_delay = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x99:{ /*Continued Charge Curr. (A)*/
        // bms.co_value = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x9A:{ /*Charge OCP Delay (s)*/
        // bms.co_delay = uint8_tsToUint16(payload[i], payload[i+1]);
        i += 2;
        break;
      } 
      case 0x9B:{ /*Start Balance Volt (V)*/
      //   bms.balance_sv = uint8_tsToUint16(payload[i], payload[i+1])/1000.0;
        i += 2;
        break;
      } 
      case 0x9C:{ /*Balance Trig. Volt (V)*/
      //   bms.balance_opd = uint8_tsToUint16(payload[i], payload[i+1])/1000.0;
        i += 2;
        break;
      } 
      
      // --- Register yang Kita Lewati (SKIP) ---
      // Data dengan panjang 1 uint8_t
      case 0x86: /*Jumlah Sensor Suhu*/ 
      case 0xA9: /*Battery string setting*/
      case 0xAE: /*Protection board address*/
      case 0xB3: /*Dedicated charger switch*/
      case 0xB8: /*Whether to start current calibration*/
      case 0xC0: /*Protocol version number*/
      i += 1;
      break;
      
      // Data dengan panjang 2 uint8_t
      case 0x9E: /*Power tube temperature protection value*/ 
      case 0x9F: /*Power tube temperature recovery value*/
      case 0xA0: /*Temperature protection value in the battery box*/
      case 0xA1: /*Temperature recovery value in the battery box*/
      case 0xA2: /*Battery temperature difference protection value*/
      case 0xA3: /*Charge UTPR (C)*/
      case 0xA4: /*Discharge OTP (C)*/
        i += 2;
      break;

      // Data dengan panjang 4 uint8_t
      case 0x89: /*Total battery cycle capacity*/
      case 0x8A: /*Total number of battery strings*/
      case 0xAA: /*Battery capacity setting*/
      case 0xB5: /*Date of manufacture*/
      case 0xB6: /*System working hours*/
      case 0xB9: /*Actual battery capacity*/
        i += 4;
        break;

      // Data dengan panjang 8 uint8_t
      case 0xB4: /*Device ID code*/
        i += 8;
        break;

      // Data dengan panjang 10 uint8_t
      case 0xB2: /*Modify parameter password*/
        i += 10;
        break;

      // Data dengan panjang 16 uint8_t
      case 0xB7: /*Software version number*/
        i += 16;
        break;
      
      // Data dengan panjang 24 uint8_t
      case 0xBA: /*Manufacturer ID naming*/
        i += 24;
        break;

      default:
        i++; 
        break;
    }
  }
}

void printBmsStatus() {
  Serial.println("\n Status BMS:");
  Serial.print("Tegangan Total: "); Serial.print(bms.total_voltage, 2); Serial.println(" V");
  Serial.print("Arus: "); Serial.print(bms.current, 2); Serial.println(" A");
  Serial.print("SOC: "); Serial.print(bms.soc_percent); Serial.println(" %");
  Serial.print("Jumlah Siklus: "); Serial.println(bms.cycle_count);
  Serial.println("Tegangan per Sel:");
  Serial.print("switch_balance: "); Serial.println(bms.switch_balance);
  Serial.print("switch_charge: "); Serial.println(bms.switch_charge);
  Serial.print("switch_discharge: "); Serial.println(bms.switch_discharge);
  Serial.print("battery_type: "); Serial.println(bms.battery_type);
  Serial.print("alarm: "); Serial.println(bms.alarm_message);
  for (int i = 0; i < bms.num_cells; i++) {
    Serial.print("  Sel-"); Serial.print(i + 1); Serial.print(": ");
    Serial.print(bms.cell_voltages[i], 3); Serial.println(" V");
  }
}

void requestBmsData() {
  Serial.println("\n\n--- Mengirim permintaan data ke BMS ---");
  Serial1.write(request_frame, sizeof(request_frame));
}

void processDataBuffer(uint8_t* buffer, int length) {
  Serial.print("\nPaket data lengkap diterima (" );
  Serial.print(length);
  Serial.println(" uint8_ts). Memulai proses...");

  // Cek apakah data dimulai dengan header yang benar
  if (length > 2 && buffer[0] == 0x4E && buffer[1] == 0x57) {
    Serial.println("  - Header ditemukan.");
    
    // Definisikan panjang header dan footer yang tetap
    const int header_len = 11;
    const int footer_len = 4;
    const int min_packet_len = header_len + footer_len;

    if (length >= min_packet_len) {
      // Hitung panjang payload yang sebenarnya
      int payload_length = length - header_len - footer_len;
      Serial.print("  - Panjang payload dihitung: ");
      Serial.println(payload_length);
      
      Serial.println("  - Mendekode...");
      // Panggil fungsi decode dengan pointer ke awal payload
      decodeBmsData(&buffer[header_len], payload_length);
      printBmsStatus();

    } else {
      Serial.println("  - ERROR: Data yang diterima lebih pendek dari panjang minimum paket.");
    }
  } else {
    Serial.println("  - ERROR: Header (0x4E 0x57) tidak ditemukan di awal buffer.");
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
  while (!client.connected()) {
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
void kirimBmsStatus() {
  char pesan[10];
  char timestamp[25];
  getWaktuSaatIni(timestamp, sizeof(timestamp));
  client.publish("kantor/b012026/timestamp", timestamp);
  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_voltage);
  client.publish("kantor/b012026/v_total", pesan);
  snprintf(pesan, sizeof(pesan), "%.2f", bms.current);
  client.publish("kantor/b012026/arus", pesan);
  snprintf(pesan, sizeof(pesan), "%.1f", bms.soc_percent);
  client.publish("kantor/b012026/soc", pesan);
  snprintf(pesan, sizeof(pesan), "%u", bms.cycle_count);
  client.publish("kantor/b012026/cycle", pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.battery_type);
  client.publish("kantor/b012026/bat_type", pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_balance);
  client.publish("kantor/b012026/sw_bal", pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_charge);
  client.publish("kantor/b012026/sw_charge", pesan);
  snprintf(pesan, sizeof(pesan), "%d", bms.switch_discharge);
  client.publish("kantor/b012026/sw_discharge", pesan);
  snprintf(pesan, sizeof(pesan), "%u", bms.alarm_message);
  client.publish("kantor/b012026/alarm", pesan);
  for (int i = 0; i < 24; i++) {
      char topic[50];
      char pesanVcell[10];
      snprintf(topic, sizeof(topic), "kantor/b012026/v_sel_%d", i + 1);
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

// --- PROGRAM UTAMA ---
void setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    pref.begin("wifi_config", true);
    String savedSSID = pref.getString("ssid", "");
    String savedPASS = pref.getString("pass", "");
    pref.end();
    if (savedSSID != "") {
        WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
    } else {
        SerialBT.begin("Comunicator BMS");
        setup_wifi();
    }
    Serial.println("JK BMS Decoder (Buffer Mode) Dimulai.");
    espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  unsigned long now = millis();
  resetWifi();

  // Jalankan MQTT
  if (!client.connected()) {reconnect();}
  client.loop();

  // Ambi data dari BMS
  // Req data
  if (now - last_request_time > 10000) {last_request_time = now;  requestBmsData();}
  // Ambil respons
  while (Serial1.available() > 0 && buffer_index < BUFFER_SIZE) {last_uint8_t_received_time = now;  receive_buffer[buffer_index++] = Serial1.read();}
  // Proses data
  if (buffer_index > 0 && now - last_uint8_t_received_time > packet_timeout) {processDataBuffer(receive_buffer, buffer_index);  buffer_index = 0;}

  // Kirim data ke MQTT
  if (now - lastMsg > 20000) {lastMsg = now;  kirimBmsStatus();}

  // Print data
  if (now - lastprint > 15000) {lastprint = now;  printBmsStatus();}
}

