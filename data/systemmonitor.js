// systemmonitor.js - Master Data Controller
window.addEventListener('DOMContentLoaded', async () => {
    console.log("🚀 Inisialisasi Master Data...");

    // 1. Cek Koneksi & Navigasi Dulu
    await cekStatusKoneksi(); 

    // 2. Ambil Data Konfigurasi secara Berurutan (Sequential)
    // Ini menjamin data loadSholat dkk tampil di layar saat refresh
    try {
        console.log("📥 Mengunduh data konfigurasi...");
        
        // Kita panggil fungsi yang ada di main.js
        loadSemuaDataGlobal();
        pilihanimasi();
        // Inisialisasi visual editor
        initEfekEditor();
        loadSlot(0); 

        console.log("✅ Semua data sinkron.");
    } catch (err) {
        console.error("Gagal sinkronisasi data awal:", err);
    }

    // 3. Event Listener & Scan Wifi
    const formWifi = document.getElementById('formWifi');
    if (formWifi) {
        formWifi.addEventListener('submit', fungsiSimpanWifi);
    }
    fungsiScanWifi();

    // 4. Monitoring Berkala
    setInterval(monitorKesehatanESP, 5000);
    setInterval(ambilStatusSync, 50000);
});
async function cekStatusKoneksi() {
    try {
        const res = await fetch('/wifi?mode=status'); //
        const status = await res.text(); //
        if (status === "1") {
            navigasi('dashboard');
            monitorKesehatanESP();
        } else {
            navigasi('wifi');
        }
    } catch (err) {
         navigasi('wifi'); // Jika gagal akses, asumsikan harus setting WiFi
    }
}

async function navigasi(menu) {
    const wadah = document.getElementById('konten-utama');
    const temp = document.getElementById('view-' + menu)
    
    if (menu === 'wifi') {
        wadah.innerHTML = temp.innerHTML; // Isi diganti WiFi, Dashboard terhapus dari DevTools
        fungsiScanWifi();
    } else if (menu === 'dashboard'){
        wadah.innerHTML = temp.innerHTML; // Isi diganti Dashboard, WiFi terhapus dari DevTools
    }
}

// --- FUNGSI SCAN (Auto) ---
// --- FUNGSI 1: SCAN WIFI & KLIK KE FORM ---
async function fungsiScanWifi() {
    const dropdown = document.getElementById('pilih-wifi');
    const inputSSID = document.getElementById('ssid');
    const inputPass = document.getElementById('pass');
    
    // Tombol refresh manual (opsional, biar user tau lagi kerja)
    const labelStatus = document.getElementById('status-scan'); // Misal ada label kecil

    if (!dropdown) return;

    // ❌ JANGAN LAKUKAN INI LAGI:
    // dropdown.innerHTML = "<option>Scanning...</option>"; 
    // ^^^ Ini yang bikin hilang/berkedip! Biarkan list lama tetap ada dulu.

    if(labelStatus) labelStatus.textContent = "Sedang memindai...";

    const lakukanScan = () => {
        fetch('/wifi?mode=scan')
            .then(response => {
                // Kalau status 202, berarti lagi proses. Tunggu aja.
                if (response.status === 202) {
                    setTimeout(lakukanScan, 1000); 
                    return null;
                }
                return response.json();
            })
            .then(data => {
                if (!data) return;

                // ✅ KUNCI PERBAIKAN DI SINI:
                // Cuma update dropdown KALAU datanya ada isinya.
                // Kalau kosong (karena error/glitch), list lama tetep aman.
                
                if (data.length > 0) {
                    // Baru kita bersihkan list lama
                    dropdown.innerHTML = ""; 

                    // Tambah Opsi Default
                    let defaultOpt = document.createElement('option');
                    defaultOpt.value = "";
                    defaultOpt.textContent = "-- Pilih WiFi Baru --";
                    dropdown.appendChild(defaultOpt);

                    // Masukkan data baru
                    data.forEach(wifi => {
                        let opt = document.createElement('option');
                        opt.value = wifi.ssid;
                        opt.textContent = `${wifi.ssid} (${wifi.rssi}dB)`;
                        dropdown.appendChild(opt);
                    });
                    
                    if(labelStatus) labelStatus.textContent = "Scan Selesai ✅";
                    
                    // Pasang event listener lagi
                    dropdown.onchange = function() {
                        if (this.value !== "") {
                            inputSSID.value = this.value;
                            inputPass.value = "";
                            inputPass.focus();
                        }
                    };
                } 
                else {
                    // Kalau data kosong, mungkin kita perlu coba scan lagi sekali lagi
                    // Tapi jangan hapus tampilan yang ada sekarang
                    console.log("Data kosong, coba lagi...");
                    setTimeout(lakukanScan, 2000);
                }
            })
            .catch(err => {
                console.log("Gagal fetch, coba lagi...", err);
                // Jangan hapus dropdown, cukup coba lagi diam-diam
                setTimeout(lakukanScan, 2000);
            });
    };

    // Jalankan
    lakukanScan();
}
// --- FUNGSI 2: SIMPAN WIFI KE ESP32 ---
async function fungsiSimpanWifi(e) {
    e.preventDefault();
    const form = e.target;
    const btnSimpan = form.querySelector('button[type="submit"]');
    const formData = new FormData(form);
    const teksAsli = btnSimpan.textContent;

    try {
        btnSimpan.disabled = true;
        btnSimpan.textContent = "Cek Nama Alat...";

        // 1. Ambil data hostname dari systemconfig
        // Karena login sudah dihapus, ini pasti langsung tembus
        const resConfig = await fetch('/load?set=system'); 
        const dataConfig = await resConfig.json();
        
        // Ambil hostname, jika tidak ada default ke jamkita
        const namaHost = dataConfig.namaPerangkat || "jam"; 
        const urlTujuan = `http://${namaHost}.local`;

        // 2. Kirim data WiFi ke ESP32
        btnSimpan.textContent = "Menyimpan WiFi...";
        const resSave = await fetch('/wifi?mode=save', {
            method: 'POST',
            body: formData
        });

        // Kita ambil respon teks ("WiFi Disimpan ke NVS...")
        const responTeks = await resSave.text();
        console.log("Respon ESP32:", responTeks);

        // 3. Proses Hitung Mundur untuk Redirect
        let detik = 10;
        
        // Munculkan info ke user
        alert("✅ WiFi Berhasil Disimpan!\nAlat akan restart. Browser akan mencoba membuka: " + urlTujuan);

        const interval = setInterval(() => {
            detik--;
            btnSimpan.textContent = `Pindah ke ${namaHost} (${detik}s)`;
            
            if (detik <= 0) {
                clearInterval(interval);
                // REDIRECT OTOMATIS
                window.location.href = urlTujuan;
            }
        }, 1000);

    } catch (err) {
        // Jika saat restart koneksi putus, biasanya masuk ke sini
        console.log("Koneksi terputus (ESP Restarting...)");
    }
}


async function monitorKesehatanESP() {
    try {
        const res = await fetch('/status');
        if (!res.ok) throw new Error("Server Error");
        const data = await res.json();

        // 1. Update Uptime (Proteksi jika split gagal)
        if (document.getElementById('valUptime')) {
            const upStr = data.uptime || "0j 0m 0d";
            const up = upStr.split(' ');
            // Ambil dua bagian pertama saja agar ringkas
            document.getElementById('valUptime').innerText = (up[0] || '0j') + " " + (up[1] || '0m');
        }

        // 2. Update RAM (Konversi Byte ke KB yang Benar)
        if (document.getElementById('valRAM')) {
            // Jika data.heap dalam Byte, bagi 1024 untuk jadi KB
            let ramKB = data.heap > 1024 ? Math.round(data.heap / 1024) : Math.round(data.heap);
            document.getElementById('valRAM').innerText = ramKB + " KB";
        }

        // 3. Update WiFi (RSSI) + Warna Indikator
        const elWiFi = document.getElementById('valRssi');
        if (elWiFi) {
            const rsSi = data.rssi || 0;
            elWiFi.innerText = rsSi + " dBm";
            
            // Logika warna sinyal
            if (rsSi > -67) elWiFi.style.color = "#20c997";      // Bagus
            else if (rsSi > -80) elWiFi.style.color = "#ffc107"; // Sedang
            else elWiFi.style.color = "#ff4d4d";                // Buruk
        }

        // 4. Update Suhu (Proteksi .toFixed)
        if (document.getElementById('valTemp')) {
            let suhu = parseFloat(data.temp) || 0;
            document.getElementById('valTemp').innerText = suhu.toFixed(1) + "°C";
        }

        // 5. Update LittleFS (Total & Used)
        if (document.getElementById('valFSTotal')) {
            let fsT = Math.round(data.fs_total / 1024) || 0;
            document.getElementById('valFSTotal').innerText = fsT + " KB";
        }
        if (document.getElementById('valFSUsed')) {
            let fsU = Math.round(data.fs_used / 1024) || 0;
            document.getElementById('valFSUsed').innerText = fsU + " KB";
        }

        // 6. Update Flash
        if (document.getElementById('valFlash')) {
            document.getElementById('valFlash').innerText = (data.flash || 4) + " MB";
        }

    } catch (err) {
        console.error("ESP Monitoring Offline:", err);
        // Tanda jika koneksi terputus
        const statusEl = document.getElementById('valRssi');
        if (statusEl) {
            statusEl.innerText = "OFFLINE";
            statusEl.style.color = "gray";
        }
    }
}
async function ambilStatusSync() {
    fetch('/lastsync')
        .then(res => res.text())
        .then(txt => {
            // Pasang di elemen dashboard Bos
            // Contoh: <span id="lastUpdate"></span>
            document.getElementById('lastUpdate').innerText = txt;
        })
        .catch(err => {
            document.getElementById('lastUpdate').innerText = txt;
        });
}
