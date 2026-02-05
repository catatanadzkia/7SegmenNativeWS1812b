#include "cuaca.h"
//#include <Firebase_ESP_Client.h> 
#include <ArduinoJson.h>
#include <LittleFS.h>

// Objek internal untuk Firebase
extern FirebaseData fbdo;
extern FirebaseConfig config;
extern FirebaseAuth auth;
cuacaKonfig cuaca;

extern int SimpanOtomatis;
extern unsigned long lastChangeTime;
unsigned long lastCekCuaca = 0;
int intervalCek = 30 * 60 * 1000;

extern struct tm timeinfo;


void handleCuaca() {
  unsigned long now = millis();
   
    int menitSekarang = timeinfo.tm_min; 
    if (menitSekarang <= 5) {
        intervalCek = 60 * 1000; 
    } else {
        intervalCek = 15 * 60 * 1000; 
    }

    if (now - lastCekCuaca >= intervalCek) {
        lastCekCuaca = now;
        updateCuacaBMKG();
    }
}

void setupCuaca() {

  cuaca.status.kondisi = "Memulai...";
  cuaca.status.aktif = false;
  cuaca.status.kodeCuaca = 0;

  // Konfigurasi Database Bos
  config.database_url = "smart7s-default-rtdb.asia-southeast1.firebasedatabase.app";
  config.signer.test_mode = true; 
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

uint32_t tentukanWarnaCuaca(int kode) {

  switch (kode) {
    case 0:   case 100: return 0x00BFFF; break; // Cerah (Deep Sky Blue)
    case 1:   case 101: return 0x1E90FF; break; // Cerah Berawan (Dodger Blue - Lebih kontras)
    case 2:   case 102: return 0x404040; break; // Berawan (Putih redup tipis)
    case 3:             return 0x080815; break; // Deep Midnight Grey
    case 4:   case 5:   return 0x202020; break; // Kabut (Remang-remang)
    case 60:  case 61:  return 0x0000FF; break; // Hujan (Biru murni biar tegas)
    case 95:  case 97:  return 0x4B0082; break; // Petir (Indigo/Ungu Tua)
    default:            return 0x00BFFF; break;
  }
}

void updateCuacaBMKG() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  //Serial.println(F("📡 Mengambil data dari path /cuaca..."));

  Serial.println(F("🔍 Mengecek data cuaca ke Firebase..."));

    // Ambil JSON mentah dari path /cuaca
    if (Firebase.RTDB.getJSON(&fbdo, "/cuaca")) {
        String playload = fbdo.jsonString();
        DynamicJsonDocument doc(512); // Gunakan Static, jangan Dynamic!
        DeserializationError error = deserializeJson(doc, playload);

        if (!error) {
            cuaca.status.kodeCuaca = doc["kode"] | 0;
            cuaca.status.kondisi = doc["desc"].as<String>();
            cuaca.status.jam = doc["jam"].as<String>();
            
            saveCuaca(); // Simpan ke LittleFS
            Serial.println(F("✅ Cuaca Berhasil Diupdate"));
        }else {
          Serial.printf("❌ Gagal Firebase: %s\n", fbdo.errorReason().c_str());
        }
      }
  fbdo.clear();
}
bool saveCuaca(){
  // Di ArduinoJson 7, cukup tulis tanpa parameter ukuran
  DynamicJsonDocument saveDoc(512);
  JsonObject s = saveDoc["status"];
  s["aktif"] = cuaca.status.aktif; 
  s["kode"]  = cuaca.status.kodeCuaca;
  s["ket"]   = cuaca.status.kondisi.c_str(); // Konsisten menggunakan "ket"
  s["jam"]   = cuaca.status.jam.c_str();

  //siimpanstructwarna
  JsonObject w = saveDoc["warna"];
  w["cerah"] = cuaca.warna.cerah;
  w["cerahBerawan"]=cuaca.warna.cerahBerawan;
  w["berawan"] = cuaca.warna.berawan;
  w["mendung"] = cuaca.warna.mendung;
  w["kabut"] = cuaca.warna.kabut;
  w["hujan"] = cuaca.warna.hujan;
  w["petir"] = cuaca.warna.petir;

  File file = LittleFS.open("/cuaca.json", "w");
  if (!file) {
    return false;
  }

  // Menulis langsung ke file
  serializeJson(saveDoc, file);
  file.close();
  
  return true;
}

bool muatCuaca() {
  // Pastikan file ada sebelum dibuka
  if (!LittleFS.exists("/cuaca.json")) {
    Serial.println(F("⚠️ File cuaca.json tidak ditemukan, menggunakan default."));
    return false;
  }

  File file = LittleFS.open("/cuaca.json", "r");
  if (!file) return false;

  // Di ArduinoJson 7, cukup JsonDocument tanpa angka ukuran
  JsonDocument loadDoc; 

  // Deserialize data dari file
  DeserializationError error = deserializeJson(loadDoc, file);
  file.close();

  if (error) {
    Serial.print(F("❌ Gagal baca JSON: "));
    Serial.println(error.c_str());
    return false;
  }

  // 1. Muat data Status
  JsonObject s = loadDoc["status"];
  cuaca.status.aktif     = s["aktif"] | false;
  cuaca.status.kodeCuaca = s["kode"]  | 0;
  cuaca.status.kondisi   = s["ket"]   | "Cerah";
  cuaca.status.jam       = s["jam"]   | "00:00";

  // 2. Muat data Warna (Konfigurasi)
  JsonObject w = loadDoc["warna"];
  cuaca.warna.cerah         = w["cerah"]         | 0x00BFFF;
  cuaca.warna.cerahBerawan  = w["cerahBerawan"]  | 0x1E90FF;
  cuaca.warna.berawan       = w["berawan"]       | 0x404040;
  cuaca.warna.mendung       = w["mendung"]       | 0x080815;
  cuaca.warna.kabut         = w["kabut"]         | 0x202020;
  cuaca.warna.hujan         = w["hujan"]         | 0x0000FF;
  cuaca.warna.petir         = w["petir"]         | 0x4B0082;

  Serial.println(F("✅ Konfigurasi cuaca dimuat dari LittleFS"));
  return true;
}

void handleSimpan(){

  if (SimpanOtomatis > 0 && (millis() - lastChangeTime > 10000)) {
    
    if (SimpanOtomatis == 1) saveSystemConfig();
    else if (SimpanOtomatis == 2) saveHama();
    else if (SimpanOtomatis == 3) saveJam();
    else if (SimpanOtomatis == 4) saveCuaca();

    // Setelah selesai, kembalikan ke 0 (Idle)
    SimpanOtomatis = 0; 
  }
}