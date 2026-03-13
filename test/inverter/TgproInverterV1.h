#ifndef TGPRO_INVERTER_V1_H
#define TGPRO_INVERTER_V1_H

#include <Arduino.h>
#include <ModbusMaster.h>

// ==========================================
// 1. STRUKTUR DATA INVERTER
// ==========================================
struct TgproData {
  uint8_t Op_Mode;        //         201

  float V_Grid;           // (1/10)  202
  float Hz_Grid;          // (1/100) 203
  int16_t W_Grid;         //         204 

  float V_Inverter;       // (1/10)  205
  float I_Inverter;       // (1/10)  206
  float Hz_Inverter;      // (1/100) 207
  int16_t W_Inverter;     //         208
  uint16_t W_Ch_Inv;      //         209
  float I_Ch_Inv;         // (1/10)  233

  float V_Out;            // (1/10)  210
  float I_Out;            // (1/10)  211
  float Hz_Out;           // (1/100) 212
  uint16_t W_Out;         //         213
  uint16_t VA_Out;        //         214

  float V_Batt;           // (1/10)  215
  float I_Batt;           // (1/10)  216 
  int16_t W_Batt;         //         217 
  uint8_t SOC_Batt;       //         229
  float I_PC_ND_Batt;     // (1/10)  232

  float V_PV;             // (1/10)  219
  float I_PV;             // (1/10)  220
  uint16_t W_PV;          //         223
  uint16_t W_Ch_PV;       //         224
  float I_Ch_PV;          //         234

  uint8_t Percent_Load;   //         225
  int8_t T_DCDC;          //         226
  int8_t T_Inverter;      //         227

  // SETTINGS
  uint8_t Out_Mode;       //         300
  uint8_t Out_Priority;   //         301
  bool V_In_Range;        //         302
  uint8_t Buzz_Mode;      //         303

  bool LCD_Light;         //         305
  bool LCD_RTH;           //         306
  bool Energy_Save;       //         307

  bool Overload_Reset;    //         308
  bool Over_T_Reset;      //         309
  bool Overload_Bypass;   //         310

  bool Batt_Eq;           //         313
  float V_Ch_Eq;          // (1/10)  334
  uint8_t Time_Eq;        // (min)   335
  uint8_t Timeout_Eq;     // (min)   336
  uint8_t Ch_Eq_Interval; // (day)   337

  float Set_V_Out;        // (1/10)  320
  float Set_Hz_Out;       // (1/100) 321

  float Max_V_Ch;         // (1/10)  324
  float Float_V_Ch;       // (1/10)  325
  float Max_I_Ch;         // (1/10)  332
  float Max_I_Ch_Grid;    // (1/10)  333
  uint8_t Ch_Batt_Priority;//        331

  float OV_Batt_Pro;      // (1/10)  323
  float V_Batt_Start_Dch; // (1/10)  326
  float V_Batt_Low_Grid;  // (1/10)  327
  float V_Batt_Low_OfG;   // (1/10)  329

  uint8_t Turn_On_Mode;   //         406
  bool Remote_Sw;         //         420
  uint8_t Ext_Fault_Mode; // (WO)    426
};

// ==========================================
// 2. CETAK BIRU CLASS
// ==========================================
class TgproInverter {
  public:
    // Constructor
    TgproInverter(HardwareSerial& serialPort, int rxPin, int txPin, int deRePin = -1);

    void begin();
    void bacaInverter(); 
    const TgproData& getData() const;

  private:
    HardwareSerial& _serial;
    ModbusMaster _node;
    int _rxPin;
    int _txPin;
    
    // Harus static agar bisa dipakai oleh callback ModbusMaster
    static int _deRePin; 
    
    TgproData _data;

    // Callback kontrol RS485
    static void preTransmission();
    static void postTransmission();
};

#endif