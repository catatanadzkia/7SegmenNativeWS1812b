
#include "webHandler.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "AsyncJson.h"
#include <Preferences.h>
#include <FastLED.h>
#include "configSystem.h"
#include "config_manajer.h"
#include "cuaca.h"


extern AsyncWebServer server;

#define MAX_DIGIT 12

extern String lastSyncTime;
extern bool modeCekJadwal;
extern unsigned long lastChangeTime;
extern int SimpanOtomatis;
extern bool perluSimpan;
extern bool scanOn;
extern uint8_t mainBuffer[];
extern CRGB warna[MAX_DIGIT];

// Deklarasi fungsi eksternal agar bisa dipanggil di sini
extern String getUptime();
extern void syncJadwalSholat();
extern bool muatJadwal();
extern void setKecerahan(int val);
extern void tampilkanKeSegmen(uint8_t *buf, CRGB* warnaS);
extern void updateCuacaBMKG();
extern void buatBufferSkor(uint8_t *buf);

static void setupPageHandlers();
static void setupWifiHandlers();
static void setupStatusHandlers();
static void setupConfigHandlers();
static void setupCommandHandlers();

static String fName = "";



void setupServer() {
 
  setupPageHandlers();
  setupWifiHandlers();
  setupStatusHandlers();
  setupConfigHandlers();
  setupCommandHandlers();


  server.begin();
}

//---SetUp index page Handler----

static void setupPageHandlers() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.serveStatic("/", LittleFS, "/");
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "Error: index.html not found");
    }
  });

  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Cek Login
    if (!request->authenticate(konfig.authUser, konfig.authPass)) {
      return request->requestAuthentication();
    }

    if (LittleFS.exists("/upload.html")) {
      request->send(LittleFS, "/upload.html", "text/html");
    } else {
      request->send(404, "text/plain", "Error: upload.html not found");
    }
  });

  // --------------------------------------------------
  // 3. API: LIST FILES (JSON)
  // URL: /list-files
  // --------------------------------------------------
  server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(konfig.authUser, konfig.authPass)) return request->requestAuthentication();

    File root = LittleFS.open("/");
    String json = "[";
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (json != "[") json += ",";
        json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
        file = root.openNextFile();
      }
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // --------------------------------------------------
  // 4. API: DELETE FILE
  // URL: /delete?file=/namafile.txt
  // --------------------------------------------------
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(konfig.authUser, konfig.authPass)) return request->requestAuthentication();

    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
        request->send(200, "text/plain", "File deleted");
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Missing param: file");
    }
  });

  // --------------------------------------------------
  // 5. PROSES UPLOAD FILE (POST)
  // URL: /upload (Method: POST)
  // --------------------------------------------------
  server.on(
    "/upload", HTTP_POST,
    // (A) Bagian Respon Selesai
    [](AsyncWebServerRequest *request) {
      String pesan = "Berhasil upload: " + fName;
      request->send(200, "text/plain", pesan);
    },
    // (B) Bagian Stream Data (Proses Fisik File)
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      // Cek Keamanan
      if (!request->authenticate(konfig.authUser, konfig.authPass)) return;

      // -- Awal Upload (Paket Pertama) --
      if (index == 0) {
        // Format nama file
        if (!filename.startsWith("/")) filename = "/" + filename;

        // Simpan ke variabel global
        fName = filename;

        // Hapus file lama & Buka file baru
        if (LittleFS.exists(fName)) LittleFS.remove(fName);
        request->_tempFile = LittleFS.open(fName, "w");

        Serial.printf("[Upload Start] %s\n", fName.c_str());
      }

      // -- Tulis Data --
      if (request->_tempFile) {
        request->_tempFile.write(data, len);
      }

      // -- Akhir Upload (Tutup & Reload) --
      if (final) {
        if (request->_tempFile) {
          request->_tempFile.close();
          Serial.printf("[Upload Selesai] Ukuran: %u bytes\n", index + len);

          // LOGIKA AUTO RELOAD CONFIG
          // Ambil nama dari fName ("/sholat.json" -> "sholat")
          String target = fName;
          target.replace("/", "");
          target.replace(".json", "");

          // Coba Reload
          if (reloadConfigByTarget(target)) {
            Serial.printf("[Auto-Reload] Config '%s' berhasil diperbarui di RAM!\n", target.c_str());
          }
        }
      }
    });

  // --------------------------------------------------
  // 6. NOT FOUND HANDLER (404)
  // --------------------------------------------------
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "text/plain", "Halaman tidak ditemukan (404)");
    }
  });
}

//---SetUp WIFI Handler----
static void setupWifiHandlers() {

  // Endpoint Tunggal: /wifi
  server.on("/wifi", HTTP_ANY, [](AsyncWebServerRequest *request) {
    // 2. Cek Parameter "mode"
    if (!request->hasParam("mode")) {
      request->send(400, "text/plain", "Error: Missing mode parameter");
      return;
    }

    String mode = request->getParam("mode")->value();
    if (mode == "scan") {
      
      // Cek status scan terakhir
      int n = WiFi.scanComplete();

      if (n == -2) { 
        // -2 Artinya: Belum mulai scan.
        // Yuk mulai scan mode ASYNC (true) biar webserver gak macet
        WiFi.scanNetworks(true); 
        
        // Kasih tau browser scan baru dimulai
        request->send(202, "application/json", "[]"); 
      } 
      else if (n == -1) {
        // -1 Artinya: Masih proses scanning...
        // Kasih tau browser suruh nunggu/coba lagi nanti
        request->send(202, "application/json", "[]");
      } 
      else {
        // >= 0 Artinya: Scan SELESAI! Hasilnya ada n network
        String json = "[";
        for (int i = 0; i < n; ++i) {
          if (i) json += ",";
          json += "{";
          json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
          json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
          // Enkripsi: Open, WEP, WPA, dll
          json += "\"enc\":\"" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured") + "\"";
          json += "}";
        }
        json += "]";
        
        request->send(200, "application/json", json);
        
        // PENTING: Hapus hasil scan dari memori biar gak numpuk
        WiFi.scanDelete(); 
      }
    }
    else if (mode == "save") {
      if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {

        String newSSID = request->getParam("ssid", true)->value();
        String newPass = request->getParam("pass", true)->value();

        // --- SIMPAN KE PREFERENCES ---
        Preferences pref;
        if (pref.begin("akses", false)) {

          pref.putString("st_ssid", newSSID);
          pref.putString("st_pass", newPass);
          pref.end();  // Tutup preferences
          Serial.println(newSSID);
          Serial.println(newPass);

          request->send(200, "text/plain", "WiFi Disimpan ke NVS. Restarting...");

          delay(2000);
          ESP.restart();  // Restart agar konek ke WiFi baru

        } else {
          request->send(500, "text/plain", "Gagal membuka Preferences");
        }
        // -----------------------------

      } else {
        request->send(400, "text/plain", "Data SSID/Pass kurang");
      }
    }

    else if (mode == "status") {
      String status = (WiFi.status() == WL_CONNECTED) ? "1" : "0";
      request->send(200, "text/plain", status);
    }

    // D. Lainnya
    else {
      request->send(400, "text/plain", "Mode tidak dikenal");
    }
  });
}
//---SetUp Status ESP----
static void setupStatusHandlers() {

 server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    
    float rawTemp = temperatureRead(); 
    
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":\"" + getUptime() + "\",";
    json += "\"temp\":" + String(rawTemp, 1) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"fs_used\":" + String(LittleFS.usedBytes()) + ",";
    json += "\"fs_total\":" + String(LittleFS.totalBytes());    
    json += "}";

    request->send(200, "application/json", json);
  });
  server.on("/lastsync", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Kirim teks isi variabel lastSyncTime yang tadi diisi di strftime
    request->send(200, "text/plain", lastSyncTime);
  });

}

//---Save and Load----
static void setupConfigHandlers() {

  server.on("/load", HTTP_GET, [](AsyncWebServerRequest *request) {
    
    // 1. Cek parameter "set"
    if (request->hasParam("set")) {
      String tNmae = request->getParam("set")->value(); // Contoh: "ultra"
      String fsPath;

      // 2. Dapatkan path file (misal: "/ultra.json")
      if (getConfigPath(tNmae, fsPath)) {
        
        if (LittleFS.exists(fsPath)) {
          // Kirim file JSON langsung
          request->send(LittleFS, fsPath, "application/json");
        } else {
          // File belum ada, kirim object kosong
          request->send(200, "application/json", "{}");
        }
        
      } else {
        // Jika target 'set' tidak dikenal
        request->send(400, "application/json", "{\"error\":\"Config name (set) not found\"}");
      }
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing parameter: set\"}");
    }
  });
  
  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/save", 
    [](AsyncWebServerRequest *request, JsonVariant &json) {
    
    // 1. Cek parameter "set"
    if (!request->hasParam("set")) {
      request->send(400, "application/json", "{\"msg\":\"Missing parameter: set\"}");
      return;
    }

    String tNmae = request->getParam("set")->value();
    String fsPath;

    // 2. Validasi target & ambil path
    if (!getConfigPath(tNmae, fsPath)) {
      request->send(400, "application/json", "{\"msg\":\"Invalid config name\"}");
      return;
    }

    // 3. Tulis JSON ke File (FIXED: Selalu Overwrite "w")
    // Hapus logika 'index == 0', langsung paksa mode "w"
    File file = LittleFS.open(fsPath, "w"); 
    
    if (file) {
      // serializeJson otomatis menulis data baru ke file kosong (karena mode w)
      serializeJson(json, file);
      file.close();
      
      // 4. RELOAD RAM
      if (reloadConfigByTarget(tNmae)) {
         Serial.printf("[Config] '%s' Saved & Reloaded.\n", tNmae.c_str());
         request->send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Saved & Reloaded\"}");
      } else {
         Serial.printf("[Config] '%s' Saved but Reload FAILED.\n", tNmae.c_str());
         request->send(200, "application/json", "{\"status\":\"warning\",\"msg\":\"Saved but Reload Failed\"}");
      }

    } else {
      Serial.println("[Config] Write Error");
      request->send(500, "application/json", "{\"status\":\"error\",\"msg\":\"Filesystem Write Error\"}");
    }
  });

  server.addHandler(handler);
}

static void setupCommandHandlers() {

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    String act = request->arg("act");
    String valStr = request->arg("v");
    int val = valStr.toInt();

    if (act == "setBr") {
      konfig.kecerahanGlobal = val;
      setKecerahan(val);
      SimpanOtomatis = 1;
    } else if (act == "restart") {
      request->send(200, "text/plain", "Restarting...");
      delay(500);
      ESP.restart();
      return;
    }else if (act == "sinkron") {
      perluSimpan = true;
      request->send(200, "text/plain", "Jadwal Berhasil Diperbarui, Bos!");
    } else if (act == "addSkorA") {
      konfig.papanSkor.skorA++;
      konfig.papanSkor.aktif = true;
      buatBufferSkor(mainBuffer);
      tampilkanKeSegmen(mainBuffer, warna);
    } else if (act == "addSkorB") {
      konfig.papanSkor.skorB++;
      konfig.papanSkor.aktif = true;
      buatBufferSkor(mainBuffer);
      tampilkanKeSegmen(mainBuffer, warna);
    } else if (act == "skor") {
      konfig.papanSkor.skorA = request->arg("a").toInt();
      konfig.papanSkor.skorB = request->arg("b").toInt();
      konfig.papanSkor.babak = request->arg("bk").toInt();
      if (request->hasArg("s")) konfig.papanSkor.aktif = (request->arg("s") == "1");
      else konfig.papanSkor.aktif = true;
      buatBufferSkor(mainBuffer);
      tampilkanKeSegmen(mainBuffer, warna);
    } else if (act == "setUltra") {
      konfig.pengusirHama.aktif = (val == 1);
      SimpanOtomatis = 2;
    } else if (act == "titik2") {
      konfig.titikDuaKedip = (val == 1);
      SimpanOtomatis = 1;
    } else if (act == "viewEfek") { // Jika (act ISINYA ADALAH "viewEfek")
      konfig.modeAnimasi.aktif = true;
      konfig.modeAnimasi.idEfek = val;
    } else if (act == "stopEfek") { // Jika (act ISINYA ADALAH "stopEfek")
      konfig.modeAnimasi.aktif = false;
    } else if (act == "cekJadwal") {
      modeCekJadwal = true;
    } else if (act == "showJam") {
      modeCekJadwal = false;
      konfig.modeEfekAktif = false;
      konfig.papanSkor.aktif = false;
    } else if (act == "animasi") {
      // Ambil nilai 'aktif' (1 atau 0) dan 'id' (0-14)
      konfig.modeAnimasi.aktif = request->arg("aktif").toInt();
      konfig.modeAnimasi.idEfek = request->arg("id").toInt();
    } else if(act == "statuscuaca"){
      cuaca.status.aktif = (val == 1);
      konfig.visjam.animasiAktif = false;
      SimpanOtomatis = 4;
    } else if (act == "updateNow") {
       updateCuacaBMKG(); // Langsung panggil fungsi ambil data ke BMKG tanpa nunggu jam
       request->send(200, "text/plain", "Update Berhasil Bos!");

    }else if (act == "jamAnim") { 
      // Ambil nilai: v=1 (ON), v=0 (OFF)
      konfig.visjam.animasiAktif = (val == 1);
      
      // Ambil ID animasi jika ada (misal dari parameter 'id')
      if (request->hasArg("id")) {
        konfig.visjam.idAnimasi = request->arg("id").toInt();
      }
      
      // Langsung simpan ke LittleFS agar tidak hilang saat mati lampu
      SimpanOtomatis = 3;
      cuaca.status.aktif = false;
      // Flag agar looping utama langsung update ke visual jam
      konfig.modeAnimasi.aktif = false; // Matikan mode "View Efek" global agar jam tampil
      konfig.papanSkor.aktif = false;   // Matikan skor agar jam tampil
      modeCekJadwal = false;            // Matikan cek jadwal
    }

    lastChangeTime = millis();
    
    
    request->send(200, "text/plain", "OK");
  });
}