#ifndef SYSTEM_LOGIC_H
#define SYSTEM_LOGIC_H

#include <Arduino.h>
#include <FastLED.h>
#include "configSystem.h"  // Mengakses struct konfig
#include "efek_visual.h"
#include "cuaca.h"




extern CRGB leds[MAX_DIGIT][LEDS_PER_DIGIT + LEDS_DP];
extern int detikSekarang;
extern bool isLiburNasional;
int lastMode = -1;             // Untuk simpan mode sebelumnya
unsigned long mulaiAnimasi = 0; // Untuk simpan waktu "start" sapuan
int idShAktif = 0;
int tipeNotif= 0;

extern void setKecerahan(int level);
//extern void updateTampilanSegmen(uint8_t* dataArray, int jumlahIC);
extern void buatBufferTanggal(struct tm info, uint8_t* buf, int format);
extern void buatBufferAngka(int angka, uint8_t* buf);
extern void buatBufferWaktu(int jam, int menit, int detik, uint8_t* buf, bool dot);
extern void clearDisplay();
extern void padDing(int d);

CRGB warnaW;

void eksekusiLogikaDP(int mode, bool state) {
  int pinAwal = 42;
  
  // SATU LOOP UNTUK SEMUA (42-45)
  for (int i = 0; i < 4; i++) {
    int pinSekarang = pinAwal + i;
    bool nyala = false; // Default mati

    switch (mode) {
      case 1: // GANTIAN (state true = 42,43 | state false = 44,45)
        if (state) { if (i < 2) nyala = true; } 
        else       { if (i >= 2) nyala = true; }
        break;

      case 2: // KEDIP BARENG
        if (state) nyala = true;
        break;

      case 3: // HEARTBEAT
        if ((millis() % 1000) < 150 || ((millis() % 1000) > 300 && (millis() % 1000) < 450)) {
          nyala = true;
        }
        break;

      case 4: // NYALA TERUS
        nyala = true;
        break;
    }
    CRGB warnaFnal = nyala ? warnaW : CRGB :: Black;

    // Eksekusi ke targetPixel
    targetPixel[1][pinSekarang] = warnaFnal;
    leds[1][pinSekarang] = warnaFnal;
  }
}
void prosesAnimasiEfek(int idEfek) {
  int slot = idEfek - 1;
  if (slot < 0 || slot >= 15) return;

  int speed = konfig.daftarEfek[slot].kecepatan;
  int mode = konfig.daftarEfek[slot].mode;

  if (mode != lastMode) {
        mulaiAnimasi = millis(); // Catat waktu saat tombol ditekan/mode ganti
        lastMode = mode;         // Simpan mode sekarang sebagai mode terakhir
    }
  
  int brDasar = konfig.daftarEfek[slot].kecerahan;
  int warnaBg = konfig.visjam.warna[0];
 
  CRGB warnaW = CRGB(konfig.daftarEfek[slot].warnaAni);
  if (konfig.modeEfekAktif) {
    if(tipeNotif == 0) warnaW = CRGB(konfig.sholat[idShAktif].warnaPeringatan);
    else if (tipeNotif == 1) warnaW = CRGB(konfig.sholat[idShAktif].warnaAdzan);
    else if (tipeNotif == 3) warnaW = CRGB(0, 255, 0); // Hijau Jumat
    else if (tipeNotif == 4) warnaW = CRGB(konfig.tanggal.warnaLibur);
    else if (tipeNotif == 6) warnaW = CRGB(konfig.tanggal.warnaAgenda); // <--- INI DIA!
    else                     warnaW = CRGB(konfig.tanggal.warnaNormal);
  }
  unsigned long skrg = millis();
  int brFinal = brDasar;

  if (konfig.daftarEfek[slot].jumlahFrame <= 0) return;
  

  // Cek apakah pakpai jam atau pola custom
  bool polaBuffer = (konfig.daftarEfek[slot].polaSegmen[0] == 0);
  

  // LOOP UNTUK SETIAP DIGIT
  for (int d = 0; d < konfig.jumlahDigit; d++) {
    uint8_t bitmask ; // Tetap menggunakan nama variabel asli
    
    if (polaBuffer) {
      bitmask = mainBuffer[d]; // Pakai data jam/skor
    } else {
      int totalFrame = konfig.daftarEfek[slot].jumlahFrame;
      int frameAktif = (totalFrame > 0) ? (skrg / (speed + 1)) % totalFrame : 0;
      bitmask = konfig.daftarEfek[slot].polaSegmen[frameAktif];
      
      // Tambahkan DP jika diatur di web
     if (!konfig.daftarEfek[slot].pakaiDP) bitmask &= 0x7F;
    }

    // --- EKSEKUSI MODE ---
    switch (mode) {
      case 1: efekRainbowWipe(d, bitmask, speed, brFinal, mulaiAnimasi); break;
      case 2: efekGlobalChase(d, bitmask, speed, brFinal, warnaW); break;
      case 3: efekRaindrop(d, bitmask, speed, brFinal, warnaW); break;
      case 4: efekBreathe(d, bitmask, speed, brFinal, warnaW); break;
      case 5: efekFire(d, bitmask, speed, brFinal, warnaW); break; // Case 5
      case 6: efekFireflies( d,  bitmask,  speed,  brFinal, warnaW); break; // Efek Baru
      case 7: efekSilkSweep(d, bitmask, speed, brFinal, warnaW); break;
      case 8: efekSolarEclipse(d, bitmask, speed, brFinal, warnaW); break;
      case 9: efekBlinkWS( d,  bitmask,  speed,  brFinal,  warnaW); break;
      case 10: efekTimeWarp( d,  bitmask,  speed,  brFinal,  warnaW); break;
      case 11: efekLiquidSmoke( d,  bitmask,  speed,  brFinal,  warnaW); break;
      
     default: 
        // Mode 0 atau default: Lukis ke targetPixel agar di-blend halus
        for (int s = 0; s < 8; s++) {
          int offset = s * LEDS_PER_SEGMENT;
          int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;
          bool nyala = (bitmask >> s) & 0x01;
          for (int l = 0; l < jml; l++) {
            targetPixel[d][offset + l] = nyala ? warnaW : CRGB::Black;
          }
        }
        break;
    }
    fadDing(d);
  }
  FastLED.setBrightness(brFinal);

}


void notifTanggal(int setiap, int durasi, bool adaAgenda, int modeDP, int speedDP) {
  if (setiap <= 0) return;
  
  // --- JALUR 1: KASTA TERTINGGI (AGENDA) ---
  if (adaAgenda) {
    konfig.modeEfekAktif = true;
    
    // Tentukan warna khusus agenda (Misal tipe 6)
    if (timeinfo.tm_wday == 5) tipeNotif = 3; // Tetap Hijau kalau Jumat
    else                       tipeNotif = 6; // Warna Agenda dari Konfig

    int modeFinal = 3;
    int speedFinal =speedDP; // Standar 500ms kalau rutin
    bool stateKedip = (millis() / speedFinal) % 2 == 0;

    buatBufferTanggal(timeinfo, mainBuffer, konfig.tanggal.format);
    
    prosesAnimasiEfek(konfig.tanggal.idEfek); 
    eksekusiLogikaDP(modeDP, stateKedip); 
    return; // Langsung pulang, jangan lirik jalur rutin
  }

  // --- JALUR 2: KASTA RUTIN (MODULO) ---
  long sekarangDetikTotal = (timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec;
  if ((sekarangDetikTotal % setiap) < durasi) {
    konfig.modeEfekAktif = true;

    if (timeinfo.tm_wday == 5)      tipeNotif = 3; // Jumat
    else if (timeinfo.tm_wday == 0) tipeNotif = 4; // Minggu/Libur
    else                            tipeNotif = 5; // Normal

    bool stateKedip = (millis() / 500) % 2 == 0;

    // 3. Eksekusi suntik titik
    buatBufferTanggal(timeinfo, mainBuffer, konfig.tanggal.format);
    
    prosesAnimasiEfek(konfig.tanggal.idEfek); 
    eksekusiLogikaDP(modeDP, stateKedip);
    return;
  }

  // JALUR 3: CLEANUP
  if (konfig.modeEfekAktif) konfig.modeEfekAktif = false;
}

// Fungsi pembantu agar kodingan tidak panjang di atas

// Kita tambahkan parameter pertama: int mJadwal (Menit Jadwal
void notifSholat(int idx, int mJadwal, int mPeringatan, int idEfekPeringatan, int detikMundur, int durasiAdzan, int idEfekAdzan, int levelRedup, int menitLamaRedup) {

  if (mJadwal <= 0) return;
  idShAktif = idx;
  extern struct tm timeinfo;
  long sekarangMenitTotal = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
  long sekarangDetikTotal = (sekarangMenitTotal * 60) + timeinfo.tm_sec;
  long jadwalDetikTotal = mJadwal * 60; 
  bool statusTitik = (timeinfo.tm_sec % 2 == 0);

  // --- 1. JALUR PUNCAK: ADZAN ---
  if (sekarangMenitTotal >= mJadwal && sekarangMenitTotal < (mJadwal + durasiAdzan)) {
     tipeNotif= 1;
    
    buatBufferWaktu(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, mainBuffer, statusTitik);
    prosesAnimasiEfek(idEfekAdzan);
    konfig.modeEfekAktif = true;
    return;
  }

  // --- 2. JALUR COUNTDOWN ---
  if (sekarangDetikTotal >= (jadwalDetikTotal - detikMundur) && sekarangDetikTotal < jadwalDetikTotal) {
    
    int sisaDetik = (int)(jadwalDetikTotal - sekarangDetikTotal);

    buatBufferAngka(sisaDetik, mainBuffer);
    pakaiWarna(konfig.sholat[idx].warnaCdown); // Ambil dari Struct clC
    tampilkanKeSegmen(mainBuffer, warna);
    konfig.modeEfekAktif = true;
    return;
  }

  // --- 3. JALUR PERINGATAN ---
  if (sekarangMenitTotal >= (mJadwal - mPeringatan) && sekarangMenitTotal < mJadwal) {
    tipeNotif = 0;

    buatBufferWaktu(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, mainBuffer, statusTitik);

    prosesAnimasiEfek(idEfekPeringatan);
    konfig.modeEfekAktif = true;
    return;
  }

  // --- 4. JALUR REDUP/HEMAT ---
  int menitMulaiRedup = mJadwal + durasiAdzan;
  if (sekarangMenitTotal >= menitMulaiRedup && sekarangMenitTotal < (menitMulaiRedup + menitLamaRedup)) {

    buatBufferWaktu(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, mainBuffer, statusTitik);
    pakaiWarna(konfig.sholat[idx].warnaHemat); // Ambil dari Struct clH
    tampilkanKeSegmen(mainBuffer, warna);
    setKecerahan(levelRedup);
   
    konfig.modeEfekAktif = true;
    return;
  }

  clearDisplay();
  konfig.modeEfekAktif = false;
}
void displayLoadingSinyal(uint32_t speed) {
  static unsigned long lastUpdate = 0;
  static int pixelAktif = 0; 
  
  const int ledsSatuDigit = (7 * LEDS_PER_SEGMENT) + LEDS_DP; 
  const int totalLedsSemua = konfig.jumlahDigit * ledsSatuDigit;

  if (millis() - lastUpdate >= speed) {
    lastUpdate = millis();

    if (pixelAktif < totalLedsSemua) {
      int d = pixelAktif / ledsSatuDigit;
      int l = pixelAktif % ledsSatuDigit;
      
     if (d < konfig.jumlahDigit && l < (LEDS_PER_DIGIT + LEDS_DP)) { 
    targetPixel[d][l] = CHSV((pixelAktif * 255 / totalLedsSemua), 255, 255);
}
      pixelAktif++;
    } else {
      for(int i = 0; i < konfig.jumlahDigit; i++) {
          for(int j = 0; j < ledsSatuDigit; j++) { 
              targetPixel[i][j] = CRGB::Black;
          }
      }
      pixelAktif = 0; 
    }
  }

  for (int d = 0; d < konfig.jumlahDigit; d++) {
    fadDing(d);
  }
  FastLED.show(); 
}
void terapkanVisualJam() {
  // Tubuh jam
  bool statusTitik = (konfig.titikDuaKedip) ? (detikSekarang % 2 == 0) : false;
  buatBufferWaktu(timeinfo.tm_hour, timeinfo.tm_min, detikSekarang, mainBuffer, statusTitik);
  if (konfig.visjam.animasiAktif) {
    
    prosesAnimasiEfek(konfig.visjam.idAnimasi); 
    setKecerahan(konfig.kecerahanGlobal);
    return;
  } 
  if (cuaca.status.aktif){
    uint32_t warnaCuaca = tentukanWarnaCuaca(cuaca.status.kodeCuaca);

    for ( int d = 0 ; d < konfig.jumlahDigit; d++){
      warna[d] = CRGB(warnaCuaca);
    }

    tampilkanKeSegmen(mainBuffer, warna);
    return;
  }
  
  pakaiWarna(konfig.visjam.warna[0]);
    
    // Lukis ke targetPixel
  tampilkanKeSegmen(mainBuffer, warna);
    
}

#endif