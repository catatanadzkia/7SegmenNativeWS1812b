#ifndef EFEK_VISUAL_H
#define EFEK_VISUAL_H

#include <FastLED.h>



extern CRGB leds[MAX_DIGIT][LEDS_PER_DIGIT + LEDS_DP];


// Gunakan 'extern' agar file ini tahu bahwa variabel ini ada di file lain (main.cpp)

extern uint8_t mainBuffer[MAX_DIGIT];

// --- KOLEKSI FUNGSI EFEK ---

// CASE 1: Rainbow Wipe (Sekali sapu)
void efekRainbowWipe(int d, uint8_t bitmask, int speed, int brFinal, unsigned long tMulai) {
    const uint8_t tinggiY[] = {100, 75, 25, 0, 25, 75, 50, 0}; 
    unsigned long waktuJalan = millis() - tMulai;
    uint16_t wipe = waktuJalan / (speed + 1);
    uint8_t baseHue = (millis() / 20) & 255;

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        // CEK BITMASK: Cuma proses segmen yang harus nyala
        if (bitmask & (1 << s)) {
            for (int l = 0; l < jml; l++) {
                int posVertical = tinggiY[s] + (l * 5); 
                if (posVertical <= wipe) {
                    // WARNA DIHITUNG DISINI (OTOMATIS)
                    uint8_t hueLED = baseHue + (d * 20) + (posVertical / 2);
                    targetPixel[d][offset + l] = CHSV(hueLED, 255, brFinal);
                } else {
                    targetPixel[d][offset + l] = CRGB::Black;
                }
            }
        } else {
            // Segmen yang gak ada di angka, matiin semua LED-nya
            for(int l = 0; l < jml; l++) targetPixel[d][offset + l] = CRGB::Black;
        }
    }
}
// CASE 2: Global Multi-Chase (Komet Kejar-kejaran)
void efekGlobalChase(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    int ledsPerDigit = 8 * LEDS_PER_SEGMENT;
    int totalLedDisplay = konfig.jumlahDigit * ledsPerDigit; 
    
    CRGB warnaBg = CRGB(konfig.visjam.warna[0]);
    warnaBg.nscale8_video(brFinal / 3); 
    uint16_t pos1 = map(beat16(speed), 0, 65535, 0, totalLedDisplay - 1);
    uint16_t pos2 = (pos1 + (totalLedDisplay / 3)) % totalLedDisplay;
    uint16_t pos3 = (pos1 + (2 * totalLedDisplay / 3)) % totalLedDisplay;

    uint8_t hueBase = (millis() / 20) & 255;

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        if (bitmask & (1 << s)) {
            for (int l = 0; l < LEDS_PER_SEGMENT; l++) {
                int idxLocal = offset + l;
                int posAbsolut = (d * ledsPerDigit) + idxLocal;
                
                int dist1 = abs(posAbsolut - (int)pos1);
                int dist2 = abs(posAbsolut - (int)pos2);
                int dist3 = abs(posAbsolut - (int)pos3);
                
                if (dist1 > totalLedDisplay / 2) dist1 = totalLedDisplay - dist1;
                if (dist2 > totalLedDisplay / 2) dist2 = totalLedDisplay - dist2;
                if (dist3 > totalLedDisplay / 2) dist3 = totalLedDisplay - dist3;

                int minDist = min(dist1, min(dist2, dist3));

                if (minDist < 8) { 
                    uint8_t brKomet = map(minDist, 0, 8, 255, 0);
                    uint8_t warnaPerLED = hueBase + (posAbsolut * 8); 
    
                    CRGB warnaKomet = CHSV(warnaPerLED, 255, 255);
                    if (minDist <= 1) warnaKomet = CRGB::White; 
                    
                    warnaKomet.nscale8_video(brKomet);
                    warnaKomet.nscale8_video(brFinal);
                    targetPixel[d][idxLocal] = warnaKomet; 
                } else {
                    CRGB bgFinal = warnaW;
                    bgFinal.nscale8_video(brFinal);
                    targetPixel[d][idxLocal] = bgFinal; 
                }
            }
        } else {
            for(int l=0; l<LEDS_PER_SEGMENT; l++) {
                targetPixel[d][offset + l] = CRGB :: Black; 
            }
        }
    }
}
// CASE 3: Raindrop (Efek Hujan Halus dengan Gravitasi)
void efekRaindrop(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    CRGB warnaBg = CRGB::Black; // Area mati harus benar-benar steril

    for (int s = 0; s < 7; s++) { // Kita urus segmen A sampai G
        int offset = s * LEDS_PER_SEGMENT;
        bool isNyala = (bitmask & (1 << s)); // Cek bitmask: Apakah segmen ini bagian dari angka?
        
        // Identifikasi Jalur Gravitasi
        bool isKanan = (s == 1 || s == 2); // Segmen b, c (Atas ke Bawah)
        bool isKiri  = (s == 4 || s == 5); // Segmen e, f (Bawah ke Atas)
        
        // Offset waktu agar jatuhnya acak antar batang
        uint32_t variabelWaktu = millis() + (s * 120) + (d * 400);
        int dropPos = (variabelWaktu / (speed + 20)) % (LEDS_PER_SEGMENT + 10);

        for (int l = 0; l < LEDS_PER_SEGMENT; l++) {
            int idx = offset + l;
            int posVirtual;

            // --- KOREKSI ALUR KABEL (GRAVITASI) ---
            if (isKanan) {
                posVirtual = l; // Turun mengikuti data
            } else if (isKiri) {
                posVirtual = (LEDS_PER_SEGMENT - 1) - l; // Turun melawan data
            } else {
                posVirtual = l; // Horisontal
            }

            CRGB warnaHasil = CRGB::Black;

            // --- LOGIKA PAYUNG (BITMASK) ---
            if (isNyala) { 
                // Jika bitmask nyala, baru kita lukis hujannya
                if (isKanan || isKiri) {
                    if (posVirtual == dropPos) {
                        warnaHasil = CRGB::White; // Kepala tetesan
                    } else if (posVirtual < dropPos && posVirtual > dropPos - 4) {
                        int jarak = dropPos - posVirtual;
                        warnaHasil = warnaW;
                        warnaHasil.nscale8_video(255 - (jarak * 60)); // Ekor redup
                    } else {
                        // Sisa batang yang nyala tapi gak kena tetesan
                        warnaHasil = warnaW;
                        warnaHasil.nscale8_video(30); // Redup banget sebagai "sisa air"
                    }
                } else {
                    // Batang Horisontal (a, g, d) hanya nyala redup/statis
                    warnaHasil = warnaW;
                    warnaHasil.nscale8_video(60); 
                }
            } else {
                // JIKA BITMASK MATI = MATI TOTAL (Anti-Guyur)
                warnaHasil = CRGB::Black;
            }

            // Tulis ke targetPixel untuk di-blend oleh fadDing
            targetPixel[d][idx] = warnaHasil.nscale8_video(brFinal);
        }
    }
}
// CASE 4: Breathe WS (Gradient Cross-Fade & Organic Pulse)
void efekBreathe(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    
    CRGB warnaA = (warnaW == CRGB(0,0,0)) ? CHSV(ms / 50, 255, 255) : warnaW;
    CRGB warnaB = CRGB(konfig.visjam.warna[0]);
    
    // Kunci BG redup di angka 40 (biar tetep dapet auranya)
    CRGB bgRedup = warnaB;
    bgRedup.nscale8_video(40);

    // 1. NAPAS GLOBAL (Satu irama untuk semua segmen)
    // Gunakan quadwave8 agar napasnya lebih "dalam" dan tenang (elegan)
    uint8_t breath = quadwave8(beat8(speed / 4)); 

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        if (bitmask & (1 << s)) {
            for (int l = 0; l < jml; l++) {
                // 2. RAHASIA ELEGAN: Jangan cuma blend warna, tapi blend intensitas
                // Kita blend dari warna background (redup) ke warna utama (terang)
                CRGB finalW = blend(bgRedup, warnaA, breath);
                
                // Gunakan targetPixel agar fadDing membantu kehalusan transisi
                targetPixel[d][offset + l] = finalW.nscale8_video(brFinal);
            }
        } else {
            // Background segmen mati: Tetap anteng di bgRedup
            for (int l = 0; l < jml; l++) {
                targetPixel[d][offset + l] = bgRedup.nscale8_video(brFinal);
            }
        }
    }
}

// CASE 5: Fire (Organic Heat Mapping)
void efekFire(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    
    // Warna Background dari visjam (Dibuat sangat redup/aura bara)
    CRGB warnaBg = CRGB(konfig.visjam.warna[0]);
    warnaBg.nscale8_video(15); // Nilai 15 dari 255 (Sangat redup)

    for (int s = 0; s < 7; s++) { // Segmen a-g
        int offset = s * LEDS_PER_SEGMENT;
        bool isNyala = (bitmask & (1 << s));
        
        // Identifikasi Sisi untuk Koreksi Gravitasi
        bool isKanan = (s == 1 || s == 2); // b, c (Kabel: Atas ke Bawah)
        bool isKiri  = (s == 4 || s == 5); // e, f (Kabel: Bawah ke Atas)

        for (int l = 0; l < LEDS_PER_SEGMENT; l++) {
            int idx = offset + l;
            int posVirtual;

            // --- KOREKSI GRAVITASI (API NAIK KE ATAS) ---
            if (isKanan) {
                posVirtual = (LEDS_PER_SEGMENT - 1) - l; // Balik jalur agar panas dari bawah
            } else if (isKiri) {
                posVirtual = l; // Jalur asli sudah dari bawah
            } else {
                posVirtual = l; // Horisontal (a, g, d)
            }

            if (isNyala) {
                // LOGIKA API MEMBARA PADA BITMASK
                // Noise bergerak naik (pengurangan ms)
                uint8_t noise = inoise8(d * 100, (s * 50) + (posVirtual * 60) - (ms / (speed + 1)));
                
                // Bonus panas di bagian bawah segmen
                uint8_t bonusPanas = (LEDS_PER_SEGMENT - posVirtual) * (255 / LEDS_PER_SEGMENT);
                uint8_t heat = qadd8(noise, bonusPanas / 2);
                
                // Ambil warna dasar api
                CRGB warnaApi = HeatColor(heat);
                
                // Jika warnaW bukan hitam, kita warnai apinya sesuai warnaW (misal api biru/hijau)
                if (warnaW != CRGB(0,0,0)) {
                    // Teknik blending agar karakter api tetap terjaga tapi warnanya ikut warnaW
                    warnaApi = nblend(warnaApi, warnaW, 128); 
                }

                targetPixel[d][idx] = warnaApi.nscale8_video(brFinal);
            } else {
                // BACKGROUND SANGAT REDUP (Aura Bara)
                targetPixel[d][idx] = warnaBg.nscale8_video(brFinal);
            }
        }
    }
}


// CASE 6: Bio-Luminescence (Aliran Energi Organik)
void efekFireflies(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    CRGB warnaKuning = CRGB(konfig.visjam.warna[0]);
    CRGB warnaKlembak = warnaW == CRGB(0,0,0) ? CRGB::White : warnaW;

    // Kunci warna BG redup (60 itu sekitar 25%)
    CRGB bgRedup = warnaKuning;
    bgRedup.nscale8_video(40);

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT; // Pastikan titik ikut dihitung
        bool isBitmask = (bitmask & (1 << s));

        for (int l = 0; l < jml; l++) {
            int idx = offset + l;
            CRGB warnaHasil;

            if (isBitmask) {
                // 1. Logika Pergerakan Kunang-kunang
                // Kita gunakan ms * speed tanpa dibagi 100 agar lincah
                uint8_t noise = inoise8(d * 50, (s * 30) + (l * 50), ms * speed / 20);
                
                // 2. Threshold "Pedes"
                // Hanya noise tinggi yang jadi kunang-kunang
                if (noise > 180) {
                    uint8_t kontras = map(noise, 180, 255, 0, 255);
                    // Blend dari Kuning ke Putih/WarnaW
                    warnaHasil = blend(warnaKuning, warnaKlembak, kontras);
                    // Kasih boost biar "nyolot" kunang-kunangnya
                    if (kontras > 200) warnaHasil += CRGB(50, 50, 50);
                } else {
                    warnaHasil = warnaKuning;
                }
            } else {
                warnaHasil = bgRedup;
            }

            targetPixel[d][idx] = warnaHasil.nscale8_video(brFinal);
        }
    }
}
void efekSilkSweep(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    int totalDigit = konfig.jumlahDigit;
    int ledsPerDigit = 8 * LEDS_PER_SEGMENT;

    // 1. Warna Dasar (Background)
    CRGB warnaBg = CRGB(konfig.visjam.warna[0]);
    // Buat cadangan warna background redup agar tidak hitung terus di dalam loop
    CRGB bgRedup = warnaBg;
    bgRedup.nscale8_video(50); // Sekitar 20% kecerahan

    // 2. Posisi Sapuan Global
    // Menggunakan beat16 agar gerakan sapuan sinkron dan halus
    float sweepX = (float)beat16(speed) / 65535.0 * (totalDigit + 1.5);

    // Mapping koordinat X tiap segmen
    float segmenX[] = {0.5, 0.9, 0.9, 0.5, 0.1, 0.1, 0.5, 1.0}; 

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        
        for (int l = 0; l < LEDS_PER_SEGMENT; l++) {
            int idxLocal = offset + l;
            int posAbs = (d * ledsPerDigit) + idxLocal;

            // Offset per LED supaya sapuan terlihat miring/halus (Silk Effect)
            float ledOffset = (float)l * 0.05; 
            float physX = (float)d + segmenX[s] + ledOffset;

            // Hitung jarak dari pusat sapuan
            float dist = fabs(sweepX - physX);

            // Tentukan warna sapuan (Pelangi atau warnaW)
            CRGB warnaSapu = (warnaW == CRGB(0,0,0)) 
                             ? CHSV((ms / 20) + (posAbs * 4), 255, 255) 
                             : warnaW;

            CRGB pixelFinal;

            if (bitmask & (1 << s)) {
                // --- SEGMEN ANGKA ---
                if (dist < 0.6) {
                    uint8_t powerSapu = map(dist * 1000, 0, 600, 255, 0);
                    pixelFinal = blend(bgRedup, warnaSapu, powerSapu);
                    
                    // Tambah kilauan di inti sapuan
                    if (dist < 0.15) pixelFinal += CRGB(60, 60, 60); 
                } else {
                    pixelFinal = warnaBg;
                    pixelFinal.nscale8_video(150);
                }
            } else {
                // --- SEGMEN MATI (Tetap kena efek sapuan tipis) ---
                if (dist < 0.4) {
                    uint8_t powerSapu = map(dist * 1000, 0, 400, 80, 0);
                    pixelFinal = blend(bgRedup, warnaSapu, powerSapu);
                } else {
                    // Sangat redup untuk area luar sapuan
                    pixelFinal = warnaBg;
                    pixelFinal.nscale8_video(20);
                }
            }

            // SEKARANG MENGGUNAKAN TARGETPIXEL
            targetPixel[d][idxLocal] = pixelFinal.nscale8_video(brFinal);
        }
    }
}
void efekSolarEclipse(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    CRGB warnaInti = (warnaW == CRGB(0,0,0)) ? CRGB(255, 200, 150) : warnaW;
    CRGB warnaBg = CRGB(konfig.visjam.warna[0]);

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        for (int l = 0; l < jml; l++) {
            if (bitmask & (1 << s)) {
                // --- LOGIKA ANGKA (TAMPILAN OK) ---
                uint8_t pulse = beatsin8(speed / 6, 160, 255, 0, s * 32);
                uint8_t orbit = sin8((ms / (speed + 50)) + (s * 20) + (l * 10));
                CRGB hasil = blend(warnaInti, warnaBg, orbit / 2);
                hasil.nscale8_video(pulse);
                targetPixel[d][offset + l] = hasil.nscale8_video(brFinal);
            } 
            else {
                // --- LOGIKA BACKGROUND (BIAR GAK MATI) ---
                CRGB bgTampil = warnaBg;
                
                // Jangan pakai 20, kita naikkan ke 60 dulu biar "napas" RGB-nya ada
                bgTampil.nscale8_video(60); 
                
                // Tambahkan pengaman: Kalau hasil nscale jadi hitam padahal aslinya ada warna,
                // kita paksa kasih nilai 1 (nyala super redup) biar gak mati total.
                if (bgTampil.r == 0 && warnaBg.r > 0) bgTampil.r = 1;
                if (bgTampil.g == 0 && warnaBg.g > 0) bgTampil.g = 1;
                if (bgTampil.b == 0 && warnaBg.b > 0) bgTampil.b = 1;

                targetPixel[d][offset + l] = bgTampil.nscale8_video(brFinal);
            }
        }
    }
}
void efekBlinkWS(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    
    // 1. Warna Angka (A) & Warna Background (B)
    CRGB warnaA = (warnaW == CRGB(0,0,0)) ? CHSV(ms / 50, 255, 255) : warnaW;
    CRGB warnaB = CRGB(konfig.visjam.warna[0]);
    warnaB.nscale8_video(30);

    // 2. Logiwarnaka Blink Menawan (Square Wave tapi agak smooth)
    // beatsin8 dengan range 0-255 akan membuat efek transisi yang cantik
    uint8_t blinkStep = beatsin8(speed, 0, 255);
    
    // Threshold: Di atas 128 dianggap "Nyala", di bawah itu "Redup ke BG"
    // Ini rahasia biar kedipnya tegas tapi gak bikin pusing
    uint8_t power = (blinkStep > 128) ? 255 : 0;

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        if (bitmask & (1 << s)) {
            // --- AREA ANGKA (BLINKING) ---
            for (int l = 0; l < jml; l++) {
                // Blend dari warnaB ke warnaA
                // Pas power 255 = Warna Angka Full
                // Pas power 0 = Warna Background (tapi di area angka)
                CRGB pixelW = blend(warnaB, warnaA, power);
                targetPixel[d][offset + l] = pixelW.nscale8_video(brFinal);
            }
        } else {
            // --- AREA BACKGROUND (DIAM/ANTENG) ---
            
            for (int l = 0; l < jml; l++) {
                targetPixel[d][offset + l] = warnaB.nscale8_video(brFinal);
            }
        }
    }
}
void efekTimeWarp(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    CRGB warnaUtama = (warnaW == CRGB(0,0,0)) ? CHSV(ms / 40, 255, 255) : warnaW;
    CRGB warnaBg = CRGB(konfig.visjam.warna[0]);
    
    // Warna dasar background yang tetap stay
    CRGB bgAnteng = warnaBg;
    bgAnteng.nscale8_video(40);

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        for (int l = 0; l < jml; l++) {
            // Kita buat gelombang yang lebih "lebar" (slow scan)
            // ms * speed / 1000 buat gerakan yang tenang
            uint8_t wave = beatsin8(speed / 5, 50, 255, 0, (s * 32) + (l * 10));

            CRGB warnaHasil;
            if (bitmask & (1 << s)) {
                // Blend dari BG ke Warna Utama berdasarkan gelombang
                warnaHasil = blend(bgAnteng, warnaUtama, wave);
                
                // Kasih kilauan (highlight) putih tipis pas lagi dipuncak wave
                if (wave > 250) warnaHasil += CRGB(60, 60, 60);
            } else {
                // Background yang tidak aktif tetap punya "aura" bergerak tipis
                warnaHasil = bgAnteng;
                warnaHasil.nscale8_video(map(wave, 50, 255, 100, 150));
            }

            // WAJIB pakai targetPixel biar transisinya "nyess"
            targetPixel[d][offset + l] = warnaHasil.nscale8_video(brFinal);
        }
    }
}

void efekLiquidSmoke(int d, uint8_t bitmask, int speed, int brFinal, CRGB warnaW) {
    uint32_t ms = millis();
    
    // Warna Utama & Background
    CRGB warnaA = (warnaW == CRGB(0,0,0)) ? CHSV(ms / 60, 255, 255) : warnaW;
    CRGB warnaB = CRGB(konfig.visjam.warna[0]);
    
    // Kecepatan sangat pelan biar elegan (Anti-Bus Malam)
    uint32_t slowMS = ms * (speed + 5) / 150;

    for (int s = 0; s < 8; s++) {
        int offset = s * LEDS_PER_SEGMENT;
        int jml = (s == 7) ? LEDS_DP : LEDS_PER_SEGMENT;

        for (int l = 0; l < jml; l++) {
            // RAHASIA: Gabungan dua noise (Sumbu X dan Y)
            // Noise 1: Gerakan horizontal pelan
            uint8_t n1 = inoise8(offset * 20, slowMS);
            // Noise 2: Gerakan vertikal acak
            uint8_t n2 = inoise8(l * 50, slowMS / 2);
            
            // Campur keduanya untuk dapet index tekstur asap
            uint8_t smokeIndex = qadd8(n1, n2);

            CRGB warnaHasil;
            if (bitmask & (1 << s)) {
                // Di area angka, asapnya "terang" (blend dari warnaB ke warnaA)
                // Kita gunakan sin8 untuk bikin transisinya "nyut-nyut" lembut
                uint8_t flow = sin8(smokeIndex);
                warnaHasil = blend(warnaB, warnaA, flow);
                
                // Tambahkan sedikit kedalaman warna (dimming acak)
                warnaHasil.nscale8_video(qadd8(255, flow / 2));
            } else {
                // Background tetap tenang tapi ada aura asap tipis (redup banget)
                warnaHasil = warnaB;
                warnaHasil.nscale8_video(30 + (smokeIndex / 10)); 
            }

            targetPixel[d][offset + l] = warnaHasil.nscale8_video(brFinal);
        }
    }
}
#endif