#include <Arduino.h>
#include "TgproInverter.h"

// Inisialisasi Objek: (Serial2, RX_PIN 33, TX_PIN 32, MAX485_DE 4)
const int RX_PIN = 33;
const int TX_PIN = 32;
const int DE_RE_PIN = 4;
TgproInverter inverter(Serial2, RX_PIN, TX_PIN, DE_RE_PIN);
TgproData dataInv;

void setup() {
  Serial.begin(115200);
  
  // Memanggil semua fungsi setup yang tadinya panjang di dalam class
  inverter.begin();
  
  Serial.println("Sistem TGPRO / PowMr Siap!");
}

void loop() {
  static unsigned long lastRead = 0;
  
  // Baca setiap 10 detik sekali
  if (millis() - lastRead > 10000) {
    lastRead = millis();
    
    // Perintahkan class untuk membaca Modbus
    inverter.bacaInverter();
    // Ambil data terbaru dari class
    dataInv = inverter.getData();

    // Jika Anda butuh mengambil datanya untuk dikirim ke MQTT:
    // TgproData dataTerkini = inverter.getData();
    // client.publish("kantor/b012026/v_batt", String(dataTerkini.V_Batt).c_str());
  }
}