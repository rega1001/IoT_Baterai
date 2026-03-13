/*
  ESP32 JK BMS -> Inverter Bridge (Pylontech Protocol V3.5)
  Hardware: ESP32
  - Serial1 (Pin 26/27): Ke JK BMS (TTL 3.3V)
  - Serial2 (Pin 16/17): Ke Inverter (Pakai modul RS485/CAN)
*/

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

void setup() {
  Serial.begin(115200);
  
  // Start JK BMS
  jkBms.begin();
  
  // Start Inverter Comm (RS485 biasanya 9600)
  Serial2.begin(9600, SERIAL_8N1, INV_RX_PIN, INV_TX_PIN);
  
  Serial.println("SYSTEM STARTED: JK BMS to Pylontech Bridge");
}

void loop() {
  // 1. UPDATE DATA JK BMS (Non-blocking)
  // Fungsi ini otomatis request data tiap detik & parsing
  jkBms.loop();

  // 2. LISTEN REQUEST DARI INVERTER
  while (Serial2.available()) {
    invBuffer[invIdx++] = Serial2.read();
    invLastTime = millis();
    if (invIdx >= 512) invIdx = 0; // Safety
  }

  // 3. PROSES REQUEST (Jika paket lengkap / timeout 50ms)
  if (invIdx > 0 && (millis() - invLastTime > 50)) {
    processInverterRequest();
    invIdx = 0; // Reset buffer
  }
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