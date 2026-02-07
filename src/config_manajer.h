#ifndef CONFIG_MANAJER_H
#define CONFIG_MANAJER_H
// ===============================
// config_manager.h
// ===============================
#include <Arduino.h>
#include <ArduinoJson.h>
#include "cuaca.h"


extern bool muatJadwal();

// --- SYSTEM ----
bool loadSystemConfig();
bool saveSystemConfig(JsonVariant dataWeb = JsonVariant());
// --- HAMA ---
bool loadHama();
bool saveHama();
// --- SHOLAT ---
bool loadSholatConfig();
bool saveSholatConfig(JsonVariant dataWeb = JsonVariant());
// --- TANGGAL ---
bool loadTanggalConfig();
bool saveTanggalConfig();
// --- EFEK ----
bool loadEfekConfig();
bool saveEfekConfig(JsonVariant dataWeb = JsonVariant());

bool loadSkor();
bool saveSkor();
bool loadWifi();
bool saveWifi();
bool loadJam();
bool saveJam();

bool saveCuaca();
bool muatCuaca();

bool muatJadwal();

//---LOAD JSON---
bool reloadConfigByTarget(const String& target);
bool getConfigPath(const String& target, String& path);

#endif