#include <Arduino.h>
#include <WiFi.h>
#include <BluetoothSerial.h>

// Inisialisasi Object Bluetooth
BluetoothSerial SerialBT;

// Variabel penampung
String inputSSID = "";
String inputPASS = "";
bool isWaitingForSSID = true; // Flag untuk tahu sedang nunggu apa

void setup() {
  Serial.begin(115200);
  
  // Nama Bluetooth yang akan muncul di HP
  SerialBT.begin("ESP32_Setup_WiFi"); 
  
  Serial.println("\nBluetooth siap! Silakan pairing dan connect.");
}

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

void loop() {
  // Cek apakah ada data masuk dari Bluetooth
  if (SerialBT.available()) {
    // Baca data sampai baris baru (Enter)
    String dataMasuk = SerialBT.readStringUntil('\n');
    
    // PENTING: Hapus spasi/enter di awal & akhir string
    dataMasuk.trim(); 

    // Jangan proses jika kosong
    if (dataMasuk.length() > 0) {
      
      if (isWaitingForSSID) {
        // --- TAHAP 1: TERIMA SSID ---
        inputSSID = dataMasuk;
        isWaitingForSSID = false; // Ganti status jadi nunggu password
        
        SerialBT.print("SSID diterima: ");
        SerialBT.println(inputSSID);
        SerialBT.println("Sekarang ketik PASSWORD:");
        
      } else {
        // --- TAHAP 2: TERIMA PASSWORD ---
        inputPASS = dataMasuk;
        
        SerialBT.println("Password diterima. Mencoba konek...");
        
        // Eksekusi koneksi
        connectToWiFi();
      }
    }
  }
  
  // Jika belum ada koneksi, beri prompt sesekali (opsional)
  // static unsigned long lastTime = 0;
  // if (millis() - lastTime > 5000 && inputSSID == "") {
  //    lastTime = millis();
  //    if(SerialBT.hasClient()) SerialBT.println("Halo! Ketik nama WiFi (SSID) Anda:");
  // }
}