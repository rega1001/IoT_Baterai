#include "CommsManager.h"

// ==========================================
// CONSTRUCTOR
// ==========================================
CommsManager::CommsManager(const char* mqtt_server, int mqtt_port, const char* mqtt_user, const char* mqtt_pass, const char* clientId, int buttonPin, const char* btName)
  : _mqttServer(mqtt_server), _mqttPort(mqtt_port), _mqttUser(mqtt_user), _mqttPass(mqtt_pass), _clientId(clientId), _buttonPin(buttonPin), _btName(btName)
{
  _isWaitingForSSID = true;
  _isWaitingForPass = true;
  _pos = 0;
  _lastSetupTime = 0;
  _lastMQTTrec = 0;
}

// ==========================================
// BEGIN (Dijalankan di Setup)
// ==========================================
void CommsManager::begin() {
  pinMode(_buttonPin, INPUT_PULLUP);
  
  _espClient.setInsecure(); // Abaikan verifikasi SSL
  _client.setClient(_espClient);
  _client.setServer(_mqttServer, _mqttPort);

  Serial.println("Network & Comms Manager Dimulai.");
}

// ==========================================
// PENGAMBIL OBJECT MQTT
// ==========================================
PubSubClient& CommsManager::getMqttClient() {
  return _client;
}

// ==========================================
// JANTUNG PROGRAM (Dijalankan di Loop)
// ==========================================
void CommsManager::loop() {
  unsigned long now = millis();

  // 1. Cek Tombol Reset
  resetWifiMemory();

  // 2. Cek Koneksi WiFi
  if (!WiFi.isConnected() && (now - _lastSetupTime > 10000)) {
    _lastSetupTime = now;
    setupWifiBluetooth(_btName);
  }

  // 3. Cek Koneksi MQTT (Non-Blocking)
  if (!WiFi.isConnected()) {
    // Jika WiFi mati, jangan paksa MQTT konek
    return;
  }

  if (!_client.connected()) {
    if (now - _lastMQTTrec > 10000) {
      _lastMQTTrec = now;
      reconnectMqtt();
    }
  } else {
    _client.loop(); // Izinkan MQTT bekerja di background
  }
}

// ==========================================
// PROSES BLUETOOTH KE WIFI
// ==========================================
void CommsManager::setupWifiBluetooth(const char* bt_name) {
  _pref.begin("config", true);
  String savedSSID = _pref.getString("ssid", "");
  String savedPASS = _pref.getString("pass", "");
  _pref.end();

  if (savedSSID != "") {
    WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
  } else {
    if (!_serialBT.isReady()) {
      _serialBT.begin(bt_name);
    }
    else if (_serialBT.hasClient()) {
      processBluetoothInput();
    } else {
      Serial.println("Menunggu koneksi Bluetooth untuk konfigurasi WiFi...");
    }
  }
  
  // Update NTP Time
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
}

void CommsManager::processBluetoothInput() {
  bool inputComplete = false;
  _serialBT.println("Silakan ketik SSID WiFi:");
  
  while (!inputComplete) {
    if (_serialBT.available()) {
      char c = _serialBT.read();
      
      if (c == '\n') {
        if (_isWaitingForSSID) { // input SSID
          _inputSSID[_pos] = '\0';
          _serialBT.print("SSID: ");
          _serialBT.println(_inputSSID);
          _serialBT.println("Sekarang Ketik PASSWORD:");
          _pos = 0;
          _isWaitingForSSID = false; 
        } else if (_isWaitingForPass) { // input Password
          _inputPASS[_pos] = '\0';
          _serialBT.print("Password: ");
          _serialBT.println(_inputPASS);
          connectToWiFi();
          _serialBT.println("Sekarang Ketik Nama Site:");
          _pos = 0;
          _isWaitingForPass = false; 
        } else { // Input nama site
          _siteName[_pos] = '\0';
          _serialBT.print("Nama Site: ");
          _serialBT.println(_siteName);
          _pos = 0;
          _isWaitingForSSID = true; 
          _isWaitingForPass = true;
          inputComplete = true;
          _pref.begin("config", false);
          _pref.putString("site_name", _siteName);
          _pref.end();
        }
      } 
      else if (c == '\r') { /* Do nothing */ }
      else {
        if (_pos < 49) {
          if (_isWaitingForSSID) _inputSSID[_pos] = c;
          else _inputPASS[_pos] = c;
          _pos++;
        }
      }
    }
  } ESP.restart();
}

void CommsManager::connectToWiFi() {
  delay(10);
  _serialBT.print("Menghubungkan ke ");
  _serialBT.println(_inputSSID);
  WiFi.begin(_inputSSID, _inputPASS);
  
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _serialBT.print(".");
    counter++;
    if (counter > 20) {
      _serialBT.println("\n[GAGAL] Tidak bisa konek. Cek password/SSID.");
      _inputSSID[0] = '\0';
      _inputPASS[0] = '\0';
      _pos = 0;
      _isWaitingForSSID = true;
      return;
    }
  }
  
  _serialBT.println("\nWiFi Terkoneksi!");
  _pref.begin("config", false); 
  _pref.putString("ssid", _inputSSID);
  _pref.putString("pass", _inputPASS);
  _pref.end();
  
  delay(1000);
}

// ==========================================
// INTERNAL MQTT & RESET
// ==========================================
void CommsManager::reconnectMqtt() {
  Serial.print("Menghubungkan ke HiveMQ Cloud...");
  // Di sini tidak ada 'while' lagi, sehingga tidak bikin ESP32 macet!
  if (_client.connect(_clientId, _mqttUser, _mqttPass)) {
    Serial.println("Berhasil!");
    // _client.subscribe("topik/perintah/#"); // Uncomment jika butuh
  } else {
    Serial.printf("Gagal, rc=%d. Coba lagi 10 detik...\n", _client.state());
  }
}

void CommsManager::resetWifiMemory() {
  if (digitalRead(_buttonPin) == LOW) {
    Serial.println("Tombol Reset ditekan. Tahan 3 detik...");
    delay(3000);
    if (digitalRead(_buttonPin) == LOW) {
      Serial.println("Menghapus data WiFi dari memori!");
      _pref.begin("config", false);
      _pref.clear(); 
      _pref.end();
      Serial.println("Data terhapus. Silakan restart perangkat.");
      ESP.restart(); 
    }
  }
}

// ==========================================
// Ambil data yang di simpan di Preferences
// ==========================================
String CommsManager::getSiteName() {
  _pref.begin("config", true);
  String site = _pref.getString("site_name", "");
  _pref.end();
  return site;
}