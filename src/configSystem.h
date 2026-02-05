#ifndef CONFIG_SYSTEM_H
#define CONFIG_SYSTEM_H

#include <Arduino.h>

// ==========================================
// --- STRUKTUR DATA (KONFIGURASI) ---
// ==========================================

// 1. Laci Daftar Efek (Animasi)
struct EfekConfig {
  int id;                 // 1 - 99
  int mode;               // Jenis: Kedip, Putar, Jalan, dll
  int kecepatan;          // Dalam milidetik (ms)
  int kecerahan;          // 0 - 255
  uint8_t polaSegmen[8];  // Bitmask untuk data 595 (a-g + DP)
  int jumlahFrame;
  bool pakaiDP;  // Apakah titik (Decimal Point) aktif
  uint32_t warnaAni;         // WARNA KHUSUS EFEK (Tambahan Elegan)
};

// 2. Laci Pengaturan Notifikasi Sholat
struct NotifSholatConfig {
  int menitPeringatan;   // Jeda sebelum adzan (Fase Warning)
  int idEfekPeringatan;  // Merujuk ke id di EfekConfig
  int hitungMundur;      // Detik sebelum adzan (Fase Countdown)
  int idEfekAdzan;       // Merujuk ke id di EfekConfig
  int durasiAdzan;       // Berapa lama efek adzan tampil (detik)
  int menitHemat;        // Jeda setelah adzan untuk masuk fase redup
  int kecerahanHemat; 
  uint32_t warnaPeringatan;
  uint32_t warnaAdzan;  
  uint32_t warnaHemat;
  uint32_t warnaCdown; // Level redup (0-255)
};

struct NotifTanggalConfig {
  bool aktif;     // Key: "a"
  int setiap;     // Key: "ls"
  int lama;       // Key: "ll"
  int format;     // Key: "ft"
  int idEfek;     // Key: "id"
  bool adaEvent;  // Key: "ev" (Flag dari GAS)
  int alertDP;
  int modeDP;     // Key: "dp" (Opsi kedip DP)

  // --- TAMBAHAN WARNA ---
  uint32_t warnaAgenda;  // Warna saat ada event/agenda dari Google Calendar (GAS)
  uint32_t warnaNormal;  // Warna tampilan rutin (Setiap beberapa menit)
  uint32_t warnaLibur;   // Warna khusus hari Minggu atau hari libur nasional
};

// 5. Laci Pengaturan Pengusir Hama (Ultrasonic Output via 595)
struct NotifUltraConfig {
  bool aktif;
  int frekuensiMin;     // Batas bawah frekuensi (misal 20000 Hz)
  int frekuensiMax;     // Batas atas frekuensi (misal 40000 Hz)
  int intervalAcak;     // Seberapa cepat frekuensi berubah (ms)
  bool aktifMalamSaja;  // Opsi hanya aktif saat gelap/malam
};

// 6. Laci Pengaturan Papan Skor (Sederhana 4 Digit)
struct NotifSkorConfig {
  bool aktif;
  int skorA;           // 0-99 (Tampil di Digit 1 & 2)
  int skorB;           // 0-99 (Tampil di Digit 3 & 4)
  int babak;           // 1 atau 2 (Ditampilkan via DP/Titik)
  bool tampilkanSkor;  // Switch antara JAM atau SKOR
};
struct WifiConfig {
  String ssid;      // SSID Router tujuan
  String pass;      // Password Router tujuan      // Nama SSID yang dipancarkan ESP32 (Host)
  String authUser;  // Username Web Login
  String authPass;  // Password Web Login & Password WiFi AP
};
struct VisualJam {
    uint32_t warna[4];    // Warna D1, D2, D3, D4
    bool titikAktif;      // On/Off Titik Dua
    bool animasiAktif;    // Saklar Animasi Jam
    int idAnimasi;        // ID dari prosesAnimasiEfek
};

// --- INDUK SEGALA PENGATURAN (SISTEM CONFIG) ---
struct SistemConfig {
  
  WifiConfig wikon;
  NotifSholatConfig sholat[6];  // 0:Subuh, 1:Dzuhur, 2:Ashar, 3:Maghrib, 4:Isya       // 10 slot agenda tambahan
  NotifTanggalConfig tanggal;   // 5 slot hari spesial
  EfekConfig daftarEfek[15];
  struct {
    bool aktif;
    int idEfek;
  } modeAnimasi;                  // Wadah untuk 60 jenis animasi
  NotifUltraConfig pengusirHama;  // Ultrasonic pengusir Hama
  NotifSkorConfig papanSkor;
  VisualJam visjam;

  // --- TAMBAHAN UNTUK VISUAL & LOOPING (Sinkron Web) ---
  int modeKedip;       // id="modeKedip"
  int speedKedip;      // id="speedKedip"
  int durasiKedip;     // id="durasiKedip"
  bool titikDuaKedip;  // id="titikDuaKedip"

  int loopSetiap;  // id="loop_setiap"
  int loopLama;    // id="loop_lama"
  int fmtTgl;      // id="fmt_tgl"
  int timezone;    // id="tz"
  // -----------------------------------------------------

  int jumlahDigit;
  int kecerahanGlobal;  // Kecerahan jam saat kondisi normal
  char namaPerangkat[32];
  char urlGAS[150];  // Nama yang muncul di WiFi/mDNS
  char terakhirSync[20];
  char authUser[16];
  char authPass[16];

  //Hanya Lewat
  bool modeEfekAktif = false;
  int idEfekTes = 0;
  uint32_t warnajam;
};

// Variabel Utama untuk digunakan di seluruh kode
extern SistemConfig konfig;

// Config Default
bool standarConfig();

#endif
