#ifndef CUACA_H
#define CUACA_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include "config_manajer.h"

struct CuacaWarna {
  bool aktif;  // Warna hasil deteksi cuaca
  int kodeCuaca;      // Kode dari BMKG
  String kondisi;
  String jam;     // Teks kondisi (cerah, hujan, dll)
};
struct WarnaCuacaConfig {
  uint32_t cerah;
  uint32_t cerahBerawan;
  uint32_t berawan;
  uint32_t mendung;
  uint32_t kabut;
  uint32_t hujan;
  uint32_t petir;
};
struct cuacaKonfig{
CuacaWarna status;
WarnaCuacaConfig warna;
};


extern cuacaKonfig cuaca;


uint32_t tentukanWarnaCuaca(int kode);
void setupCuaca();
//void handleCuaca();
void handleCuaca();
void updateCuacaBMKG();
bool muatCuaca();
void handleSimpan();

//bool saveSystemConfig();
//bool saveHama();

#endif