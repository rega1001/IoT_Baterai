/*
  PylontechEncoder.h
  Library untuk "cosplay" sebagai BMS Pylontech. (FIXED V3.3 - Added CMD 63 Management)
*/

#ifndef PYLONTECH_ENCODER_H
#define PYLONTECH_ENCODER_H

#include <Arduino.h>

// Ukuran buffer
#define PYLON_FRAME_BUFFER_SIZE 150

struct BmsSysData {
  // --- Data Sistem (CMD 61) ---
  float total_voltage_v;      // No 1
  float total_current_a;      // No 2
  int   soc_percent;          // No 3
  int   avg_cycle_count;      // No 4
  int   max_cycle_count;      // No 5
  int   avg_soh_percent;      // No 6
  int   min_soh_percent;      // No 7
  float max_cell_voltage_v; // No 8
  int   max_cell_volt_loc;    // No 9
  float min_cell_voltage_v; // No 10
  int   min_cell_volt_loc;    // No 11
  float avg_cell_temp_c;    // No 12
  float max_cell_temp_c;    // No 13
  int   max_cell_temp_loc;    // No 14
  float min_cell_temp_c;    // No 15
  int   min_cell_temp_loc;    // No 16
  
  // --- Data Manajemen (CMD 63) [BARU] ---
  float charge_voltage_limit_v;    // Batas Tegangan Charge (misal 53.5V)
  float discharge_voltage_limit_v; // Batas Tegangan Discharge (misal 45.0V)
  float charge_current_limit_a;    // Batas Arus Charge (misal 25A)
  float discharge_current_limit_a; // Batas Arus Discharge (misal 50A)
  
  // Status Flags (CMD 63 Byte 5)
  bool charge_enable;      // True = Boleh Charge
  bool discharge_enable;   // True = Boleh Discharge
  bool charge_immediately; // True = Minta Charge Paksa (Force Charge)
  bool full_charge_req;    // True = Minta Full Charge
  
  // Data Sel Individual (Internal)
  int   num_cells;
  float cell_voltages[24]; 
  int   num_temps;
  float temperatures_c[5]; 

  // Data Monitor and Control (iot)
  uint8_t battery_type;
  bool switch_balance;
  bool switch_charge;
  bool switch_discharge;
  uint16_t alarm_message;
};


class PylontechEncoder {
  public:
    PylontechEncoder(byte ver, byte adr, byte cid1);

    void updateData(const BmsSysData& newData);

    // Handshake (0x60)
    const uint8_t* buildBasicInfoResponse();

    // Data Rutin (0x61)
    const uint8_t* buildAnalogResponse();

    // Data Alarm (0x62)
    const uint8_t* buildAlarmInfoResponse();

    // [BARU] Data Manajemen (0x63) - Limit Volt/Ampere
    const uint8_t* buildManagementResponse();

    int getFrameLength();

    // void 

  private:
    byte _VER;
    byte _ADR;
    byte _CID1;

    uint8_t _frameBuffer[PYLON_FRAME_BUFFER_SIZE];
    int _frameLength;
    
    BmsSysData _bmsData; 

    void appendHexAscii(byte value, int &index);
    void appendHexAscii(uint16_t value, int &index);
    void appendHexAscii(int16_t value, int &index);
    void appendLengthField(int info_ascii_len, int &index);
    uint16_t calculatePylontechChecksum(byte* data, int len);
};

#endif