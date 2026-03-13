#include <Arduino.h>
#include <ModbusMaster.h>

// ==========================================
// 1. STRUKTUR DATA INVERTER
// ==========================================
struct TgproData {
  uint8_t Op_Mode;	//			          201

  float V_Grid; // (1/10)			        202
  float Hz_Grid; // (1/100)			      203
  uint16_t W_Grid;	//			          204

  float V_Inverter; // (1/10)		      205
  float I_Inverter; // (1/10)		      206
  float Hz_Inverter; // (1/100)		    207
  uint16_t W_Inverter;	//		        208
  uint16_t W_Ch_Inv;	//		          209
  float I_Ch_Inv; // (1/10)			      233

  float V_Out; // (1/10)			        210
  float I_Out; // (1/10)			        211
  float Hz_Out; // (1/100)			      212
  uint16_t W_Out; //				          213
  uint16_t VA_Out;	//			          214

  float V_Batt; // (1/10)			        215
  float I_Batt; // (1/10)			        216
  uint16_t W_Batt; //				          217
  uint8_t SOC_Batt;	//		            229
  float I_PC_ND_Batt; // (1/10)		    232

  float V_PV; // (1/10)			          219
  float I_PV; // (1/10)			          220
  uint16_t W_PV; //				            223
  uint16_t W_Ch_PV; //				        224
  float I_Ch_PV; //				            234

  uint8_t Percent_Load; //			      225
  int8_t T_DCDC; //				            226
  int8_t T_Inverter; //			          227

  // SETINGS
  uint8_t Out_Mode;	//		            300
  uint8_t Out_Priority;	//		        301
  bool V_In_Range;	//		        302
  uint8_t Buzz_Mode;	//		        303

  bool LCD_Light;	//		        305
  bool LCD_RTH;	//		        306
  bool Energy_Save;	//		        307

  bool Overload_Reset; //			    308
  bool Over_T_Reset; //			    309
  bool Overload_Bypass; //			    310

  bool Batt_Eq;	//			        313
  float V_Ch_Eq; // (1/10)			    334
  uint8_t Time_Eq; // (min)			    335
  uint8_t Timeout_Eq; // (min)		    336
  uint8_t Ch_Eq_Interval; // (day)	    337

  float Set_V_Out; // (1/10)		    320
  float Set_Hz_Out; // (1/100)		    321

  float Max_V_Ch; // (1/10)			    324
  float Float_V_Ch; // (1/10)		    325
  float Max_I_Ch; // (1/10)			    332
  float Max_I_Ch_Grid; // (1/10)		333
  uint8_t Ch_Batt_Priority; //		    331

  float OV_Batt_Pro; // (1/10)		    323
  float V_Batt_Start_Dch; // (1/10)		326
  float V_Batt_Low_Grid; // (1/10)		327
  float V_Batt_Low_OfG;	// (1/10)		329

  uint8_t Turn_On_Mode;	//			    406
  bool Remote_Sw;	//			        420
  uint8_t Ext_Fault_Mode; // (WO)		    426
};

TgproData tgpro; // Wadah untuk menyimpan data

// ==========================================
// 2. SETUP MODBUS & SERIAL
// ==========================================
ModbusMaster node;

// Pin untuk Serial2 ke Inverter
const int RX_PIN = 33;
const int TX_PIN = 32;

// (OPSIONAL) Jika Anda pakai modul RS485 Max485, butuh pin DE/RE
// Jika pakai RS232 (Max3232), abaikan pin ini.
const int MAX485_RE_NEG = 4;
const int MAX485_DE = 4;

// Fungsi kontrol arah aliran data untuk Max485 (Abaikan jika RS232)
void preTransmission() { digitalWrite(MAX485_RE_NEG, 1); }
void postTransmission() { digitalWrite(MAX485_RE_NEG, 0); }

void setup() {
  Serial.begin(115200);
  
  // Konfigurasi pin RS485 (Jika pakai)
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);

  // Mulai Serial2 untuk ke Inverter (Baud rate standar PowMr/SRNE biasanya 9600)
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  // Mulai Modbus di ID 1, lewat Serial2
  node.begin(1, Serial2);
  
  // Daftarkan fungsi kontrol RS485
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  Serial.println("Sistem TGPRO / PowMr Siap!");
}

// ==========================================
// 3. FUNGSI PEMBACAAN (CERDAS & NON-BLOCKING)
// ==========================================
void bacaInverter() {
  uint8_t result;
  
  Serial.println("--- Mengambil Data Inverter ---");

  // ------------------------------------------------
  // BLOK 1: Baca Grid
  // ------------------------------------------------
  result = node.readHoldingRegisters(202, 3); 
  if (result == node.ku8MBSuccess) {
    tgpro.V_Grid = node.getResponseBuffer(0) / 10.0; // Reg 202
    tgpro.Hz_Grid = node.getResponseBuffer(1) / 100.0; // Reg 203
    tgpro.W_Grid = node.getResponseBuffer(2); // Reg 204
    
    Serial.printf("Grid: %.1f V | %.2f Hz | Power: %u W\n", tgpro.V_Grid, tgpro.Hz_Grid, tgpro.W_Grid);
  } else {
    Serial.println("Gagal baca Blok Grid");
  }  delay(10); 

  // ------------------------------------------------
  // BLOK 2: Baca Blok Inverter
  // ------------------------------------------------
  result = node.readHoldingRegisters(205, 5);
  if (result == node.ku8MBSuccess) {
    tgpro.V_Inverter    = node.getResponseBuffer(0) / 10.0;       // Reg 205
    tgpro.I_Inverter    = node.getResponseBuffer(1) / 10.0;       // Reg 206
    tgpro.Hz_Inverter   = node.getResponseBuffer(3) / 100.0;      // Reg 207
    tgpro.W_Inverter    = node.getResponseBuffer(4);              // Reg 208
    tgpro.W_Ch_Inv    = node.getResponseBuffer(5);              // Reg 209
    
    Serial.printf("Inverter V: %.1f V | I: %.1f A | Hz: %.2f | Power: %u W | Charge Power: %u W\n", 
                  tgpro.V_Inverter, tgpro.I_Inverter, tgpro.Hz_Inverter, tgpro.W_Inverter, tgpro.W_Ch_Inv);
  } else {
    Serial.println("Gagal baca Blok Inverter");
  }  delay(10);

  // ------------------------------------------------
  // BLOK 3: Baca Blok Output
  // ------------------------------------------------
  result = node.readHoldingRegisters(210, 5);
  if (result == node.ku8MBSuccess) {
    tgpro.V_Out = node.getResponseBuffer(0) / 10.0;   // Reg 210
    tgpro.I_Out   = node.getResponseBuffer(1) / 10.0; // Reg 211
    tgpro.Hz_Out = node.getResponseBuffer(2)/ 100.0;  // Reg 212
    tgpro.W_Out = node.getResponseBuffer(3);          // Reg 213
    tgpro.VA_Out = node.getResponseBuffer(4);         // Reg 214
    
    Serial.printf("Output V: %.1f V | I: %.1f A | Freq: %.2f Hz | Power: %u W | VA Power: %u VA\n", 
                  tgpro.V_Out, tgpro.I_Out, tgpro.Hz_Out, tgpro.W_Out, tgpro.VA_Out);
  } else {
    Serial.println("Gagal baca Blok Output");
  }  delay(10);

  // ------------------------------------------------
  // BLOK 4: Baca Blok Baterai
  // ------------------------------------------------
  result = node.readHoldingRegisters(215, 3);
  if (result == node.ku8MBSuccess) {
    tgpro.V_Batt = node.getResponseBuffer(0) / 10.0;  // Reg 215
    tgpro.I_Batt = node.getResponseBuffer(1) / 10.0; // Reg 216
    tgpro.W_Batt = node.getResponseBuffer(2);            // Reg 217
  } delay(10);
  result = node.readHoldingRegisters(229, 1);
  if (result == node.ku8MBSuccess) {
    tgpro.SOC_Batt = node.getResponseBuffer(0);  // Reg 229
    
    Serial.printf("Batt V: %.1f V | I: %.1f A | Power: %u W | SOC: %u%\n", tgpro.V_Batt, tgpro.I_Batt, tgpro.W_Batt, tgpro.SOC_Batt);
  } else {
    Serial.println("Gagal baca Blok Baterai");
  } delay(10);

  // ------------------------------------------------
  // BLOK 5: Baca Blok PV
  // ------------------------------------------------
  result = node.readHoldingRegisters(219, 4);
  if (result == node.ku8MBSuccess) {
    tgpro.V_PV = node.getResponseBuffer(0) / 10.0;  // Reg 219
    tgpro.I_PV = node.getResponseBuffer(1) / 10.0; // Reg 220
    tgpro.W_PV = node.getResponseBuffer(2);            // Reg 221
    tgpro.W_Ch_PV = node.getResponseBuffer(3);           // Reg 222
    
    Serial.printf("PV V: %.1f V | I: %.1f A | Power: %u W | Charge Power: %u W\n", tgpro.V_PV, tgpro.I_PV, tgpro.W_PV, tgpro.W_Ch_PV);
  } else {
    Serial.println("Gagal baca Blok PV");
  } delay(10);

  // ------------------------------------------------
  // BLOK 6: Baca Blok Load and Temperature
  // ------------------------------------------------
  result = node.readHoldingRegisters(225, 3);
  if (result == node.ku8MBSuccess) {
    tgpro.Percent_Load = node.getResponseBuffer(0);  // Reg 225
    tgpro.T_DCDC = node.getResponseBuffer(1); // Reg 226
    tgpro.T_Inverter = node.getResponseBuffer(2);            // Reg 227
    
    Serial.printf("Load: %u% | DCDC Temp: %d°C | Inverter Temp: %d°C\n", tgpro.Percent_Load, tgpro.T_DCDC, tgpro.T_Inverter);
  } else {
    Serial.println("Gagal baca Blok Load and Temperature");
  } delay(10);

  // ------------------------------------------------
  // BLOK 7: Baca Blok Arus Charging / Discharging
  // ------------------------------------------------
  result = node.readHoldingRegisters(232, 3);
  if (result == node.ku8MBSuccess) {
    tgpro.I_PC_ND_Batt = node.getResponseBuffer(0);        // Reg 232
    tgpro.I_Ch_Inv = node.getResponseBuffer(1);            // Reg 233
    tgpro.I_Ch_PV = node.getResponseBuffer(2);             // Reg 234
    
    Serial.printf("Load: %u% | DCDC Temp: %d°C | Inverter Temp: %d°C\n", tgpro.Percent_Load, tgpro.T_DCDC, tgpro.T_Inverter);
  } else {
    Serial.println("Gagal baca Blok Arus Charging / Discharging");
  } delay(10);

  // ------------------------------------------------
  // BLOK 8: Baca Blok Setting 300 - 309
  // ------------------------------------------------
  result = node.readHoldingRegisters(300, 10);
  if (result == node.ku8MBSuccess) {
    tgpro.Out_Mode = node.getResponseBuffer(0);     // Reg 300
    tgpro.Out_Priority = node.getResponseBuffer(1);  // Reg 301
    tgpro.V_In_Range = node.getResponseBuffer(2);    // Reg 302
    tgpro.Buzz_Mode = node.getResponseBuffer(3);     // Reg 303
    tgpro.LCD_Light = node.getResponseBuffer(5);     // Reg 305
    tgpro.LCD_RTH = node.getResponseBuffer(6);       // Reg 306
    tgpro.Energy_Save = node.getResponseBuffer(7);    // Reg 307
    tgpro.Overload_Reset = node.getResponseBuffer(8);  // Reg 308
    tgpro.Over_T_Reset = node.getResponseBuffer(9);    // Reg 309
    
    Serial.printf("Out Mode: %u | Out Priority: %u | V In Range: %u | Buzz Mode: %u\n", tgpro.Out_Mode, tgpro.Out_Priority, tgpro.V_In_Range, tgpro.Buzz_Mode);
    Serial.printf("LCD Light: %u | LCD RTH: %u | Energy Save: %u | Overload Reset: %u | Over T Reset: %u\n", tgpro.LCD_Light, tgpro.LCD_RTH, tgpro.Energy_Save, tgpro.Overload_Reset, tgpro.Over_T_Reset);
  } else {
    Serial.println("Gagal Baca Blok Setting 300 - 309");
  } delay(10);

  // ------------------------------------------------
  // BLOK 9: Baca Blok Setting 310 - 313
  // ------------------------------------------------
  result = node.readHoldingRegisters(310, 4);
  if (result == node.ku8MBSuccess) {
    tgpro.Overload_Bypass = node.getResponseBuffer(0);    // Reg 310
    tgpro.Batt_Eq = node.getResponseBuffer(3);            // Reg 313
    
    Serial.printf("Overload Bypass: %u | Batt Eq: %u\n", tgpro.Overload_Bypass, tgpro.Batt_Eq);
  } else {
    Serial.println("Gagal Baca Blok Setting 310 - 313");
  } delay(10);
  
  // ------------------------------------------------
  // BLOK 10: Baca Blok Setting 320 - 329
  // ------------------------------------------------
  result = node.readHoldingRegisters(320, 10);
  if (result == node.ku8MBSuccess) {
    tgpro.Set_V_Out = node.getResponseBuffer(0);     // Reg 320
    tgpro.Set_Hz_Out = node.getResponseBuffer(1);     // Reg 321
    tgpro.OV_Batt_Pro = node.getResponseBuffer(3);     // Reg 323
    tgpro.Max_V_Ch = node.getResponseBuffer(4);       // Reg 324
    tgpro.Float_V_Ch = node.getResponseBuffer(5);      // Reg 325
    tgpro.V_Batt_Start_Dch = node.getResponseBuffer(6); // Reg 326
    tgpro.V_Batt_Low_Grid = node.getResponseBuffer(7); // Reg 327
    tgpro.V_Batt_Low_OfG = node.getResponseBuffer(9);  // Reg 329

    Serial.printf("Set V Out: %u | Set Hz Out: %u | OV Batt Pro: %u | Max V Ch: %u\n", tgpro.Set_V_Out, tgpro.Set_Hz_Out, tgpro.OV_Batt_Pro, tgpro.Max_V_Ch);
    Serial.printf("Float V Ch: %u | V Batt Start Dch: %u | V Batt Low Grid: %u | V Batt Low OfG: %u\n", tgpro.Float_V_Ch, tgpro.V_Batt_Start_Dch, tgpro.V_Batt_Low_Grid, tgpro.V_Batt_Low_OfG);
  } else {
    Serial.println("Gagal Baca Blok Setting 320 - 329");
  } delay(10);
  
  // ------------------------------------------------
  // BLOK 11: Baca Blok Setting 331 - 337
  // ------------------------------------------------
  result = node.readHoldingRegisters(331, 7);
  if (result == node.ku8MBSuccess) {
    tgpro.Ch_Batt_Priority = node.getResponseBuffer(0); // Reg 331
    tgpro.Max_I_Ch = node.getResponseBuffer(1);         // Reg 332
    tgpro.Max_I_Ch_Grid = node.getResponseBuffer(2);    // Reg 333
    tgpro.V_Ch_Eq = node.getResponseBuffer(3);          // Reg 334
    tgpro.Time_Eq = node.getResponseBuffer(4);          // Reg 335
    tgpro.Timeout_Eq = node.getResponseBuffer(5);       // Reg 336
    tgpro.Ch_Eq_Interval = node.getResponseBuffer(6);   // Reg 337

    Serial.printf("Ch Batt Priority: %u | Max I Ch: %u | Max I Ch Grid: %u | V Ch Eq: %u\n", tgpro.Ch_Batt_Priority, tgpro.Max_I_Ch, tgpro.Max_I_Ch_Grid, tgpro.V_Ch_Eq);
    Serial.printf("Time Eq: %u | Timeout Eq: %u | Ch Eq Interval: %u\n", tgpro.Time_Eq, tgpro.Timeout_Eq, tgpro.Ch_Eq_Interval);
  } else {
    Serial.println("Gagal Baca Blok Setting 331 - 337");
  } delay(10);

  // ------------------------------------------------
  // BLOK 12: Baca Blok Setting 406 dan 420
  // ------------------------------------------------
  result = node.readHoldingRegisters(406,1);
  if (result == node.ku8MBSuccess) {
    tgpro.Turn_On_Mode = node.getResponseBuffer(0); // Reg 406
    Serial.printf("Turn On Mode: %u\n", tgpro.Turn_On_Mode);
  } delay(10);
  result = node.readHoldingRegisters(420,1);
  if (result == node.ku8MBSuccess) {
    tgpro.Set_V_Out = node.getResponseBuffer(0); // Reg 420
    Serial.printf("Set V Out: %u\n", tgpro.Set_V_Out);
  } else{
    Serial.println("Gagal Baca Blok Setting 406 dan 420");
  } delay(10);

}


// ==========================================
// 4. MAIN LOOP
// ==========================================
void loop() {
  static unsigned long lastRead = 0;
  
  // Baca setiap 10 detik sekali (Non-blocking)
  if (millis() - lastRead > 30000) {
    lastRead = millis();
    bacaInverter();
    
    // Nanti di sini Anda bisa tambahkan:
    // mqtt.publishTgproData(tgpro);
  }
}