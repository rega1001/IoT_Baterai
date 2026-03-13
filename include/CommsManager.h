#ifndef COMMS_MANAGER_H
#define COMMS_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <BluetoothSerial.h>
#include <Preferences.h>

class CommsManager {
  public:
    // Constructor untuk memasukkan konfigurasi
    CommsManager(const char* mqtt_server, int mqtt_port, const char* mqtt_user, const char* mqtt_pass, const char* clientId, int buttonPin, const char* btName);

    void begin();
    void loop();
    
    // Fungsi untuk meminjamkan objek PubSubClient ke main.cpp agar bisa melakukan publish/subscribe
    PubSubClient& getMqttClient();

    void getSiteName(char* buffer, size_t maxLen);

  private:
    const char* _btName;
    const char* _mqttServer;
    int _mqttPort;
    const char* _mqttUser;
    const char* _mqttPass;
    const char* _clientId;
    int _buttonPin;

    WiFiClientSecure _espClient;
    PubSubClient _client;
    BluetoothSerial _serialBT;
    Preferences _pref;

    char _inputSSID[50];
    char _inputPASS[50];
    char _siteName[20];
    bool _isWaitingForSSID;
    bool _isWaitingForPass;
    int _pos;

    unsigned long _lastSetupTime;
    unsigned long _lastMQTTrec;

    // Metode Internal
    void connectToWiFi();
    void setupWifiBluetooth(const char* bt_name);
    void processBluetoothInput();
    void resetWifiMemory();
    void reconnectMqtt();
};

#endif