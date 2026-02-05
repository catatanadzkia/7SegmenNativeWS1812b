// ===============================
// config_manajer.cpp (FIXED VERSION)
// ===============================
#include "config_manajer.h"
#include "configSystem.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <FastLED.h>


// ---------- UTIL & HELPER ----------

// Fungsi Wrapper yang AMAN: Cek exist dulu baru buka
static bool openJson(const char* path, DynamicJsonDocument& doc) {
  // PENGAMAN 1: Cek apakah file ada?
  if (!LittleFS.exists(path)) {
    Serial.print("⚠️ File tidak ditemukan: ");
    Serial.println(path);
    return false; // Jangan paksa buka "r"
  }

  File f = LittleFS.open(path, "r");
  // PENGAMAN 2: Cek apakah gagal buka atau folder
  if (!f || f.isDirectory()) {
    Serial.println("❌ Gagal buka file (Corrupt/Folder)");
    return false;
  }
  
  // PENGAMAN 3: Cek ukuran file
  if (f.size() == 0) {
    Serial.println("⚠️ File kosong (0 bytes)");
    f.close();
    return false;
  }

  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("❌ JSON Error: ");
    Serial.println(err.c_str());
    return false;
  }
  
  return true;
}

static bool writeJson(const char* path, DynamicJsonDocument& doc) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if(ok) Serial.printf("✅ Berhasil simpan: %s\n", path);
  else Serial.printf("❌ Gagal simpan: %s\n", path);
  return ok;
}

bool reloadConfigByTarget(const String& target) {
  if (target == "system") return loadSystemConfig();
  if (target == "sholat") {
    DynamicJsonDocument docTemp(2048);
    if (openJson("/sholat.json", docTemp)) {
      return saveSholatConfig(docTemp.as<JsonVariant>());
    }
  }
  if (target == "tanggal") return loadTanggalConfig();
  if (target == "efek") {
    DynamicJsonDocument docTemp(2048);
    if (openJson("/efek.json", docTemp)) {
      return saveEfekConfig(docTemp.as<JsonVariant>());
    }
  }
  if (target == "ultra") return loadHama();
  if (target == "skor") return loadSkor();
  if (target == "jadwal") return muatJadwal();
  if (target == "jam") return loadJam();
  if (target == "cuaca") return muatCuaca();

  return false;
}

bool getConfigPath(const String& target, String& path) {
  if (target == "system") path = "/system.json";
  else if (target == "sholat") path = "/sholat.json";
  else if (target == "tanggal") path = "/tanggal.json";
  else if (target == "efek") path = "/efek.json";
  else if (target == "ultra") path = "/ultra.json";
  else if (target == "skor") path = "/skor.json";
  else if (target == "jadwal") path ="/jadwal.json";
  else if (target == "jam") path = "/jam.json";
  else if (target == "cuaca") path = "/cuaca.json";
  else return false;
  return true;
}


// ---------- SYSTEM ----------
bool loadSystemConfig() {
  // LOGIKA PENYELAMAT: Kalau file gak ada, buat baru dari default RAM
  if (!LittleFS.exists("/system.json")) {
    Serial.println("⚙️ Membuat Default /system.json...");
    // Pastikan nilai RAM standar dulu (kalau-kalau belum init)
    standarConfig(); 
    saveSystemConfig(); // Tulis ke LittleFS
  }

  DynamicJsonDocument doc(1024);
  if (!openJson("/system.json", doc)) {
    // Kalau gagal baca (corrupt), reset ke standar
    Serial.println("⚠️ System Config Corrupt -> Resetting");
    standarConfig();
    saveSystemConfig();
    return false;
  }

  auto g = doc["global"];
  konfig.jumlahDigit = g["dg"].as<int>() | konfig.jumlahDigit;
  konfig.kecerahanGlobal = g["br"] | konfig.kecerahanGlobal;
  strlcpy(konfig.namaPerangkat, g["dev"] | konfig.namaPerangkat, sizeof(konfig.namaPerangkat));
  strlcpy(konfig.urlGAS, g["gas"] | konfig.urlGAS, sizeof(konfig.urlGAS));
  strlcpy(konfig.terakhirSync, g["sncy"] | konfig.terakhirSync, sizeof(konfig.terakhirSync));
  strlcpy(konfig.authUser, g["user"] | konfig.authUser, sizeof(konfig.authUser));
  strlcpy(konfig.authPass, g["pass"] | konfig.authPass, sizeof(konfig.authPass));
  konfig.modeKedip = g["mk"] | konfig.modeKedip;
  konfig.speedKedip = g["sk"] | konfig.speedKedip;
  konfig.durasiKedip = g["dk"] | konfig.durasiKedip;
  konfig.titikDuaKedip = g["t2k"] | konfig.titikDuaKedip;
  konfig.loopSetiap = g["ls"] | konfig.loopSetiap;
  konfig.loopLama = g["ll"] | konfig.loopLama;
  konfig.fmtTgl = g["ft"] | konfig.fmtTgl;
  konfig.timezone = g["tz"] | konfig.timezone;
  konfig.warnajam = g["wj"] | konfig.warnajam;

  auto w = doc["wifi"];
  konfig.wikon.ssid = w["idwifi"] | konfig.wikon.ssid;
  konfig.wikon.pass = w["pas"] | konfig.wikon.pass;

  Serial.println("📂 System Config Loaded.");
  return true;
}

bool saveSystemConfig() {
  DynamicJsonDocument doc(1024);
  auto g = doc.createNestedObject("global");
  g["dg"] = konfig.jumlahDigit;
  g["br"] = konfig.kecerahanGlobal;
  g["dev"] = konfig.namaPerangkat;
  g["gas"] = konfig.urlGAS;
  g["sncy"] = konfig.terakhirSync;
  g["user"] = konfig.authUser;
  g["pass"] = konfig.authPass;
  g["mk"] = konfig.modeKedip;
  g["sk"] = konfig.speedKedip;
  g["dk"] = konfig.durasiKedip;
  g["t2k"] = konfig.titikDuaKedip;
  g["ls"] = konfig.loopSetiap;
  g["ll"] = konfig.loopLama;
  g["ft"] = konfig.fmtTgl;
  g["tz"] = konfig.timezone;
  g["wj"] = konfig.warnajam;

  auto w = doc.createNestedObject("wifi");
  w["idwifi"] = konfig.wikon.ssid;
  w["pas"] = konfig.wikon.pass;

  return writeJson("/system.json", doc);
}

// ---------- HAMA / ULTRA ----------
bool loadHama() {
  // LOGIKA PENYELAMAT
  if (!LittleFS.exists("/ultra.json")) {
    Serial.println("⚙️ Membuat Default /ultra.json...");
    konfig.pengusirHama.aktif = true; // Set default manual
    saveHama();
  }

  DynamicJsonDocument doc(512);
  if (!openJson("/ultra.json", doc)) {
    return false;
  }

  auto ul = doc["ultra"];
  konfig.pengusirHama.aktif = ul["on"] | konfig.pengusirHama.aktif;
  konfig.pengusirHama.frekuensiMin = ul["fMin"] | konfig.pengusirHama.frekuensiMin;
  konfig.pengusirHama.frekuensiMax = ul["fMax"] | konfig.pengusirHama.frekuensiMax;
  konfig.pengusirHama.intervalAcak = ul["int"] | konfig.pengusirHama.intervalAcak;
  konfig.pengusirHama.aktifMalamSaja = ul["malam"] | konfig.pengusirHama.aktifMalamSaja;
  
  Serial.println("📂 Hama Config Loaded.");
  return true;
}

bool saveHama() {
  DynamicJsonDocument doc(512);
  auto ul = doc.createNestedObject("ultra");
  ul["on"] = konfig.pengusirHama.aktif;
  ul["fMin"] = konfig.pengusirHama.frekuensiMin;
  ul["fMax"] = konfig.pengusirHama.frekuensiMax;
  ul["int"] = konfig.pengusirHama.intervalAcak;
  ul["malam"] = konfig.pengusirHama.aktifMalamSaja;

  return writeJson("/ultra.json", doc);
}

bool loadJam() {
  if (!LittleFS.exists("/jam.json")) {
    saveJam(); 
    return true;
  }

  DynamicJsonDocument doc(512);
  if (!openJson("/jam.json", doc)) return false;

  // Tetap pakai root "jam" sesuai kemauan Bos
  JsonObject jm = doc["jam"];
  
  konfig.visjam.titikAktif = jm["on_t"] | true;
  konfig.visjam.animasiAktif = jm["on_a"] | false;
  konfig.visjam.idAnimasi = jm["id_a"] | 0;

  JsonArray w = jm["w"];
  if (!w.isNull()) {
    for (int i = 0; i < 4; i++) {
      if (i < w.size()) {
        // Ambil string-nya secara eksplisit
        String hexText = w[i].as<String>(); 
        
        // Konversi ke angka. Gunakan base 0 supaya otomatis deteksi "0x"
        uint32_t warnaAngka = (uint32_t)strtoul(hexText.c_str(), NULL, 0);
        
        konfig.visjam.warna[i] = warnaAngka;

        // CEK DI SERIAL MONITOR:
        Serial.printf("LOAD -> Digit %d: %s -> Hasil: %06X\n", i, hexText.c_str(), warnaAngka);
      }
    }
  } else {
    Serial.println("EROR: Array warna 'w' tidak ditemukan di JSON!");
  }
  return true;
}

bool saveJam() {
  // 1. Siapkan dokumen JSON
  DynamicJsonDocument doc(512);
  
  // 2. Buat root object "jam" sesuai struktur loadJam
  JsonObject jm = doc.createNestedObject("jam");
  
  // 3. Masukkan data konfigurasi
  jm["on_t"] = konfig.visjam.titikAktif;
  jm["on_a"] = konfig.visjam.animasiAktif;
  jm["id_a"] = konfig.visjam.idAnimasi;

  // 4. Masukkan array warna "w"
  JsonArray w = jm.createNestedArray("w");
  for (int i = 0; i < 4; i++) {
    char hexBuffer[12];
    
    // Ambil warna dari struct dan konversi ke CRGB untuk ambil komponen R,G,B
    CRGB c = (uint32_t)konfig.visjam.warna[i]; 
    
    // Format menjadi string HEX (contoh: 0xa51d2d)
    sprintf(hexBuffer, "0x%02x%02x%02x", c.r, c.g, c.b);
    
    // Tambahkan ke dalam array JSON
    w.add(hexBuffer);
  }

  // 5. Tulis file ke LittleFS
  if (writeJson("/jam.json", doc)) {
    Serial.println("SAVE -> Berhasil menyimpan /jam.json");
    return true;
  } else {
    Serial.println("SAVE -> Gagal menulis file /jam.json!");
    return false;
  }
}


bool loadSkor() {
  // LOGIKA PENYELAMAT
  if (!LittleFS.exists("/skor.json")) {
    Serial.println("⚙️ Membuat Default /skor.json...");
    konfig.papanSkor.skorA = 0;
    konfig.papanSkor.skorB = 0;
    saveSkor();
  }

  DynamicJsonDocument doc(512);
  if (!openJson("/skor.json", doc)) {
    return false;
  }

  auto sek = doc["papanSkor"];
  konfig.papanSkor.skorA = sek["skorA"] | konfig.papanSkor.skorA;
  konfig.papanSkor.skorB = sek["skorB"] | konfig.papanSkor.skorB;
  konfig.papanSkor.babak = sek["bbk"] | konfig.papanSkor.babak;
  konfig.papanSkor.aktif = sek["on"] | konfig.papanSkor.aktif;
  
  Serial.println("📂 Skor Config Loaded.");
  return true;
}

bool saveSkor() {
  DynamicJsonDocument doc(512);
  auto sek = doc.createNestedObject("papanSkor");
  sek["skorA"] = konfig.papanSkor.skorA;
  sek["skorB"] = konfig.papanSkor.skorB;
  sek["bbk"] = konfig.papanSkor.babak;
  sek["on"] = konfig.papanSkor.aktif;

  return writeJson("/skor.json", doc);
}
bool loadWifi() {
  // LOGIKA PENYELAMAT
  if (!LittleFS.exists("/config.json")) {
    Serial.println("⚙️ Membuat Default /skor.json...");
    
    saveWifi();
    
  }

  DynamicJsonDocument doc(512);
  if (!openJson("/config.json", doc)) {
    return false;
  }

  auto wiFi= doc["wifi"];
  konfig.wikon.pass = wiFi["pas"] | konfig.wikon.pass;
  konfig.wikon.ssid = wiFi["sid"] | konfig.wikon.ssid;
  
  
  Serial.println("📂 Skor Config Loaded.");
  return true;
}
bool saveWifi() {
  DynamicJsonDocument doc(512);
  auto wiFi = doc.createNestedObject("wifi");
  wiFi["pas"] = konfig.wikon.pass;
  wiFi["sid"] = konfig.wikon.ssid;

  return writeJson("/config.json", doc);
}
// ---------- SHOLAT ----------
bool loadSholatConfig() {
  if (!LittleFS.exists("/sholat.json")) {
    saveSholatConfig(); 
  }

  DynamicJsonDocument doc(3072);
  if (!openJson("/sholat.json", doc)) return false;
  
  JsonArray a = doc.as<JsonArray>();
  for (int i = 0; i < 6 && i < (int)a.size(); i++) {
    JsonObject s = a[i];

    // Data Angka & Status Tetap Sama
    konfig.sholat[i].menitPeringatan  = s["mP"] | 2;
    konfig.sholat[i].idEfekPeringatan = s["iEP"] | 0;
    konfig.sholat[i].hitungMundur     = s["cDown"] | 0;
    konfig.sholat[i].idEfekAdzan      = s["iEA"] | 0; // Sesuaikan key JSON Bos
    konfig.sholat[i].durasiAdzan      = s["durA"] | 0;
    konfig.sholat[i].menitHemat       = s["mH"] | 0;
    konfig.sholat[i].kecerahanHemat   = s["brH"] | 0;

    // --- LOGIKA BACA WARNA 0x (Cara Bos) ---
    // Kita ambil sebagai String, lalu konversi pakai base 0
    if (s.containsKey("clP")) konfig.sholat[i].warnaPeringatan = (uint32_t)strtoul(s["clP"].as<const char*>(), NULL, 0);
    if (s.containsKey("clA")) konfig.sholat[i].warnaAdzan      = (uint32_t)strtoul(s["clA"].as<const char*>(), NULL, 0);
    if (s.containsKey("clH")) konfig.sholat[i].warnaHemat      = (uint32_t)strtoul(s["clH"].as<const char*>(), NULL, 0);
    if (s.containsKey("clC")) konfig.sholat[i].warnaCdown      = (uint32_t)strtoul(s["clC"].as<const char*>(), NULL, 0);
  }
  
  Serial.println("📂 Sholat Config Loaded (0x Format OK).");
  return true;
}

bool saveSholatConfig(JsonVariant dataWeb) {
  // --- LANGKAH 1: SUNTIK RAM (konfig.sholat) ---
  // Kita cek apakah data yang masuk itu Objek Tunggal (punya "id") atau Array
  if (dataWeb.is<JsonObject>() && dataWeb.containsKey("id")) {
    int idx = dataWeb["id"].as<int>();
    if (idx >= 0 && idx < 6) {
      // Masukkan ke Struct RAM (konfig)
      konfig.sholat[idx].menitPeringatan  = dataWeb["mP"] | konfig.sholat[idx].menitPeringatan;
      konfig.sholat[idx].idEfekPeringatan = dataWeb["iEP"] | konfig.sholat[idx].idEfekPeringatan;
      konfig.sholat[idx].hitungMundur     = dataWeb["cDown"] | konfig.sholat[idx].hitungMundur;
      konfig.sholat[idx].idEfekAdzan      = dataWeb["iEA"] | konfig.sholat[idx].idEfekAdzan;
      konfig.sholat[idx].durasiAdzan      = dataWeb["durA"] | konfig.sholat[idx].durasiAdzan;
      konfig.sholat[idx].menitHemat       = dataWeb["mH"] | konfig.sholat[idx].menitHemat;
      konfig.sholat[idx].kecerahanHemat   = dataWeb["brH"] | konfig.sholat[idx].kecerahanHemat;

      // Warna (Ambil sebagai string, lalu konversi)
      if (dataWeb.containsKey("clP")) konfig.sholat[idx].warnaPeringatan = (uint32_t)strtoul(dataWeb["clP"].as<const char*>(), NULL, 0);
      if (dataWeb.containsKey("clA")) konfig.sholat[idx].warnaAdzan      = (uint32_t)strtoul(dataWeb["clA"].as<const char*>(), NULL, 0);
      if (dataWeb.containsKey("clH")) konfig.sholat[idx].warnaHemat      = (uint32_t)strtoul(dataWeb["clH"].as<const char*>(), NULL, 0);
      if (dataWeb.containsKey("clC")) konfig.sholat[idx].warnaCdown      = (uint32_t)strtoul(dataWeb["clC"].as<const char*>(), NULL, 0);
      
      Serial.printf("✅ RAM Sholat Slot %d Diupdate\n", idx);
    }
  }

  // --- LANGKAH 2: TULIS ULANG SELURUH RAM (6 SLOT) KE FILE ---
  DynamicJsonDocument doc(3072);
  JsonArray root = doc.to<JsonArray>();

  for (int i = 0; i < 6; i++) {
    JsonObject s = root.createNestedObject();
    s["mP"]    = konfig.sholat[i].menitPeringatan;
    s["iEP"]   = konfig.sholat[i].idEfekPeringatan;
    s["cDown"] = konfig.sholat[i].hitungMundur;
    s["iEA"]   = konfig.sholat[i].idEfekAdzan;
    s["durA"]  = konfig.sholat[i].durasiAdzan;
    s["mH"]    = konfig.sholat[i].menitHemat;
    s["brH"]   = konfig.sholat[i].kecerahanHemat;

    // Simpan dalam format 0x agar LoadSholatConfig tidak error
    char cP[12], cA[12], cH[12], cC[12];
    sprintf(cP, "0x%06X", konfig.sholat[i].warnaPeringatan);
    sprintf(cA, "0x%06X", konfig.sholat[i].warnaAdzan);
    sprintf(cH, "0x%06X", konfig.sholat[i].warnaHemat);
    sprintf(cC, "0x%06X", konfig.sholat[i].warnaCdown);
    s["clP"] = cP; s["clA"] = cA; s["clH"] = cH; s["clC"] = cC;
  }

  File f = LittleFS.open("/sholat.json", "w");
  if (!f) return false;
  serializeJson(root, f); 
  f.close();

  Serial.println("DEBUG: File /sholat.json berhasil disimpan (6 Slot Utuh)");
  return true;
}
// ---------- TANGGAL ----------
bool loadTanggalConfig() {
  // ... (buka file dan deserializeJson) ...
   if (!LittleFS.exists("/tanggal.json")) {
    Serial.println("⚙️ Membuat Default /tanggal.json...");
    
    saveTanggalConfig();
  }

  DynamicJsonDocument doc(512);
  if (!openJson("/tanggal.json", doc)) {
    return false;
  }

  // 1. Ambil objek pembungkusnya dulu
  JsonObject tgl = doc["tanggal"];

  // 2. Ambil isinya satu per satu sesuai Key di JSON Bos
  konfig.tanggal.aktif    = tgl["a"] | false;
  konfig.tanggal.setiap   = tgl["dtke"] | 60; // dtke di JSON Bos
  konfig.tanggal.lama     = tgl["lma"] | 10;  // lma di JSON Bos
  konfig.tanggal.format   = tgl["ft"] | 0;
  konfig.tanggal.adaEvent = tgl["ev"] | false;
  konfig.tanggal.idEfek   = tgl["id"] | 0;
  konfig.tanggal.modeDP   = tgl["dp"] | 0;

  // 3. Ambil Warna (clAg, clNr, clLb)
  const char* clAg = tgl["clAg"] | "0x00FFFF";
  const char* clNr = tgl["clNr"] | "0xFFFFFF";
  const char* clLb = tgl["clLb"] | "0xFF0000";

  konfig.tanggal.warnaAgenda = strtoul(clAg, NULL, 16);
  konfig.tanggal.warnaNormal = strtoul(clNr, NULL, 16);
  konfig.tanggal.warnaLibur  = strtoul(clLb, NULL, 16);

  return true;
}

bool saveTanggalConfig() {
  DynamicJsonDocument doc(1024);
  JsonObject tgl = doc.createNestedObject("tanggal");

  tgl["a"]   = konfig.tanggal.aktif;
  tgl["dtke"]  = konfig.tanggal.setiap;
  tgl["lma"]  = konfig.tanggal.lama;
  tgl["ft"]  = konfig.tanggal.format;
  tgl["id"]  = konfig.tanggal.idEfek;
  tgl["ev"]  = konfig.tanggal.adaEvent;
  tgl["dp"]  = konfig.tanggal.modeDP;

  // Konvertli warna ke Hex 0x untuk JS Bos
  char hexAg[11], hexNr[11], hexLb[11];
  sprintf(hexAg, "0x%06X", konfig.tanggal.warnaAgenda);
  sprintf(hexNr, "0x%06X", konfig.tanggal.warnaNormal);
  sprintf(hexLb, "0x%06X", konfig.tanggal.warnaLibur);

  tgl["clAg"] = hexAg;
  tgl["clNr"] = hexNr;
  tgl["clLb"] = hexLb;

  return writeJson("/tanggal.json", doc);
}

// ---------- EFEK ----------
bool loadEfekConfig() {
  if (!LittleFS.exists("/efek.json")) {
     Serial.println("⚙️ Membuat Default /efek.json...");
     
      saveEfekConfig();
  }

  DynamicJsonDocument doc(4096); // Saya naikkan sedikit kapasitasnya karena ada data warna tambahan
  if (!openJson("/efek.json", doc)) {
    return false;
  }
  for (int i = 0; i < 15; i++) konfig.daftarEfek[i].id = 0;
  JsonArray a = doc.as<JsonArray>();
  int i = 0;
  for (JsonObject e : a) {
    if (i >= 15) break;
    konfig.daftarEfek[i].id = e["id"] | 0;
    konfig.daftarEfek[i].mode = e["m"] | 0;
    konfig.daftarEfek[i].kecepatan = e["s"] | 0;
    konfig.daftarEfek[i].kecerahan = e["b"] | 0;
    konfig.daftarEfek[i].pakaiDP = e["dp"] | false;
    
    const char* warnaHex = e["c"] | "0xFFFFFF"; 
    konfig.daftarEfek[i].warnaAni = strtoul(warnaHex, NULL, 16); 
    
    JsonArray p = e["p"].as<JsonArray>();
    int f = 0;
    for (uint8_t v : p) {
      if (f >= 8) break;
      konfig.daftarEfek[i].polaSegmen[f++] = v;
    }
    konfig.daftarEfek[i].jumlahFrame = f;
    i++;
  }
  Serial.println("📂 Efek Config Loaded.");
  return true;
}
// Tambahkan parameter JsonObject untuk menangkap data dari Web
bool saveEfekConfig(JsonVariant dataWeb) {
  // --- LANGKAH 1: UPDATE DATA DI RAM (Suntik Data) ---
  // Kita cek apakah data dari web punya "id"
  if (!dataWeb.isNull() && dataWeb.containsKey("id")) {
    int idEfek = dataWeb["id"]; 
    int idx = idEfek - 1; // Ubah ID 1-15 menjadi Index 0-14

    if (idx >= 0 && idx < 15) {
      // Hanya update index yang dikirim dari Web ke dalam RAM ESP32
      konfig.daftarEfek[idx].id = idEfek;
      konfig.daftarEfek[idx].mode = dataWeb["m"] | 1;
      konfig.daftarEfek[idx].kecepatan = dataWeb["s"] | 150;
      konfig.daftarEfek[idx].kecerahan = dataWeb["b"] | 200;
      konfig.daftarEfek[idx].pakaiDP = dataWeb["dp"] | 0;
      
      const char* hexWarna = dataWeb["c"] | "0xFFFFFF";
      konfig.daftarEfek[idx].warnaAni = strtoul(hexWarna, NULL, 0);
      
      JsonArray p = dataWeb["p"];
      if (!p.isNull()) {
        konfig.daftarEfek[idx].jumlahFrame = p.size();
        for (int f = 0; f < p.size(); f++) {
          konfig.daftarEfek[idx].polaSegmen[f] = p[f];
        }
      }
      Serial.printf("DEBUG: Data RAM Slot %d Diperbarui\n", idEfek);
    }
  }

  // --- LANGKAH 2: TULIS ULANG SELURUH RAM KE FILE (15 Slot) ---
  // Kita buat dokumen baru yang bersih
  DynamicJsonDocument doc(5120); 
  JsonArray root = doc.to<JsonArray>(); // PAKSA JADI ARRAY [ ]

  for (int i = 0; i < 15; i++) {
    // Ambil data dari RAM (baik data lama maupun data yang baru diupdate tadi)
    JsonObject e = root.createNestedObject();
    
    // Pastikan ID tidak 0
    int idFix = (konfig.daftarEfek[i].id > 0) ? konfig.daftarEfek[i].id : (i + 1);
    
    e["id"] = idFix;
    e["m"]  = konfig.daftarEfek[i].mode;
    e["s"]  = konfig.daftarEfek[i].kecepatan;
    e["b"]  = konfig.daftarEfek[i].kecerahan;
    e["dp"] = konfig.daftarEfek[i].pakaiDP;
    
    char hexStr[12];
    sprintf(hexStr, "0x%06X", konfig.daftarEfek[i].warnaAni);
    e["c"] = hexStr;
    
    JsonArray pArray = e.createNestedArray("p");
    for (int f = 0; f < konfig.daftarEfek[i].jumlahFrame; f++) {
      pArray.add(konfig.daftarEfek[i].polaSegmen[f]);
    }
  }

  // --- LANGKAH 3: SIMPAN KE LITTLEFS ---
  File f = LittleFS.open("/efek.json", "w");
  if (!f) return false;
  serializeJson(root, f); // Menulis array [ {}, {}, ... ]
  f.close();

  Serial.println("DEBUG: File /efek.json berhasil disimpan (15 Slot Utuh)");
  return true;
}