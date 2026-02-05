#ifndef DISPLAY_LOGIC_H
#define DISPLAY_LOGIC_H

#include <Arduino.h>
#include "configSystem.h" // Mengakses struct konfig

#define LEDS_PER_SEGMENT 6
#define LEDS_PER_DIGIT (LEDS_PER_SEGMENT * 7) // 42 LED per digit
#define LEDS_DP 4
#define MAX_DIGIT 12

// --- TABEL KARAKTER STANDAR ---
const uint8_t angkaSegmen[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
const uint8_t segmenKosong = 0x00;
const uint8_t segmenStrip  = 0x40;


CRGB leds[MAX_DIGIT][LEDS_PER_DIGIT + LEDS_DP];
CRGB targetPixel[MAX_DIGIT][LEDS_PER_DIGIT + LEDS_DP];

// --- BUFFER & TIMING ---
uint8_t mainBuffer[MAX_DIGIT];  // Buffer tetap aman meskipun digit di web diubah-ubah
CRGB warna[MAX_DIGIT];
CRGB warnaTemp[MAX_DIGIT];
unsigned long lastUpdateJam = 0;
struct tm timeinfo;
/**
 * 1. MENDAPATKAN BYTE PER DIGIT
 */
uint8_t dapatkanByte(int angka, bool pakaiDP) {
  if (angka < 0 || angka > 9) return segmenKosong;
  uint8_t pola = angkaSegmen[angka];
  if (pakaiDP) pola |= 0x80;
  return pola;
}
// Ganti uint32_t* (pointer) menjadi uint32_t (nilai langsung)
void pakaiWarna(uint32_t warnaKonfig) {
  for (int i = 0; i < konfig.jumlahDigit; i++) {
    // Sekarang benar: Nilai warna dimasukkan ke setiap digit
    warna[i] = CRGB(warnaKonfig); 
  }
}
void fadDing(int d) {
  // Tugas: Mengoles semua LED di digit 'd' dari targetPixel ke leds
  for (int i = 0; i < (LEDS_PER_DIGIT + LEDS_DP); i++) {
    nblend(leds[d][i], targetPixel[d][i], 65);
  }
}
/**
 * 2. MENGISI BUFFER JAM (Dinamis 4 atau 6 Digit)
 */
void buatBufferWaktu(int jam, int menit, int detik, uint8_t* buffer, bool titikNyala) {
  if (konfig.jumlahDigit < 4) return;

  // Bersihkan semua buffer dulu agar tidak ada data sisa (Ghosting)
  for(int i = 0; i < konfig.jumlahDigit; i++) {
    buffer[i] = segmenKosong; // atau bisa pakai segmenKosong;
  }

  if (konfig.jumlahDigit == 4) {
    // MODE 4 DIGIT: Tampilkan Jam & Menit
    buffer[0] = dapatkanByte(jam / 10, false);
    // Gunakan titikNyala sebagai pemisah detik yang berkedip
    buffer[1] = dapatkanByte(jam % 10, titikNyala); 
    buffer[2] = dapatkanByte(menit / 10, titikNyala);
    buffer[3] = dapatkanByte(menit % 10, false);
  } 
  else if (konfig.jumlahDigit >= 6) {
    // MODE 6 DIGIT: Jam, Menit, Detik
    buffer[0] = dapatkanByte(jam / 10, false);
    buffer[1] = dapatkanByte(jam % 10, titikNyala); 
    buffer[2] = dapatkanByte(menit / 10, titikNyala);
    buffer[3] = dapatkanByte(menit % 10, false); 
    buffer[4] = dapatkanByte(detik / 10, false);
    buffer[5] = dapatkanByte(detik % 10, false);
  }
}
uint32_t dapatkanWarnaCuaca(int kode) {
  // 0-2: Cerah, 3-4: Berawan, 60+: Hujan, 95: Petir
  if (kode <= 2) return 0xFFA500;      // Oranye (Cerah)
  if (kode <= 5) return 0xFFFFFF;      // Putih (Mendung)
  if (kode >= 60 && kode < 90) return 0x0000FF; // Biru (Hujan)
  if (kode >= 95) return 0xFF0000;     // Merah (Petir)
  
  return konfig.visjam.warna[0];       // Warna standar Bos
}

void buatBufferAngka(int angka, uint8_t* buf) {

  // 2. Logika Penempatan Angka & Strip
  if (angka < 10) {
    // Jika angka satuan (0-9), tampilkan "- 09" di tengah
    buf[0] = segmenStrip;                         // Strip di digit 0
    buf[2] = dapatkanByte(0, false);               // Angka 0 di digit 2
    buf[3] = dapatkanByte(angka, false);           // Angka satuan di digit 3
  } 
  else if (angka < 100) {
    // Jika angka puluhan (10-99), tampilkan "- 59"
    buf[0] = segmenStrip;                         // Strip di digit paling kiri
    buf[2] = dapatkanByte(angka / 10, false);      // Puluhan di digit 2
    buf[3] = dapatkanByte(angka % 10, false);      // Satuan di digit 3
  } 
  else {
    // Jika angka ratusan (misal sisa detik masih banyak)
    buf[0] = segmenStrip;
    buf[1] = dapatkanByte((angka / 100) % 10, false);
    buf[2] = dapatkanByte((angka / 10) % 10, false);
    buf[3] = dapatkanByte(angka % 10, false);
  }
}
 //* MENGISI BUFFER TANGGAL (Format DD:MM)


void buatBufferTanggal(struct tm info, uint8_t* buf, int format) {
  int tgl = info.tm_mday;
  int bln = info.tm_mon + 1;
  int thn = info.tm_year + 1900;

  // Bersihkan buffer
  for(int i=0; i<konfig.jumlahDigit; i++) buf[i] = segmenKosong;

  switch (format) {
    case 0: // Format DD.MM (Contoh: 09.01)
      buf[0] = dapatkanByte(tgl / 10, false); 
      buf[1] = dapatkanByte(tgl % 10, true);  // Titik nyala di sini sebagai pemisah
      buf[2] = dapatkanByte(bln / 10, false);
      buf[3] = dapatkanByte(bln % 10, false);
      break;

    case 1: // Format MM.YY (Contoh: 01.26)
      buf[0] = dapatkanByte(bln / 10, false);
      buf[1] = dapatkanByte(bln % 10, true);  // Titik nyala di sini
      buf[2] = dapatkanByte((thn % 100) / 10, false);
      buf[3] = dapatkanByte((thn % 100) % 10, false);
      break;

    case 2: // Format YYYY (Contoh: 2026)
      buf[0] = dapatkanByte(thn / 1000, false);
      buf[1] = dapatkanByte((thn / 100) % 10, false);
      buf[2] = dapatkanByte((thn / 10) % 10, false);
      buf[3] = dapatkanByte(thn % 10, false);
      break;
      
    default: // Backup jika format salah
      buf[0] = dapatkanByte(tgl / 10, false);
      buf[1] = dapatkanByte(tgl % 10, true);
      buf[2] = dapatkanByte(bln / 10, false);
      buf[3] = dapatkanByte(bln % 10, false);
      break;
  }
}
 //* 3. MENGISI BUFFER SKOR (Papan Skor)
void buatBufferSkor(uint8_t* buffer) {
  // Pastikan setidaknya ada 2 digit untuk Skor A
  if (konfig.jumlahDigit >= 2) {
    buffer[0] = dapatkanByte(konfig.papanSkor.skorA / 10, false);
    buffer[1] = dapatkanByte(konfig.papanSkor.skorA % 10, (konfig.papanSkor.babak == 1));
  }
  
  // Pastikan ada digit ke-3 dan ke-4 untuk Skor B
  if (konfig.jumlahDigit >= 4) {
    buffer[2] = dapatkanByte(konfig.papanSkor.skorB / 10, false);
    buffer[3] = dapatkanByte(konfig.papanSkor.skorB % 10, (konfig.papanSkor.babak == 2));
  }
  
  // Digit sisa (5 ke atas) dikosongkan
  for(int i = 4; i < konfig.jumlahDigit; i++) {
    if (i < MAX_DIGIT) buffer[i] = segmenKosong;
  }
}

/**
 * 4. EKSEKUSI EFEK BERDASARKAN ID
 * Digunakan untuk Adzan, Alarm, atau Notif Agenda
 */
void setDigitRaw(int digit, uint8_t pola) {
  if (digit < konfig.jumlahDigit) {
    mainBuffer[digit] = pola;
  }
}

// Fungsi untuk mengosongkan layar
void clearDisplay() {
  for (int i = 0; i < konfig.jumlahDigit; i++) {
    mainBuffer[i] = 0x00; // Kosongkan pola bitmask
    
    // Kosongkan juga targetPixel per LED agar fading langsung lari ke hitam
    for (int l = 0; l < (LEDS_PER_DIGIT + LEDS_DP); l++) {
      targetPixel[i][l] = CRGB::Black;
    }
  }
}

void tampilkanKeSegmen(uint8_t* buf, CRGB* warnaS) {
  // --- PAGAR PEMBATAS ---
  const int batasAngka = LEDS_PER_SEGMENT * 7; // Area Angka (0-41)
  const int totalLeds  = LEDS_PER_DIGIT + LEDS_DP; // Total (0-45)

  for (int d = 0; d < konfig.jumlahDigit; d++) {
    uint8_t pola = buf[d];

    for (int l = 0; l < totalLeds; l++) {
      if (l < batasAngka) {
        // --- LAMPU ANGKA ---
        int bitKe = l / LEDS_PER_SEGMENT; // 0 sampai 6
        bool nyala = (pola >> bitKe) & 0x01;
        targetPixel[d][l] = nyala ? warnaS[d] : CRGB::Black;
      } 
      else {
        // --- LAMPU SISANYA (PASTI DP) ---
        // Langsung intip bit ke-8 (0x80)
        bool titikNyala = (pola & 0x80);
        targetPixel[d][l] = titikNyala ? warnaS[d] : CRGB::Black;
      }
    }
    fadDing(d);
  }
}

void display(){
 FastLED.show();
}

#endif