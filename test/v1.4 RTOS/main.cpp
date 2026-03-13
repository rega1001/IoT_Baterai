#include <Arduino.h>
#include <time.h>
#include "JkBmsDriver.h"
#include "PylontechEncoder.h"
#include "KirimDataMQTT.h"
#include "CommsManager.h"
#include "TgproInverter.h"

// --- KONFIGURASI PIN ---
#define JK_RX_PIN 26
#define JK_TX_PIN 27

#define INV_RX_PIN 16
#define INV_TX_PIN 17

#define COMM_RX_PIN 33
#define COMM_TX_PIN 32

const int   BUTTON_PIN  = 0;

// --- OBJEK ---
// Driver untuk baca JK BMS
JkBmsDriver jkBms(Serial1, JK_RX_PIN, JK_TX_PIN);
// Encoder untuk Inverter (Versi Pylontech 2.0, Addr 12)
PylontechEncoder pylonEncoder(0x20, 0x12, 0x46);
// Driver untuk TGPRO Inverter
TgproInverter tgproInverter(Serial1, COMM_RX_PIN, COMM_TX_PIN);
// Handle FreeRTOS Tasks
TaskHandle_t TaskJaringan;
TaskHandle_t TaskSensor;

// Variabel Simpanan
byte invBuffer[512];
int invIdx = 0;
unsigned long invLastTime = 0;
unsigned long lastMsg = 0;
unsigned long lastTGpro = 0;
char site_name[20];
char topic_pref_bms[50];
char topic_pref_inv[50];

// --- KONFIGURASI PENGGUNA ---
const char* mqtt_server = "37d12fc61b8641adb496ecc91a0d2539.s1.eu.hivemq.cloud"; 
const int   mqtt_port   = 8883;
const char* mqtt_user   = "iot_batari";
const char* mqtt_pass   = "@IoTbatari123";
// const char* clientId    = "BySense-T001";
const char* device_name = "BySense-T001"; 
const char* inv_name    = "Inv-0001";

CommsManager comms(mqtt_server, mqtt_port, mqtt_user, mqtt_pass, device_name, BUTTON_PIN, device_name);

// Fungsi penerima pesan MQTT (Callback)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Pesan masuk [%s]: ", topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

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

// =======================================================
// TASK 1: MENGURUS JARINGAN DAN KOMUNIKASI (Core 0)
// =======================================================
void loopJaringan(void * parameter) {
    // Di FreeRTOS, Task tidak boleh berhenti. Harus pakai infinite loop (for(;;))
    for(;;) {
        comms.loop(); // Keep-alive MQTT jalan terus di sini tanpa hambatan!

        if (millis() - lastMsg > 10000) {
            lastMsg = millis();
            if (site_name[0] == '\0') { 
                comms.getSiteName(site_name, sizeof(site_name));
                snprintf(topic_pref_bms, sizeof(topic_pref_bms), "%s/%s/", site_name, device_name);
                snprintf(topic_pref_inv, sizeof(topic_pref_inv), "%s/%s/", site_name, inv_name);

                Serial.printf("Site: %s", site_name); Serial.println();
            }
            if (comms.getMqttClient().connected()) {
                kirimBmsStatus(topic_pref_bms); 
                kirimDataInverter(topic_pref_inv);
            }
        }

        // Kirim data ke Inverter setiap ada request masuk
        while (Serial2.available()) {
            invBuffer[invIdx++] = Serial2.read();
            invLastTime = millis();
            if (invIdx >= 512) invIdx = 0;
        }

        if (invIdx > 0 && (millis() - invLastTime > 50)) {
            processInverterRequest(); 
            invIdx = 0;
        }

        // PENTING: Wajib beri waktu istirahat 10 milidetik agar CPU tidak kepanasan
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

// =======================================================
// TASK 2: MENGURUS AMBIL DATA (Core 1)
// =======================================================
void loopSensor(void * parameter) {
    for(;;) {
        jkBms.loop();

        // Baca Inverter TGPRO
        if (millis() - lastTGpro > 30000) {
            lastTGpro = millis();
            
            Serial1.end(); 
            delay(10);
            
            Serial1.begin(9600, SERIAL_8N1, INV_RX_PIN, INV_TX_PIN); 
            tgproInverter.begin(); 
            tgproInverter.bacaInverter();
            
            Serial1.end(); 
            delay(10);
            
            Serial1.begin(115200, SERIAL_8N1, JK_RX_PIN, JK_TX_PIN);
            jkBms.begin(); 
        }

        

        // Istirahatkan Core 1
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

// =======================================================
// SETUP UTAMA
// =======================================================
void setup() {
    Serial.begin(115200);
    // BMS Serial Init
    jkBms.begin();
    // Inverter Serial Init
    Serial2.begin(9600, SERIAL_8N1, INV_RX_PIN, INV_TX_PIN);
    // Button Reset
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // Mulai jaringan (WiFi/BT/MQTT)
    comms.begin();
    // Daftarkan fungsi callback ke MQTT
    comms.getMqttClient().setCallback(mqttCallback);
    // Buat Task Jaringan di Core 0
    xTaskCreatePinnedToCore(
        loopJaringan,   // Nama fungsi yang akan dijalankan
        "TaskJaringan", // Nama task (untuk debugging)
        10000,          // Ukuran memori/Stack (10000 byte aman untuk WiFi/MQTT)
        NULL,           // Parameter yang dikirim (Kosong)
        1,              // Prioritas (1 = Standar)
        &TaskJaringan,  // Handle Task
        0               // Dijalankan di CORE 0
    );

    // Buat Task Sensor di Core 1
    xTaskCreatePinnedToCore(
        loopSensor,     
        "TaskSensor",   
        10000,          // Ukuran memori (10000 byte aman untuk array dan Modbus)
        NULL,           
        1,              
        &TaskSensor,    
        1               // Dijalankan di CORE 1
    );
    Serial.println("Communicator Dimulai.");
}

// =======================================================
// LOOP UTAMA - TIDAK BOLEH ADA LOGIKA BERAT DI SINI!
// =======================================================
void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

