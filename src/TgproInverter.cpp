#include "TgproInverter.h"

// Inisialisasi variabel static
int TgproInverter::_deRePin = -1;

// ==========================================
// CONSTRUCTOR
// ==========================================
TgproInverter::TgproInverter(HardwareSerial& serialPort, int rxPin, int txPin, int deRePin)
  : _serial(serialPort), _rxPin(rxPin), _txPin(txPin) {
    _deRePin = deRePin;
    memset(&_data, 0, sizeof(_data)); // Bersihkan data di awal
}

// ==========================================
// FUNGSI CALLBACK RS485
// ==========================================
void TgproInverter::preTransmission() {
  if (_deRePin >= 0) digitalWrite(_deRePin, 1);
}

void TgproInverter::postTransmission() {
  if (_deRePin >= 0) digitalWrite(_deRePin, 0);
}

// ==========================================
// BEGIN (SETUP AWAL)
// ==========================================
void TgproInverter::begin() {
  if (_deRePin >= 0) {
    pinMode(_deRePin, OUTPUT);
    digitalWrite(_deRePin, 0);
  }

  _serial.begin(9600, SERIAL_8N1, _rxPin, _txPin);
  _node.begin(1, _serial);

  if (_deRePin >= 0) {
    _node.preTransmission(preTransmission);
    _node.postTransmission(postTransmission);
  }
}

// ==========================================
// AMBIL DATA (GETTER)
// ==========================================
const TgproData& TgproInverter::getData() const {
  return _data;
}

// ==========================================
// BACA INVERTER UTAMA
// ==========================================
void TgproInverter::bacaInverter() {
    uint8_t result;

    // BLOK 1: Baca Grid
    result = _node.readHoldingRegisters(202, 3); 
    if (result == _node.ku8MBSuccess) {
        _data.V_Grid = _node.getResponseBuffer(0) / 10.0; 
        _data.Hz_Grid = _node.getResponseBuffer(1) / 100.0; 
        _data.W_Grid = (int16_t)_node.getResponseBuffer(2); 

        Serial.println("Berhasil baca Blok Grid");
    } else { Serial.println("Gagal baca Blok Grid"); return; }  
    delay(10); 

    // BLOK 2: Baca Blok Inverter
    result = _node.readHoldingRegisters(205, 5);
    if (result == _node.ku8MBSuccess) {
        _data.V_Inverter  = _node.getResponseBuffer(0) / 10.0; 
        _data.I_Inverter  = _node.getResponseBuffer(1) / 10.0;
        _data.Hz_Inverter = _node.getResponseBuffer(3) / 100.0;
        _data.W_Inverter  = (int16_t)_node.getResponseBuffer(4);
        _data.W_Ch_Inv    = _node.getResponseBuffer(5); 

        Serial.println("Berhasil baca Blok Inverter");
    } else { Serial.println("Gagal baca Blok Inverter"); return; } delay(10);

    // BLOK 3: Baca Blok Output
    result = _node.readHoldingRegisters(210, 5);
    if (result == _node.ku8MBSuccess) {
        _data.V_Out  = _node.getResponseBuffer(0) / 10.0; 
        _data.I_Out  = _node.getResponseBuffer(1) / 10.0; 
        _data.Hz_Out = _node.getResponseBuffer(2) / 100.0;
        _data.W_Out  = _node.getResponseBuffer(3);
        _data.VA_Out = _node.getResponseBuffer(4);

        Serial.println("Berhasil baca Blok Output");
    } else { Serial.println("Gagal baca Blok Output"); return; } delay(10);

    // BLOK 4: Baca Blok Baterai
    result = _node.readHoldingRegisters(215, 3);
    if (result == _node.ku8MBSuccess) {
        _data.V_Batt = _node.getResponseBuffer(0) / 10.0; 
        _data.I_Batt = (int16_t)_node.getResponseBuffer(1) / 10.0; // Menggunakan int16_t agar bisa minus
        _data.W_Batt = (int16_t)_node.getResponseBuffer(2); 

        Serial.println("Berhasil baca Blok Baterai");
    } else { Serial.println("Gagal baca Blok Baterai"); return; } delay(10);
    
    result = _node.readHoldingRegisters(229, 1);
    if (result == _node.ku8MBSuccess) {
        _data.SOC_Batt = _node.getResponseBuffer(0); 
    } else { Serial.println("Gagal baca Blok SOC Baterai"); return; } delay(10);

    // BLOK 5: Baca Blok PV
    result = _node.readHoldingRegisters(219, 4);
    if (result == _node.ku8MBSuccess) {
        _data.V_PV    = _node.getResponseBuffer(0) / 10.0;
        _data.I_PV    = _node.getResponseBuffer(1) / 10.0;
        _data.W_PV    = _node.getResponseBuffer(2);
        _data.W_Ch_PV = _node.getResponseBuffer(3); 

        Serial.println("Berhasil baca Blok PV");
    } else { Serial.println("Gagal baca Blok PV"); return; } delay(10);

    // BLOK 6: Baca Blok Load and Temperature
    result = _node.readHoldingRegisters(225, 3);
    if (result == _node.ku8MBSuccess) {
        _data.Percent_Load = _node.getResponseBuffer(0); 
        _data.T_DCDC       = (int8_t)_node.getResponseBuffer(1);
        _data.T_Inverter   = (int8_t)_node.getResponseBuffer(2); 

        Serial.println("Berhasil baca Blok Load and Temperature");
    } else { Serial.println("Gagal baca Blok Load and Temperature"); return; } delay(10);

    // BLOK 7: Baca Blok Arus Charging / Discharging
    result = _node.readHoldingRegisters(232, 3);
    if (result == _node.ku8MBSuccess) {
        _data.I_PC_ND_Batt = (int16_t)_node.getResponseBuffer(0) / 10.0; 
        _data.I_Ch_Inv     = _node.getResponseBuffer(1) / 10.0; // Diperbaiki: dibagi 10
        _data.I_Ch_PV      = _node.getResponseBuffer(2) / 10.0; // Diperbaiki: dibagi 10

        Serial.println("Berhasil baca Blok Arus Charging / Discharging");
    } else { Serial.println("Gagal baca Blok Arus Charging / Discharging"); return; } delay(10);

    // BLOK 8: Baca Blok Setting 300 - 309
    result = _node.readHoldingRegisters(300, 10);
    if (result == _node.ku8MBSuccess) {
        _data.Out_Mode       = _node.getResponseBuffer(0); 
        _data.Out_Priority   = _node.getResponseBuffer(1); 
        _data.V_In_Range     = _node.getResponseBuffer(2); 
        _data.Buzz_Mode      = _node.getResponseBuffer(3); 
        _data.LCD_Light      = _node.getResponseBuffer(5); 
        _data.LCD_RTH        = _node.getResponseBuffer(6); 
        _data.Energy_Save    = _node.getResponseBuffer(7); 
        _data.Overload_Reset = _node.getResponseBuffer(8); 
        _data.Over_T_Reset   = _node.getResponseBuffer(9); 

        Serial.println("Berhasil baca Blok Setting 300 - 309");
    } else { Serial.println("Gagal baca Blok Setting 300 - 309"); return; } delay(10);

    // BLOK 9: Baca Blok Setting 310 - 313
    result = _node.readHoldingRegisters(310, 4);
    if (result == _node.ku8MBSuccess) {
        _data.Overload_Bypass = _node.getResponseBuffer(0); 
        _data.Batt_Eq         = _node.getResponseBuffer(3); 

        Serial.println("Berhasil baca Blok Setting 310 - 313");
    } else { Serial.println("Gagal baca Blok Setting 310 - 313"); return; } delay(10);

    // BLOK 10: Baca Blok Setting 320 - 329
    result = _node.readHoldingRegisters(320, 10);
    if (result == _node.ku8MBSuccess) {
        _data.Set_V_Out        = _node.getResponseBuffer(0) / 10.0;  // Diperbaiki: / 10.0
        _data.Set_Hz_Out       = _node.getResponseBuffer(1) / 100.0; // Diperbaiki: / 100.0
        _data.OV_Batt_Pro      = _node.getResponseBuffer(3) / 10.0;  // Diperbaiki: / 10.0
        _data.Max_V_Ch         = _node.getResponseBuffer(4) / 10.0;  // Diperbaiki: / 10.0
        _data.Float_V_Ch       = _node.getResponseBuffer(5) / 10.0;  // Diperbaiki: / 10.0
        _data.V_Batt_Start_Dch = _node.getResponseBuffer(6) / 10.0;  // Diperbaiki: / 10.0
        _data.V_Batt_Low_Grid  = _node.getResponseBuffer(7) / 10.0;  // Diperbaiki: / 10.0
        _data.V_Batt_Low_OfG   = _node.getResponseBuffer(9) / 10.0;  // Diperbaiki: / 10.0

        Serial.println("Berhasil baca Blok Setting 320 - 329");
    } else { Serial.println("Gagal baca Blok Setting 320 - 329"); return; } delay(10);
    
    // BLOK 11: Baca Blok Setting 331 - 337
    result = _node.readHoldingRegisters(331, 7);
    if (result == _node.ku8MBSuccess) {
        _data.Ch_Batt_Priority = _node.getResponseBuffer(0);
        _data.Max_I_Ch       = _node.getResponseBuffer(1) / 10.0; // Diperbaiki: / 10.0
        _data.Max_I_Ch_Grid  = _node.getResponseBuffer(2) / 10.0; // Diperbaiki: / 10.0
        _data.V_Ch_Eq        = _node.getResponseBuffer(3) / 10.0; // Diperbaiki: / 10.0
        _data.Time_Eq        = _node.getResponseBuffer(4); 
        _data.Timeout_Eq     = _node.getResponseBuffer(5); 
        _data.Ch_Eq_Interval = _node.getResponseBuffer(6); 

        Serial.println("Berhasil baca Blok Setting 331 - 337");
    } else { Serial.println("Gagal baca Blok Setting 331 - 337"); return; } delay(10);

    // BLOK 12: Baca Blok Setting 406 dan 420
    result = _node.readHoldingRegisters(406,1);
    if (result == _node.ku8MBSuccess) {
        _data.Turn_On_Mode = _node.getResponseBuffer(0); 
    } else { Serial.println("Gagal baca Blok Setting 406"); return; } delay(10);

    result = _node.readHoldingRegisters(420,1);
    if (result == _node.ku8MBSuccess) {
        _data.Remote_Sw = _node.getResponseBuffer(0); 
    } else { Serial.println("Gagal baca Blok Setting 420"); return; } delay(10);
}