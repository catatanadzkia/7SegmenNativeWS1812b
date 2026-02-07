#include <Arduino.h>
#include "esp_err.h"
#include <FastLED.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "AsyncJson.h" 
#include <ArduinoOTA.h>
#include "time.h"
#include "configSystem.h"
#include "config_manajer.h"
#include "displayLogic.h"
#include "systemLogic.h"
#include "cuaca.h"
#include "efek_visual.h"
#include "webHandler.h"

#define FASTLED_ESP32_I2S true // Kunci agar Web Server & Ultrasonic tetap lancar
#define PIN_D1 22
#define PIN_D2 18
#define PIN_D3 19
#define PIN_D4 21




// --- TAMBAHKAN DI BAGIAN ATAS (PINOUT BARU) ---
#define PIN_DATA_ULTRA  (gpio_num_t)14
#define PIN_LATCH_ULTRA (gpio_num_t)12
#define PIN_CLOCK_ULTRA (gpio_num_t)13

#define LT_ULTRA (gpio_num_t)PIN_LATCH_ULTRA
#define DT_ULTRA (gpio_num_t)PIN_DATA_ULTRA
#define CL_ULTRA (gpio_num_t)PIN_CLOCK_ULTRA


Preferences pref;
TaskHandle_t TaskUltra;
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

int jadwalMenit[6];
String nmAgnd = "";
unsigned long lastUltraPulse = 0;
bool ultraStatus = false;
unsigned long lastChangeTime = 0;
int SimpanOtomatis = 0;
bool perluSimpan = false;
bool modeCekJadwal = false;        // <--- Tambahkan ini Bos
unsigned long timerCekJadwal = 0;  // Untuk hitung mundur otomatis mati
int detikSekarang;                 // Deklarasikan agar bisa dipakai di loop dan cekDanTampilkan
String lastSyncTime = "Belum Sinkron";
unsigned long lastCekWiFi = 0;

bool mdnsOn = false;
bool scanOn = false;
bool wifiConnected = false;
unsigned long wifiRetryTimer = 0;
bool isLiburNasional = false;
String teksEventHariIni = ""; 



AsyncWebServer server(80);


// Fungsi pembantu Uptime
String getUptime() {
  unsigned long sec = millis() / 1000;
  int jam = sec / 3600;
  int sisa = sec % 3600;
  int menit = sisa / 60;
  int detik = sisa % 60;
  return String(jam) + "j " + String(menit) + "m " + String(detik) + "d";
}

// ===MEMUAT SUARA ULTRASONIC===
void UltraTaskCode(void *pvParameters) {
  // Gunakan variabel lokal agar lebih cepat diakses CPU
  static uint64_t lastPulse = 0;
  static bool status = false;
  
  for (;;) {
    if (konfig.pengusirHama.aktif) {
      // 1. Logika Waktu (Cek tiap 1 detik saja, jangan tiap loop biar hemat CPU)
      static uint32_t lastTimeCheck = 0;
      static bool bolehNyala = true;
      if (millis() - lastTimeCheck > 1000) {
          int jamSekarang = timeinfo.tm_hour;
          bool isMalam = (jamSekarang >= 18 || jamSekarang < 4);
          bolehNyala = !(konfig.pengusirHama.aktifMalamSaja && !isMalam);
          lastTimeCheck = millis();
      }

      if (bolehNyala) {
        static uint32_t lastChange = 0;
        static uint32_t currentFreq = 22000;

        // 2. Ganti Frekuensi
        if (millis() - lastChange >= (uint32_t)konfig.pengusirHama.intervalAcak) {
          lastChange = millis();
          currentFreq = random(konfig.pengusirHama.frekuensiMin, konfig.pengusirHama.frekuensiMax + 1);
          if (currentFreq < 100) currentFreq = 22000;
        }

        // 3. Generator Pulsa (Pake cara Native biar gak bentrok Brightness)
        uint64_t now = esp_timer_get_time(); // Lebih akurat dari micros()
        uint64_t halfPeriod = 500000ULL / currentFreq;

        if (now - lastPulse >= halfPeriod) {
          lastPulse = now;
          status = !status;

          // GANTI DENGAN CARA NATIVE (Cepat & Anti-Bentrok)
          gpio_set_level(LT_ULTRA, 0);
          
          uint8_t dataKirim = status ? 0x01 : 0x00;
          for (int i = 7; i >= 0; i--) {
              gpio_set_level(CL_ULTRA, 0);
              gpio_set_level(DT_ULTRA, (dataKirim >> i) & 0x01);
              gpio_set_level(CL_ULTRA, 1);
          }
          gpio_set_level(LT_ULTRA, 1);
        }
      }
    }

    // 4. KUNCI SUKSES: Kasih napas buat Core 0 (Web Server/Brightness)
    // Jangan vTaskDelay(1), tapi kasih yield kecil
    vTaskDelay(1); 
  }
}
void setKecerahan(int persen) {
    // 1. Batasi input dari Web agar tetap 0-100%
    if (persen > 100) persen = 100;
    if (persen < 0) persen = 0;

    // 2. Simpan nilai persen (0-100) ke struct biar di Web tampilannya tetap 100
    konfig.kecerahanGlobal = persen; 

    // 3. KONVERSI: Ubah 0-100 menjadi 0-255 untuk FastLED
    // Rumus: (persen * 255) / 100
    int nilaiAsli = map(persen, 0, 100, 0, 255);

    // 4. Kirim ke Hardware
    FastLED.setBrightness(nilaiAsli);  
}
void inisialisasiHardware();
void setupWiFi();
void setupServer();
void syncJadwalSholat();
bool muatJadwal();
void cekDanTampilkan();
void WiFiEvent(arduino_event_id_t event);
void displayLoadingSinyal(uint32_t speed);



void setup() {
  Serial.begin(115200);
  delay(1000);


  // 1. Mulai LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Gagal Mount LittleFS!");
    return;
  }
  loadWifi();
  // 2. MUAT CONFIG DULU (PENTING!)
  // --- SYSTEM ----
  loadSystemConfig();
  // --- HAMA ---
  loadHama();
  // --- SHOLAT ---
  loadSholatConfig();
  // --- TANGGAL ---
  loadTanggalConfig();
  // --- EFEK ----
  loadEfekConfig();
  loadSkor();
  loadJam();
  

  // 3. Inisialisasi Hardware & WiFi
  inisialisasiHardware();
  setKecerahan(50);
  
  setupWiFi();
  
  setupCuaca();
  

  // 4. Sinkronisasi Waktu NTP
  configTime(konfig.timezone * 3600, 0, "id.pool.ntp.org", "time.google.com", "pool.ntp.org");

  // 5. CEK SYNC PERDANA (Setelah Config & WiFi siap)

  delay(1000);
  if (!LittleFS.exists("/jadwal.json") || strlen(konfig.terakhirSync) < 5) {
    Serial.println("- Data kosong/awal, mencoba sinkronisasi perdana...");
    syncJadwalSholat();
  }

  // 6. Muat Jadwal ke RAM
  muatJadwal();
  muatCuaca();

  ArduinoOTA.setHostname(konfig.namaPerangkat);
  ArduinoOTA.begin();

  xTaskCreatePinnedToCore(UltraTaskCode, "TaskUltra", 10000, NULL, 1, &TaskUltra, 0);
  setupServer();

  Serial.println("Sistem Siap!");
}

void loop() {
  ArduinoOTA.handle();
  handleSimpan();
  handleCuaca();


  // --- TUGAS LAMBAT (Setiap 1 Detik) ---
  if (millis() - lastUpdateJam >= 1000) {
    lastUpdateJam = millis();
    if (getLocalTime(&timeinfo)) {
      detikSekarang = timeinfo.tm_sec;  // Update di sini sudah cukup
      if (timeinfo.tm_hour == 1 && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
        syncJadwalSholat();        
      }
    }

    if (perluSimpan){
      syncJadwalSholat();
      perluSimpan = false;
    }
  }

  // --- TUGAS REFRESH (Visual - Diatur Kecepatannya) ---
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 33) {  // 33ms = Sekitar 30 FPS (Standar mata manusia)
    lastRefresh = millis();

    // 1. Olah data (Hanya isi buffer, jangan kirim hardware di dalam sini)
    cekDanTampilkan();

    // 2. Kirim ke hardware (Satu-satunya pintu keluar data)
    display();
  }
}


//=== Handle Goodle Appscript===
void syncJadwalSholat() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  Serial.println("- Menghubungi Google (Mode Raw Text)...");

  // 1. Mulai koneksi
  if (http.begin(client, konfig.urlGAS)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    // 2. INI KUNCI AGAR DATA TIDAK KOSONG:
    // Kita minta Google jangan pakai GZIP atau kompresi lainnya
    http.addHeader("Accept-Encoding", "identity"); 
    http.addHeader("User-Agent", "ESP32-JadwalSholat");

    int httpCode = http.GET();
    Serial.printf("- Respon Code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      // Ambil string mentah
      String payload = http.getString();
      
      // 3. Validasi: pastikan isinya benar-benar JSON jadwal
      if (payload.indexOf("jadwal") > -1) { 
        File file = LittleFS.open("/jadwal.json", "w");
        if (file) {
        file.print(payload);
        file.close();
        Serial.println("✅ Data Disimpan.");

        // 1. Ambil waktu sekarang dari sistem ESP32
        struct tm ti;
        if (getLocalTime(&ti)) {
            char buf[32];
            // Format: Tgl/Bulan Jam:Menit (Contoh: 19/01 19:30)
            strftime(buf, sizeof(buf), "%d/%m %H:%M", &ti);
            lastSyncTime = String(buf); 
        }
          
          // Langsung panggil muatJadwal agar RAM terupdate
          muatJadwal(); 
        } else {
          Serial.println("❌ Gagal buka file LittleFS");
        }
      } else {
        Serial.println("⚠️ Data masuk tapi bukan JSON Jadwal. Isi: " + payload);
      }
    } else {
      Serial.printf("❌ HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}
bool muatJadwal() {
  if (!LittleFS.exists("/jadwal.json")) return false;

  File file = LittleFS.open("/jadwal.json", "r");
  DynamicJsonDocument doc(3072); 
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) return false;

  // --- BAGIAN JADWAL (Tetap Aman) ---
  JsonObject j = doc["jadwal"];
  auto konversi = [](const char *t) {
    if (!t) return -1;
    int h, m;
    if (sscanf(t, "%d:%d", &h, &m) == 2) return (h * 60) + m;
    return -1;
  };
  jadwalMenit[0] = konversi(j["Imsak"] | "00:00");
  jadwalMenit[1] = konversi(j["Subuh"] | "00:00");
  jadwalMenit[2] = konversi(j["Luhur"] | "00:00");
  jadwalMenit[3] = konversi(j["Ashar"] | "00:00");
  jadwalMenit[4] = konversi(j["Magrib"] | "00:00");
  jadwalMenit[5] = konversi(j["Isya"] | "00:00");

  // --- BAGIAN KALENDER (Ditambahkan Logika Warna) ---
  JsonArray kal = doc["kalender"];
  teksEventHariIni = ""; 
  
  // Reset flag pendeteksi (Pastikan variabel ini ada di global)
  konfig.tanggal.adaEvent = false;     // Untuk warna Agenda
  isLiburNasional = false;            // Untuk warna Libur

  for (JsonObject v : kal) {
    const char* nmAgnd = v["nama"];
    const char* mulai = v["mulai"];
    
    if (nmAgnd) {
      String namaStr = String(nmAgnd);
      
      // LOGIKA PENENTU WARNA (Berdasarkan isi teks)
      if (namaStr.indexOf("[PRIBADI]") >= 0) {
          konfig.tanggal.adaEvent = true; // Ketemu tanda pribadi
      } else {
          isLiburNasional = true; // Jika tidak ada [PRIBADI], berarti Nasional
      }

      // Teks untuk Running Text (Tetap sesuai aslinya)
      if (teksEventHariIni != "") teksEventHariIni += "  |  ";
      if (String(mulai) != "0:0") {
          teksEventHariIni += namaStr + " (" + String(mulai) + ")";
      } else {
          teksEventHariIni += namaStr;
      }
    }
  }

  Serial.println("✅ Muat Berhasil!");
  return true;
}

// --- DETAIL FUNGSI ---

void inisialisasiHardware() {
 // Daftarkan masing-masing pin secara terpisah (1 Pin 1 Digit)
  FastLED.addLeds<WS2812B, PIN_D1, GRB>(leds[0], LEDS_PER_DIGIT + LEDS_DP);
  FastLED.addLeds<WS2812B, PIN_D2, GRB>(leds[1], LEDS_PER_DIGIT + LEDS_DP);
  FastLED.addLeds<WS2812B, PIN_D3, GRB>(leds[2], LEDS_PER_DIGIT + LEDS_DP);
  FastLED.addLeds<WS2812B, PIN_D4, GRB>(leds[3], LEDS_PER_DIGIT + LEDS_DP);

    FastLED.setBrightness(150); // Set kecerahan awal
    // 2. Logika Native Bos untuk Ultrasonic (LT, DT, CL) tetap dipertahankan
    uint64_t pin_mask = (1ULL << LT_ULTRA) | (1ULL << DT_ULTRA) | (1ULL << CL_ULTRA);
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = pin_mask;
    gpio_config(&io_conf);

    // Set kondisi awal High agar tidak noise
    gpio_set_level(LT_ULTRA, 1);
}

// ---- SETUP WIFI (DIPANGGIL SEKALI) ----
void setupWiFi() {

  pref.begin("akses", true);
  String ssid = pref.getString("st_ssid", "");
  String pass = pref.getString("st_pass", "");
  pref.end();

  String apName = konfig.namaPerangkat;
  if (apName == "") apName = "JamBos";
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(apName.c_str());

  WiFi.setAutoReconnect(true);  // biarkan stack
  WiFi.persistent(false);

  if (ssid != "") {
    Serial.print("Mencoba Konek WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());

    // --- TAMBAHKAN LOGIKA TUNGGU DI SINI ---
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED && counter < 20) {
      
      // Jalankan animasi sinyal (misal pindah tiap 300ms)
      displayLoadingSinyal(2);
      
      delay(10); // Memberi ruang untuk proses background WiFi
      
      // Logika Serial Print tetap setiap 500ms
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print(".");
        counter++;
      }
    }
  }
}
void WiFiEvent(arduino_event_id_t event) {

  switch (event) {

    // === STA DAPAT IP ===
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:

      if (!mdnsOn) {
        if (MDNS.begin(konfig.namaPerangkat)) {
          MDNS.addService("http", "tcp", 80);
          mdnsOn = true;
        }
      }

      // AP OFF kalau nyala
      WiFi.softAPdisconnect(true);
      break;

    // === STA PUTUS ===
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:

      if (mdnsOn) {
        MDNS.end();  // ⬅️ INI WAJIB
        mdnsOn = false;
      }

      // AP ON untuk akses darurat
      WiFi.softAP(konfig.namaPerangkat, "12345678");
      break;

    default:
      break;
  }
}
void cekDanTampilkan() {
  setKecerahan(konfig.kecerahanGlobal);

  // 1. PRIORITAS UTAMA: MODE CEK JADWAL
  if (modeCekJadwal) {
    int durasiPerSholat = konfig.tanggal.lama * 1000;
    if (durasiPerSholat < 1000) durasiPerSholat = 2000;
    int indexJadwal = (millis() / durasiPerSholat) % 6;
    int h = jadwalMenit[indexJadwal] / 60;
    int m = jadwalMenit[indexJadwal] % 60;
    buatBufferWaktu(h, m, 0, mainBuffer, true);
    pakaiWarna(konfig.visjam.warna[0]);
    tampilkanKeSegmen(mainBuffer, warna);
    if (indexJadwal < 4) mainBuffer[indexJadwal] |= 0x80;
    return;  // Kunci, jangan lanjut ke bawah
  }

  // 2. PRIORITAS KEDUA: PAPAN SKOR
  if (konfig.papanSkor.aktif) {
    buatBufferSkor(mainBuffer);
    return;  // Kunci, jangan lanjut ke bawah
  }
  if (konfig.modeAnimasi.aktif) {
    prosesAnimasiEfek(konfig.modeAnimasi.idEfek);
    return;  // Kunci, jangan lanjut ke bawah
  }
  // 3. PRIORITAS KETIGA: NOTIF SHOLAT (IMSAL - ISYA)
  konfig.modeEfekAktif = false;  // Reset flag
  for (int i = 0; i < 6; i++) {
    notifSholat(
      i,
      jadwalMenit[i],
      konfig.sholat[i].menitPeringatan,
      konfig.sholat[i].idEfekPeringatan,
      konfig.sholat[i].hitungMundur,
      konfig.sholat[i].durasiAdzan,
      konfig.sholat[i].idEfekAdzan,
      konfig.sholat[i].kecerahanHemat,
      konfig.sholat[i].menitHemat);
    if (konfig.modeEfekAktif) return;  // Jika sholat sedang aktif, kunci dan keluar
  }

  // 4. PRIORITAS KEEMPAT: TAMPIL TANGGAL
  if (konfig.tanggal.aktif) {

    bool isAdaEvent = (konfig.tanggal.adaEvent && nmAgnd.length() > 0);

  notifTanggal(
    konfig.tanggal.setiap,
    konfig.tanggal.lama,
    isAdaEvent, // Parameter penentu perilaku
    konfig.tanggal.modeDP,    // Mode Heboh vs Mode Normal
    konfig.tanggal.alertDP   // Speed Heboh vs Speed Normal
  );

  if (konfig.modeEfekAktif) return; 
}

  // 5. DEFAULT: JAM NORMAL (Hanya tampil jika tidak ada prioritas di atas)
  terapkanVisualJam();
}