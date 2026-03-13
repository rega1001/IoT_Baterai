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
#include <WiFiClientSecure.h> // PENTING: Library untuk koneksi aman (SSL)
#include <PubSubClient.h>
#include <BluetoothSerial.h>

// --- Pengaturan ---
#define RX_PIN 26 // RXD2e
#define TX_PIN 27 // TXD2
#define BUFFER_SIZE 512

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

// Inisialisasi Object Bluetooth
BluetoothSerial SerialBT;

// Variabel penampung
String inputSSID = "";
String inputPASS = "";
bool isWaitingForSSID = true; // Flag untuk tahu sedang nunggu apa

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
const long request_interval = 5000; // Minta data setiap 5 detik

// --- Struktur Data untuk Menampung Status BMS ---
struct BmsStatus {
  float total_voltage;
  float current;
  int soc_percent;
  float cell_voltages[24];
  int num_cells;
  float power_tube_temp_c;
  float balancing_board_temp_c; // Ditambahkan
  float cell_temp_c;            // Ditambahkan
  int cycle_count;
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
      case 0x80: { // Suhu Power Tube
        int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        bms.power_tube_temp_c = temp;
        i += 2;
        break;
      }
      case 0x81: { // Suhu Balancing Board
        int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        bms.balancing_board_temp_c = temp;
        i += 2;
        break;
      }
      case 0x82: { // Suhu Baterai (dari sensor eksternal)
        int16_t temp = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        bms.cell_temp_c = temp;
        i += 2;
        break;
      }
      case 0x83: { // Tegangan Total
        uint16_t total_voltage_mv = uint8_tsToUint16(payload[i], payload[i+1]);
        bms.total_voltage = total_voltage_mv / 100.0;
        i += 2;
        break;
      }
      case 0x84: { // Arus
        int16_t raw_current = (int16_t)uint8_tsToUint16(payload[i], payload[i+1]);
        bms.current = raw_current / 100.0;
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

      // --- Register yang Kita Lewati (SKIP) ---
      // Data dengan panjang 1 uint8_t
      case 0x86:/**/ case 0x9D: case 0x9E: case 0x9F: case 0xA0: case 0xA1:
      case 0xA2: case 0xA9: case 0xAB: case 0xAC: case 0xAE: case 0xAF:
      case 0xB3: case 0xB8: case 0xC0:
        i += 1;
        break;
      
      // Data dengan panjang 2 uint8_t
      case 0x8B: case 0x8C: case 0x8E: case 0x8F: case 0x90: case 0x91:
      case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
      case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0xA3:
      case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xA8: case 0xAD:
      case 0xB0: case 0xB1:
        i += 2;
        break;

      // Data dengan panjang 4 uint8_t
      case 0x89: case 0x8A: case 0xAA: case 0xB5: case 0xB6: case 0xB9:
        i += 4;
        break;

      // Data dengan panjang 8 uint8_t
      case 0xB4:
        i += 8;
        break;

      // Data dengan panjang 10 uint8_t
      case 0xB2:
        i += 10;
        break;

      // Data dengan panjang 16 uint8_t
      case 0xB7:
        i += 16;
        break;
      
      // Data dengan panjang 24 uint8_t
      case 0xBA:
        i += 24;
        break;

      default:
        Serial.print("PERINGATAN: Register ID tidak dikenal 0x");
        Serial.print(register_id, HEX);
        Serial.println(". Menghentikan parse paket ini.");
        return; // Hentikan parse jika menemukan ID yang benar-benar tidak dikenal
    }
  }
}

void printBmsStatus() {
  Serial.println("\n Status BMS:");
  Serial.print("Tegangan Total: "); Serial.print(bms.total_voltage, 2); Serial.println(" V");
  Serial.print("Arus: "); Serial.print(bms.current, 2); Serial.println(" A");
  Serial.print("SOC: "); Serial.print(bms.soc_percent); Serial.println(" %");
  Serial.print("Suhu Power Tube: "); Serial.print(bms.power_tube_temp_c, 1); Serial.println(" C");
  Serial.print("Suhu Balancing Board: "); Serial.print(bms.balancing_board_temp_c, 1); Serial.println(" C");
  Serial.print("Suhu Baterai: "); Serial.print(bms.cell_temp_c, 1); Serial.println(" C");
  Serial.print("Jumlah Siklus: "); Serial.println(bms.cycle_count);
  Serial.println("Tegangan per Sel:");
  for (int i = 0; i < bms.num_cells; i++) {
    Serial.print("  Sel-"); Serial.print(i + 1); Serial.print(": ");
    Serial.print(bms.cell_voltages[i], 3); Serial.println(" V");
  }
}

void requestBmsData() {
  // Serial.println("\n\n--- Mengirim permintaan data ke BMS ---");
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
void setup_wifi() {
  delay(10);
  SerialBT.println("\nSedang mencoba konek ke WiFi...");
  SerialBT.print("SSID: "); SerialBT.println(inputSSID);
  SerialBT.print("Pass: "); SerialBT.println(inputPASS);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(inputSSID.c_str(), inputPASS.c_str());

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    
    // Timeout setelah 10 detik (20 x 500ms)
    if (counter > 20) {
      SerialBT.println("\n[GAGAL] Tidak bisa konek. Cek password/SSID.");
      SerialBT.println("Silakan ketik ulang SSID:");
      
      // Reset proses
      inputSSID = "";
      inputPASS = "";
      isWaitingForSSID = true;
      return;
    }
  }

  // Jika berhasil
  SerialBT.println("\n[BERHASIL] WiFi Terkoneksi!");
  SerialBT.print("IP Address: ");
  SerialBT.println(WiFi.localIP());
  
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
    String clientId = "BMSClient-000001";
    
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

// Fungsi untuk mengirim data BMS ke MQTT
void kirimBmsStatus() {
    String pesanVtotal = String(bms.total_voltage, 2);
    String pesanI = String(bms.current, 2);
    String pesanSOC = String(bms.soc_percent);
    String pesanTpowTube = String(bms.power_tube_temp_c, 1);
    String pesanTbalBoard = String(bms.balancing_board_temp_c, 1);
    String pesanTbat = String(bms.cell_temp_c, 1);
    String pesanCycle = String(bms.cycle_count);

    client.publish("Kantor/B012026/V_Total", pesanVtotal.c_str());
    client.publish("Kantor/B012026/Arus", pesanI.c_str());
    client.publish("Kantor/B012026/SOC", pesanSOC.c_str());
    client.publish("Kantor/B012026/Suhu_Power_Tube", pesanTpowTube.c_str());
    client.publish("Kantor/B012026/Suhu_Balancing_Board", pesanTbalBoard.c_str());
    client.publish("Kantor/B012026/Suhu_Baterai", pesanTbat.c_str());
    client.publish("Kantor/B012026/Jumlah_Siklus", pesanCycle.c_str());
    for (int i = 0; i < 24; i++) {
        String pesanVcell = String(bms.cell_voltages[i], 3);
        String topic = "Kantor/B012026/Tegangan_Sel_" + String(i + 1);
        client.publish(topic.c_str(), pesanVcell.c_str());
    }

    Serial.println("Publish Berhasil");
}

//
void connectToWiFi() {
  SerialBT.println("\nSedang mencoba konek ke WiFi...");
  SerialBT.print("SSID: "); SerialBT.println(inputSSID);
  SerialBT.print("Pass: "); SerialBT.println(inputPASS);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(inputSSID.c_str(), inputPASS.c_str());

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    
    // Timeout setelah 10 detik (20 x 500ms)
    if (counter > 20) {
      SerialBT.println("\n[GAGAL] Tidak bisa konek. Cek password/SSID.");
      SerialBT.println("Silakan ketik ulang SSID:");
      
      // Reset proses
      inputSSID = "";
      inputPASS = "";
      isWaitingForSSID = true;
      return;
    }
  }

  // Jika berhasil
  SerialBT.println("\n[BERHASIL] WiFi Terkoneksi!");
  SerialBT.print("IP Address: ");
  SerialBT.println(WiFi.localIP());
}


// --- PROGRAM UTAMA ---
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  SerialBT.begin("ESP32_Setup_WiFi");
  Serial.println("JK BMS Decoder (Buffer Mode) Dimulai.");
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  static unsigned long lastMsg = 0;
  unsigned long now = millis();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  
  if (now - last_request_time > request_interval) {
    last_request_time = now;
    requestBmsData();
  }
  
  while (Serial1.available() > 0 && buffer_index < BUFFER_SIZE) {
    receive_buffer[buffer_index++] = Serial1.read();
    last_uint8_t_received_time = now;
  }

  if (buffer_index > 0 && now - last_uint8_t_received_time > packet_timeout) {
    processDataBuffer(receive_buffer, buffer_index);
    buffer_index = 0; 
  }

  // Kirim data ke MQTT
  if (now - lastMsg > 10000) {
    lastMsg = now;
    kirimBmsStatus();
  }
}

