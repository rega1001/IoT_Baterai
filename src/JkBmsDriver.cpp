#include "JkBmsDriver.h"

// Frame Request standar JK BMS (Read All Data)
static const byte JK_REQ_FRAME[] = { 
  0x4E, 0x57, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 
  0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x68, 0x00, 0x00, 0x01, 0x29 
};

// --- KONFIGURASI BATERAI ---
// Ubah ini sesuai spek baterai LFP kamu
const float BAT_CAPACITY_AH = 100.0;    
const float MAX_DISCHARGE_C = 0.6;     // 0.6C Discharge
const float MAX_CHARGE_C    = 0.5;     // 0.5C Charge

JkBmsDriver::JkBmsDriver(HardwareSerial& serialPort, int rxPin, int txPin) 
  : _serial(serialPort), _rxPin(rxPin), _txPin(txPin) {
    _bufferIndex = 0;
    _lastByteTime = 0;
    _lastRequestTime = 0;
    
    // Init data default agar tidak kosong saat start
    memset(&_bmsData, 0, sizeof(_bmsData));
    _bmsData.num_cells = 16; 
    _bmsData.charge_enable = true;
    _bmsData.discharge_enable = true;
    _bmsData.avg_soh_percent = 100;
}

void JkBmsDriver::begin() {
    _serial.begin(115200, SERIAL_8N1, _rxPin, _txPin);
}

BmsSysData JkBmsDriver::getData() {
    return _bmsData;
}

void JkBmsDriver::loop() {
    // 1. Kirim Request ke JK BMS setiap detik
    if (millis() - _lastRequestTime > _requestInterval) {
        if(!_serial){
            begin(); // Coba mulai ulang serial jika belum siap
        }
        sendRequest();
        _lastRequestTime = millis();
    }

    // 2. Baca Serial masuk
    while (_serial.available() > 0 && _bufferIndex < JKBMS_BUFFER_SIZE) {
        _rxBuffer[_bufferIndex++] = _serial.read();
        _lastByteTime = millis();
    }

    // 3. Proses Paket jika sudah lengkap (timeout detected)
    if (_bufferIndex > 0 && (millis() - _lastByteTime > _packetTimeout)) {
        processBuffer();
        _bufferIndex = 0; // Reset buffer
    }
}

void JkBmsDriver::sendRequest() {
    _serial.write(JK_REQ_FRAME, sizeof(JK_REQ_FRAME));
}

uint16_t JkBmsDriver::bytesToUint16(byte msb, byte lsb) {
    return (uint16_t)((msb << 8) | lsb);
}

void JkBmsDriver::processBuffer() {
    // Validasi Header JK (0x4E 0x57)
    if (_bufferIndex > 11 && _rxBuffer[0] == 0x4E && _rxBuffer[1] == 0x57) {
        // Offset Header = 11 byte, Footer = 4 byte
        int payloadLen = _bufferIndex - 11 - 4; 
        if (payloadLen > 0) {
            decodeFrame(&_rxBuffer[11], payloadLen);
        }
    }
}

// Helper Lokal: Hitung Smart Discharge Limit (Kurva)
float calcDischargeLimit(int soc) {
    float max_amps = BAT_CAPACITY_AH * MAX_DISCHARGE_C;
    
    // Zona Aman (20% - 100%): Full Power
    if (soc >= 20) {
        return max_amps;
    }
    // Zona Derating (5% - 20%): Turun Linear dari 100% ke 10%
    else if (soc >= 5) {
        float slope = (float)(soc - 5) / (20.0 - 5.0); // 0.0 s.d 1.0
        float min_limit = max_amps * 0.10; // Min 10%
        return min_limit + (slope * (max_amps - min_limit));
    }
    // Zona Kritis (< 5%): Limp Mode (5%)
    else if (soc > 0) {
        return max_amps * 0.05; 
    }
    // Mati
    return 0.0;
}

// Helper Lokal: Hitung Smart Charge Limit (Tapering)
float calcChargeLimit(int soc) {
    float max_amps = BAT_CAPACITY_AH * MAX_CHARGE_C;
    
    // Tapering saat hampir penuh (>98%)
    if (soc >= 98) return 5.0; // Trickle/Balancing current
    if (soc >= 90) return max_amps * 0.5; // Kurangi setengah jelang penuh
    
    return max_amps;
}

void JkBmsDriver::decodeFrame(byte* payload, int length) {
    int i = 0;
    while (i < length) {
        byte id = payload[i++];
        
        switch (id) {
            case 0x79: { // Cell Voltages
                int len = payload[i++];
                int total_cells_jk = len / 3;
                int cellsToRead = (total_cells_jk > 16) ? 16 : total_cells_jk;
                
                _bmsData.num_cells = cellsToRead;
                float maxV = 0.0;
                float minV = 10.0;
                
                for(int j=0; j<cellsToRead; j++) {
                    uint16_t mv = bytesToUint16(payload[i+1], payload[i+2]);
                    float v = mv / 1000.0;
                    _bmsData.cell_voltages[j] = v;
                    if(v > maxV) { maxV = v; _bmsData.max_cell_volt_loc = j+1; }
                    if(v < minV) { minV = v; _bmsData.min_cell_volt_loc = j+1; }
                    i += 3;
                }
                if (total_cells_jk > 16) i += (total_cells_jk - 16) * 3;
                _bmsData.max_cell_voltage_v = maxV;
                _bmsData.min_cell_voltage_v = minV;
                break;
            }
            case 0x80: { // Power Tube Temp
                _bmsData.max_cell_temp_c = (int16_t)bytesToUint16(payload[i], payload[i+1]);
                i += 2; break;}
            case 0x81: { // Battery Box Temp
                _bmsData.min_cell_temp_c = (int16_t)bytesToUint16(payload[i], payload[i+1]);
                i += 2; break;}
            case 0x82: {// Battery Temp
                _bmsData.avg_cell_temp_c = (int16_t)bytesToUint16(payload[i], payload[i+1]);
                i += 2; break;}
            case 0x83: {// Total Voltage
                _bmsData.total_voltage_v = bytesToUint16(payload[i], payload[i+1]) / 100.0;
                i += 2; break;}
            case 0x84: {// Current
                int16_t rawI = (int16_t)bytesToUint16(payload[i], payload[i+1]);
                rawI = rawI < 0 ? rawI + 32768 : -rawI;
                _bmsData.total_current_a = rawI / 100.0; 
                i += 2; break;}
            case 0x85: {// SOC
                _bmsData.soc_percent = payload[i];
                i += 1; break;}
            case 0x87: {// Cycles
                _bmsData.avg_cycle_count = bytesToUint16(payload[i], payload[i+1]);
                _bmsData.max_cycle_count = _bmsData.avg_cycle_count;
                i += 2; break;}
            case 0x9D: {// Balance Switch (RW)
                _bmsData.switch_balance = payload[i];
                i += 1; break;}
            case 0xAB: {// Charge Switch (RW)
                _bmsData.switch_charge = payload[i];
                i += 1; break;}
            case 0xAC: {// Discharge Switch (RW)
                _bmsData.switch_discharge = payload[i];
                i += 1; break;}
            case 0xAF: {// Battery Type (RW)
                _bmsData.battery_type = payload[i];
                i += 1; break;}
            case 0x8B: {// Pesan Alarm
                _bmsData.alarm_message = bytesToUint16(payload[i], payload[i+1]);
                i += 2; break;}

            // Skip unknown
            case 0x86: case 0x9E: case 0x9F: case 0xA0: case 0xA1: case 0xC0:
            case 0xA2: case 0xA9: case 0xAE: case 0xB3: case 0xB8:
                i += 1; break;
            case 0x8C: case 0x8E: case 0x8F: case 0x90: case 0x91: case 0xB1:
            case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
            case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0xA3:
            case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xA8: case 0xAD:
            case 0xB0: 
                i += 2; break;
            case 0x89: case 0x8A: case 0xAA: case 0xB5: case 0xB6: case 0xB9:
                i += 4; break;
            case 0xB4: i += 8; break;
            case 0xB2: i += 10; break;
            case 0xB7: i += 16; break;
            case 0xBA: i += 24; break;
            default: i++; break;
        }
    }
    
    // --- UPDATE MANAGEMENT LIMITS (Smart Algorithm) ---
    
    // 1. Voltage Limits
    _bmsData.charge_voltage_limit_v = _bmsData.num_cells * 3.65; 
    _bmsData.discharge_voltage_limit_v = _bmsData.num_cells * 2.8;
    
    // 2. Current Limits (Pakai Fungsi Cerdas)
    _bmsData.discharge_current_limit_a = calcDischargeLimit(_bmsData.soc_percent);
    _bmsData.charge_current_limit_a    = calcChargeLimit(_bmsData.soc_percent);

    // 3. Flags
    // Matikan charge/discharge jika limit sudah 0 (untuk keamanan tambahan)
    _bmsData.charge_enable    = (_bmsData.charge_current_limit_a > 1.0);
    _bmsData.discharge_enable = (_bmsData.discharge_current_limit_a > 1.0);
}