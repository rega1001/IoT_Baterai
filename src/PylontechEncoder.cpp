/*
  PylontechEncoder.cpp
  Implementasi (logika) untuk Pylontech Encoder Library.
  
  UPDATE V3.4: Menambahkan buildManagementResponse() untuk CMD 0x63.
*/

#include "PylontechEncoder.h"

static char nibbleToHexChar(byte nibble) {
  nibble &= 0x0F; 
  return (nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A');
}

PylontechEncoder::PylontechEncoder(byte ver, byte adr, byte cid1) {
  _VER = ver;
  _ADR = adr;
  _CID1 = cid1;
  _frameLength = 0;
  memset(&_bmsData, 0, sizeof(_bmsData));
  memset(_frameBuffer, 0, PYLON_FRAME_BUFFER_SIZE);
}

void PylontechEncoder::updateData(const BmsSysData& newData) {
  memcpy(&_bmsData, &newData, sizeof(BmsSysData));
}

int PylontechEncoder::getFrameLength() {
  return _frameLength;
}

// 0x60: Basic Info Response
const uint8_t* PylontechEncoder::buildBasicInfoResponse() {
  byte info_payload_biner[60]; 
  int idx = 0;

  const char* devName = "SUPERBWALL";
  memcpy(&info_payload_biner[idx], devName, 10); idx += 10;

  const char* manuf = "BATARI"; 
  memcpy(&info_payload_biner[idx], manuf, 7); idx += 7;

  info_payload_biner[idx++] = 0x00; 
  info_payload_biner[idx++] = 0x02;

  info_payload_biner[idx++] = 0x01; 

  const char* barcode = "BATARI09122025";
  memcpy(&info_payload_biner[idx], barcode, 15); idx += 15;

  int info_binary_len = idx; 
  int info_ascii_len = info_binary_len * 2; 

  int frame_idx = 0;
  _frameBuffer[frame_idx++] = 0x7E; 

  appendHexAscii(_VER, frame_idx);
  appendHexAscii(_ADR, frame_idx);
  appendHexAscii(_CID1, frame_idx);
  appendHexAscii((byte)0x00, frame_idx); 

  appendLengthField(info_ascii_len, frame_idx);

  int checksum_start_index = 1;

  for(int i=0; i < info_binary_len; i++) {
    appendHexAscii(info_payload_biner[i], frame_idx);
  }

  int checksum_len = frame_idx - checksum_start_index;
  uint16_t checksum = calculatePylontechChecksum(&_frameBuffer[checksum_start_index], checksum_len);
  
  appendHexAscii(checksum, frame_idx);
  _frameBuffer[frame_idx++] = 0x0D; 
  
  _frameLength = frame_idx;
  return _frameBuffer;
}

// 0x61: Analog Data Response
const uint8_t* PylontechEncoder::buildAnalogResponse() {
  byte info_payload_biner[PYLON_FRAME_BUFFER_SIZE / 2]; 
  int info_idx_biner = 0;

  uint16_t total_volt_scaled = (uint16_t)(_bmsData.total_voltage_v * 1000); 
  info_payload_biner[info_idx_biner++] = (total_volt_scaled >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = total_volt_scaled & 0xFF;

  int16_t total_current_scaled = (int16_t)(_bmsData.total_current_a * 100.0); 
  info_payload_biner[info_idx_biner++] = (total_current_scaled >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = total_current_scaled & 0xFF;
  
  info_payload_biner[info_idx_biner++] = (byte)_bmsData.soc_percent;

  info_payload_biner[info_idx_biner++] = (_bmsData.avg_cycle_count >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.avg_cycle_count & 0xFF;
  
  info_payload_biner[info_idx_biner++] = (_bmsData.max_cycle_count >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.max_cycle_count & 0xFF;
  
  info_payload_biner[info_idx_biner++] = (byte)_bmsData.avg_soh_percent;
  info_payload_biner[info_idx_biner++] = (byte)_bmsData.min_soh_percent;

  uint16_t max_cell_v_mv = (uint16_t)(_bmsData.max_cell_voltage_v * 1000.0);
  info_payload_biner[info_idx_biner++] = (max_cell_v_mv >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = max_cell_v_mv & 0xFF;
  
  info_payload_biner[info_idx_biner++] = (_bmsData.max_cell_volt_loc >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.max_cell_volt_loc & 0xFF;
  
  uint16_t min_cell_v_mv = (uint16_t)(_bmsData.min_cell_voltage_v * 1000.0);
  info_payload_biner[info_idx_biner++] = (min_cell_v_mv >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = min_cell_v_mv & 0xFF;

  info_payload_biner[info_idx_biner++] = (_bmsData.min_cell_volt_loc >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.min_cell_volt_loc & 0xFF;

  int16_t avg_temp_dk = (int16_t)((_bmsData.avg_cell_temp_c + 273.15) * 10.0);
  info_payload_biner[info_idx_biner++] = (avg_temp_dk >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = avg_temp_dk & 0xFF;

  int16_t max_temp_dk = (int16_t)((_bmsData.max_cell_temp_c + 273.15) * 10.0);
  info_payload_biner[info_idx_biner++] = (max_temp_dk >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = max_temp_dk & 0xFF;

  info_payload_biner[info_idx_biner++] = (_bmsData.max_cell_temp_loc >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.max_cell_temp_loc & 0xFF;

  int16_t min_temp_dk = (int16_t)((_bmsData.min_cell_temp_c + 273.15) * 10.0);
  info_payload_biner[info_idx_biner++] = (min_temp_dk >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = min_temp_dk & 0xFF;
  
  info_payload_biner[info_idx_biner++] = (_bmsData.min_cell_temp_loc >> 8) & 0xFF;
  info_payload_biner[info_idx_biner++] = _bmsData.min_cell_temp_loc & 0xFF;

  for(int k=0; k<10; k++) { 
      int16_t dummy_temp = 2981; 
      info_payload_biner[info_idx_biner++] = (dummy_temp >> 8) & 0xFF;
      info_payload_biner[info_idx_biner++] = dummy_temp & 0xFF;
  }

  int info_binary_len = info_idx_biner; 
  int info_ascii_len = info_binary_len * 2; 

  int frame_idx = 0;
  _frameBuffer[frame_idx++] = 0x7E; 

  appendHexAscii(_VER, frame_idx);
  appendHexAscii(_ADR, frame_idx);
  appendHexAscii(_CID1, frame_idx);
  appendHexAscii((byte)0x00, frame_idx); 

  appendLengthField(info_ascii_len, frame_idx);

  int checksum_start_index = 1; 

  for(int i=0; i < info_binary_len; i++) {
    appendHexAscii(info_payload_biner[i], frame_idx);
  }

  int checksum_len = frame_idx - checksum_start_index;
  uint16_t checksum = calculatePylontechChecksum(&_frameBuffer[checksum_start_index], checksum_len);
  
  appendHexAscii(checksum, frame_idx);
  _frameBuffer[frame_idx++] = 0x0D; 
  
  _frameLength = frame_idx;
  return _frameBuffer;
}

// 0x62: Alarm Info Response
const uint8_t* PylontechEncoder::buildAlarmInfoResponse() {
  byte info_payload_biner[10];
  int idx = 0;
  
  for(int i=0; i<4; i++) {
    info_payload_biner[idx++] = 0x00; // Semua Normal
  }
  
  int info_binary_len = idx; 
  int info_ascii_len = info_binary_len * 2;

  int frame_idx = 0;
  _frameBuffer[frame_idx++] = 0x7E; 

  appendHexAscii(_VER, frame_idx);
  appendHexAscii(_ADR, frame_idx);
  appendHexAscii(_CID1, frame_idx);
  appendHexAscii((byte)0x00, frame_idx); 

  appendLengthField(info_ascii_len, frame_idx);

  int checksum_start_index = 1;

  for(int i=0; i < info_binary_len; i++) {
    appendHexAscii(info_payload_biner[i], frame_idx);
  }

  int checksum_len = frame_idx - checksum_start_index;
  uint16_t checksum = calculatePylontechChecksum(&_frameBuffer[checksum_start_index], checksum_len);
  
  appendHexAscii(checksum, frame_idx);
  _frameBuffer[frame_idx++] = 0x0D; 
  
  _frameLength = frame_idx;
  return _frameBuffer;
}

// Referensi: Sunsynk PDF Hal 22-23
const uint8_t* PylontechEncoder::buildManagementResponse() {
  byte info_payload_biner[20];
  int idx = 0;

  // 1. Charge Voltage Limit (2 Byte, mV)
  // Contoh PDF: 56.531V -> DCD3 Hex (56531) -> Skala 1000
  uint16_t chg_volt_mv = (uint16_t)(_bmsData.charge_voltage_limit_v * 1000.0);
  info_payload_biner[idx++] = (chg_volt_mv >> 8) & 0xFF;
  info_payload_biner[idx++] = chg_volt_mv & 0xFF;

  // 2. Discharge Voltage Limit (2 Byte, mV)
  // Contoh PDF: 24.00V -> 5DC0 Hex (24000) -> Skala 1000
  uint16_t dis_volt_mv = (uint16_t)(_bmsData.discharge_voltage_limit_v * 1000.0);
  info_payload_biner[idx++] = (dis_volt_mv >> 8) & 0xFF;
  info_payload_biner[idx++] = dis_volt_mv & 0xFF;

  // 3. Charge Current Limit (2 Byte, Skala 100 -> 0.01A)
  // Contoh PDF: 25.0A -> 09C4 Hex (2500) -> 2500 * 0.01 = 25.0
  uint16_t chg_curr_scaled = (uint16_t)(_bmsData.charge_current_limit_a * 10.0);
  info_payload_biner[idx++] = (chg_curr_scaled >> 8) & 0xFF;
  info_payload_biner[idx++] = chg_curr_scaled & 0xFF;

  // 4. Discharge Current Limit (2 Byte, Skala 100 -> 0.01A)
  // Contoh PDF: 20.2A -> 07E4 Hex (2020) -> 2020 * 0.01 = 20.2
  uint16_t dis_curr_scaled = (uint16_t)(_bmsData.discharge_current_limit_a * 10.0);
  info_payload_biner[idx++] = (dis_curr_scaled >> 8) & 0xFF;
  info_payload_biner[idx++] = dis_curr_scaled & 0xFF;

  // 5. Charge/Discharge Status (1 Byte)
  // Bit 7: Charge Enable
  // Bit 6: Discharge Enable
  // Bit 5: Charge Immediately (Force Charge)
  // Bit 4: Full Charge Req
  byte status_byte = 0;

  
  if (_bmsData.charge_enable)      status_byte |= (1 << 7);
  if (_bmsData.discharge_enable)   status_byte |= (1 << 6);
  if (_bmsData.charge_immediately) status_byte |= (1 << 5);
  if (_bmsData.full_charge_req)    status_byte |= (1 << 4);
  
  info_payload_biner[idx++] = status_byte;

  // --- Bangun Frame ASCII ---
  int info_binary_len = idx; // 9 byte total
  int info_ascii_len = info_binary_len * 2; 

  int frame_idx = 0;
  _frameBuffer[frame_idx++] = 0x7E; 

  appendHexAscii(_VER, frame_idx);
  appendHexAscii(_ADR, frame_idx);
  appendHexAscii(_CID1, frame_idx);
  appendHexAscii((byte)0x00, frame_idx); 

  appendLengthField(info_ascii_len, frame_idx);

  int checksum_start_index = 1;

  for(int i=0; i < info_binary_len; i++) {
    appendHexAscii(info_payload_biner[i], frame_idx);
  }

  int checksum_len = frame_idx - checksum_start_index;
  uint16_t checksum = calculatePylontechChecksum(&_frameBuffer[checksum_start_index], checksum_len);
  
  appendHexAscii(checksum, frame_idx);
  _frameBuffer[frame_idx++] = 0x0D; 
  
  _frameLength = frame_idx;
  return _frameBuffer;
}


void PylontechEncoder::appendHexAscii(byte value, int &index) {
  _frameBuffer[index++] = nibbleToHexChar(value >> 4);   
  _frameBuffer[index++] = nibbleToHexChar(value & 0x0F); 
}

void PylontechEncoder::appendHexAscii(uint16_t value, int &index) {
  appendHexAscii((byte)((value >> 8) & 0xFF), index); 
  appendHexAscii((byte)(value & 0xFF), index);      
}

void PylontechEncoder::appendHexAscii(int16_t value, int &index) {
  appendHexAscii((uint16_t)value, index);
}

void PylontechEncoder::appendLengthField(int info_ascii_len, int &index) {
  uint16_t lenid = info_ascii_len & 0x0FFF;
  uint16_t lchksum_part1 = (lenid >> 8) & 0x0F;
  uint16_t lchksum_part2 = (lenid >> 4) & 0x0F;
  uint16_t lchksum_part3 = lenid & 0x0F;
  uint16_t lchksum_val = lchksum_part1 + lchksum_part2 + lchksum_part3;
  lchksum_val = (~(lchksum_val & 0x0F)) + 1;
  lchksum_val &= 0x0F;
  
  uint16_t length_field = (lchksum_val << 12) | lenid;
  appendHexAscii(length_field, index);
}

uint16_t PylontechEncoder::calculatePylontechChecksum(byte* data, int len) {
  uint16_t sum = 0;
  for (int i = 0; i < len; i++) {
    sum += data[i];
  }
  sum = (~sum) + 1;
  return (sum & 0xFFFF); 
}