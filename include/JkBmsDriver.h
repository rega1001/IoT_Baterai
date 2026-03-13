#ifndef JKBMS_DRIVER_H
#define JKBMS_DRIVER_H

#include <Arduino.h>
#include "PylontechEncoder.h" // Kita butuh struktur BmsSysData dari library yang kamu upload

#define JKBMS_BUFFER_SIZE 512

class JkBmsDriver {
  public:
    // Constructor: Serial Port & Pins
    JkBmsDriver(HardwareSerial& serialPort, int rxPin, int txPin);

    void begin();
    void loop(); // Panggil ini terus menerus di main loop
    
    // Getter: Mengambil data matang untuk disuapkan ke PylontechEncoder
    BmsSysData getData();

  private:
    HardwareSerial& _serial;
    int _rxPin, _txPin;
    
    // Buffer & Timing
    byte _rxBuffer[JKBMS_BUFFER_SIZE];
    int _bufferIndex;
    unsigned long _lastByteTime;
    
    unsigned long _lastRequestTime;
    const long _requestInterval = 1000; // Minta data tiap 1 detik
    const int _packetTimeout = 50;      // 50ms idle = paket selesai

    // Data Internal (Format Pylontech)
    BmsSysData _bmsData; 
    
    // Internal Methods
    void sendRequest();
    void processBuffer();
    void decodeFrame(byte* payload, int length);
    uint16_t bytesToUint16(byte msb, byte lsb);
};

#endif