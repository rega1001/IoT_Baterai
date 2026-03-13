#ifndef KIRIM_DATA_MQTT_H
#define KIRIM_DATA_MQTT_H

#include <Arduino.h>

// 1. Masukkan semua header yang tipe datanya dipakai di fungsi ini
#include "PylontechEncoder.h"
#include "JkBmsDriver.h"
#include "TgproInverter.h"
#include "CommsManager.h"

// 2. Deklarasi EXTERN untuk meminjam objek global dari main.cpp
extern CommsManager comms;
extern JkBmsDriver jkBms;
extern PylontechEncoder pylonEncoder; 
extern TgproInverter tgproInverter;

// 3. Deklarasi fungsi yang ada di main.cpp / file lain agar dikenali di sini
void getWaktuSaatIni(char* wadahWaktu, size_t ukuranMaksimal);

// 4. Deklarasi fungsi utama kita
void kirimBmsStatus(const char* topic_prefix);
void kirimDataInverter(const char* topic_prefix);

#endif