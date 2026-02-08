// config.js - Perintah Langsung & Operasional Real-time
document.addEventListener('DOMContentLoaded', () => {
    muatJadwalKeWeb();
});
// Data cadangan jika ESP belum mengirim JSON atau file JSON kosong
window.konfig = {
    global: {
        dg: 4 ,    // Default 6 digit
        br: 100,    // Kecerahan maksimal
        ls: 60,     // Loop setiap 60 detik
        ll: 5       // Loop lama 5 detik
    }
};

function muatJadwalKeWeb() {
    fetch('/load?set=jadwal')
        .then(response => response.json())
        .then(data => {
            if (data.jadwal) {
                const j = data.jadwal;
                // ✅ SESUAIKAN DENGAN HASIL JSON BOS:
                document.getElementById('disp-0').innerHTML = `IMSAK<br>${j.Imsak || '--:--'}`;
                document.getElementById('disp-1').innerHTML = `SUBUH<br>${j.Subuh || '--:--'}`;
                document.getElementById('disp-2').innerHTML = `DZUHUR<br>${j.Luhur || '--:--'}`; // Pakai 'Luhur'
                document.getElementById('disp-3').innerHTML = `ASHAR<br>${j.Ashar || '--:--'}`;
                document.getElementById('disp-4').innerHTML = `MAGRIB<br>${j.Magrib || '--:--'}`; // Pakai 'Magrib' (tanpa h)
                document.getElementById('disp-5').innerHTML = `ISYA<br>${j.Isya || '--:--'}`;
                
                //console.log("Jadwal Berhasil Update:", j);
            }
        })
        .catch(err => console.error("Gagal ambil jadwal:", err));
}
function updateJamUI() {
    const display = document.getElementById('jamDigital');
    const bg = document.getElementById('bgJam'); // Sebaiknya ID berbeda untuk background
    if (!display) return;
    
    // Gunakan nilai default jika konfig belum load
    const g = (konfig.global) ? konfig.global : { dg: 4, br: 40, t2k: true };

    const sekarang = new Date();
    const h = sekarang.getHours().toString().padStart(2, '0');
    const m = sekarang.getMinutes().toString().padStart(2, '0');
    const s = sekarang.getSeconds().toString().padStart(2, '0');
    
    // Logika titik: kedip hanya jika t2k true
    const tdk = (typeof g.t2k === 'undefined') ? true : g.t2k;
    const titik = (!tdk || sekarang.getSeconds() % 2 === 0) ? ":" : " ";

    const jmlDigit = parseInt(g.dg);
    
    

    if (jmlDigit === 4) {
        display.innerHTML = h + titik + m;
        
    } else {
        display.innerHTML = h + titik + m + titik + s;
        
    }
}
function sinkronGas() {
    const btn = document.getElementById('btnGas');
    const originalText = btn.innerHTML;
    
    btn.disabled = true;
    btn.innerHTML = '🔄🔄🔄';

    fetch('/cmd?act=sinkron')
        .then(res => res.text())
        .then(txt => {
            alert(res);
            // Tunggu 3 detik baru cek status update terakhir
            setTimeout(ambilStatusSync, 3000);
        })
        .catch(err => alert("Gagal koneksi ke ESP"))
        .finally(() => {
            btn.disabled = false;
            btn.innerHTML = originalText;
        });
}
function showJam() {
    // Mematikan mode cek jadwal dan kembali ke jam normal
    fetch('/cmd?act=showJam')
        .then(response => {
            if(response.ok) {
                console.log("Display kembali ke Jam Normal");
            }
        });
}
async function statuscuaca() {
    console.log("Meminta update data ke BMKG...");
    const status = document.getElementById('c_a').checked;
    const v = status ? 1 : 0;
    
    // 1. Perintah ke webHandler untuk ambil data baru (jika ada act-nya)
    // Atau Bos bisa langsung aktifkan saklarnya
    fetch(`/cmd?act=statuscuaca&v=${v}`)
        .then(() => {
            console.log("Tampilan cuaca diaktifkan!");
        });
    document.getElementById("on_a").checked = false;
}
function setEfekJam(isAktif) {
    let v = isAktif ? "1" : "0";
    let id = document.getElementById("id_a").value;
    
    // Kirim ke act jamAnim yang kita buat di webHandler.cpp tadi
    // v=1 (Animasi Aktif), v=0 (Animasi Mati -> Pakai Warna Statis)
    fetch(`/cmd?act=jamAnim&v=${v}&id=${id}`);
    
    // Opsional: Redupkan pilihan efek kalau lagi OFF biar user gak bingung
    document.getElementById("id_a").style.opacity = isAktif ? "1" : "0.5";
    document.getElementById("c_a").checked = false;
    
}
function showJadwal() {
    // Mematikan mode cek jadwal dan kembali ke jam normal
    fetch('/cmd?act=cekJadwal')
        .then(response => {
            if(response.ok) {
                console.log("Display kembali ke Jam Normal");
            }
        });
}
let timerSimulasi;

function setModeAnimasi(status) {
    // Jika status false, hentikan semua simulasi
    if (!status) {
        clearInterval(timerSimulasi);
        fetch(`/cmd?act=animasi&aktif=0&id=0`); // Kirim sinyal STOP ke alat
        console.log("Simulasi Dihentikan");
        return;
    }

    // Jika status true, mulai RUN ALL (Slot 1 sampai 15)
    let currentSlot = 0;
    const totalSlot = 15;
    
    // Hentikan dulu kalau ada timer yang masih jalan
    clearInterval(timerSimulasi);

    // Jalankan Slot Pertama Seketika
    kirimKeAlat(currentSlot);

    // Jalankan Slot berikutnya setiap 5 detik
    timerSimulasi = setInterval(() => {
        currentSlot++;
        if (currentSlot >= totalSlot) {
            currentSlot = 0; // Ulang lagi dari awal kalau sudah sampai slot 15
        }
        kirimKeAlat(currentSlot);
    }, 5000); 
}

// Fungsi khusus buat nembak ke ESP32
function kirimKeAlat(id) {
    console.log("Simulasi Running Slot: " + (id + 1));
    fetch(`/cmd?act=animasi&aktif=1&id=${id}`)
    .catch(err => console.error("Koneksi ESP32 Putus!"));
}
// Fungsi untuk respon instan (ON/OFF)
function liveUpdateHama() {
    // 1. Ambil status checkbox (true/false)
    const status = document.getElementById('on').checked;
    
    // 2. Ubah jadi angka (1 atau 0) agar sesuai dengan C++ Bos
    const v = status ? 1 : 0;
    
    console.log("Mengirim perintah Hama, v =", v);

    // 3. Kirim ke ESP dengan argumen ?act=setUltra&v=...
    fetch(`/cmd?act=setUltra&v=${v}`)
        .then(response => {
            if(response.ok) {
                console.log("Berhasil: Hama diatur ke " + v);
            }
        })
        .catch(err => {
            console.error("Gagal kirim perintah instan:", err);
        });
}

function titik2kedip() {
    // 1. Ambil status checkbox (true/false)
    const status = document.getElementById('t2k').checked;
    
    // 2. Ubah jadi angka (1 atau 0) agar sesuai dengan C++ Bos
    const v = status ? 1 : 0;
    
    console.log("Mengirim perintah, v =", v);

    // 3. Kirim ke ESP dengan argumen ?act=setUltra&v=...
    fetch(`/cmd?act=titik2&v=${v}`)
        .then(response => {
            if(response.ok) {
                console.log("Berhasil: Hama diatur ke " + v);
            }
        })
        .catch(err => {
            console.error("Gagal kirim perintah instan:", err);
        });
}


// 2. KONTROL KECERAHAN LIVE (br)
let delayBr;
function liveBr(val) {
    document.getElementById('br').innerText = val;
    document.getElementById('val_br').innerText = val;
    
    // Teknik Debounce: Tunggu user berhenti geser baru kirim request
    clearTimeout(delayBr);
    delayBr = setTimeout(() => {
        fetch(`/cmd?act=setBr&v=${val}`).catch(err => console.error("ESP Offline"));
    }, 100); 
}


let isAnimating = false;
async function testEfekKeAlat() {
    // 1. Ambil ID slot dari dropdown (ingat, val > 0 untuk aktif)
    const slotId = parseInt(document.getElementById('slot').value) + 1; // +1 karena biasanya ID mulai dari 1
    const btn = document.getElementById("btnTes");
   
    let action = !isAnimating ? "viewEfek" : "stopEfek";

    // 3. Kirim permintaan ke ESP32
    // URL format: /cmd?act=viewEfek&v=1 atau /cmd?act=stopEfek&v=1
    fetch(`/cmd?act=${action}&v=${slotId}`)
    .then(response => {
        if (response.ok) {
            // 4. Balikkan status jika pengiriman sukses
            isAnimating = !isAnimating;

            // 5. Update UI Tombol
            if (isAnimating) {
                btn.innerText = "STOP EFEK " + slotId;
                btn.style.backgroundColor = "#d9534f"; // Merah (Stop)
                btn.style.boxShadow = "0 0 15px rgba(217, 83, 79, 0.5)"; // Efek Glow
            } else {
                btn.innerText = "LIHAT EFEK " + slotId;
                btn.style.backgroundColor = "#5cb85c"; // Hijau (Lihat)
                btn.style.boxShadow = "none";
            }
            console.log("Berhasil mengirim: " + action);
        } else {
            alert("Gagal konek ke ESP32, Bos!");
        }
    })
    .catch(err => console.error("Error:", err));
}


let skorA = 0;
let skorB = 0;
let babak = 1;

// 1. Fungsi Utama Kirim Perintah (Pakai Gatekeeper/Satpam)
function kirimCmd(params) {
    // Ambil status tombol skor_s (Switch Aktif Papan Skor)
    const isSkorAktif = document.getElementById('skor_s').checked;

    // JANGAN KIRIM kalau switch skor_s mati (kecuali kalau mau matiin via showJam)
    if (!isSkorAktif && !params.includes('act=showJam')) {
        console.log("Perintah ditahan, Mode Skor belum ON!");
        return; 
    }

    fetch(`/cmd?${params}`)
        .then(res => {
            if(res.ok) console.log("Perintah Terkirim: " + params);
        })
        .catch(err => console.error("Koneksi ke ESP Putus!", err));
}

// 2. Kontrol On/Off Papan Skor (Gunakan ID skor_s)
document.getElementById('skor_s').addEventListener('change', function() {
    if (this.checked) {
        // Begitu di ON-kan, langsung kirim data skor saat ini
        kirimCmd(`act=skor&a=${skorA}&b=${skorB}&bk=${babak}`);
    } else {
        // Kalau di OFF-kan, paksa ESP balik ke Jam
        fetch('/cmd?act=showJam'); 
    }
});

// 3. Fungsi Tambah Skor Tetap Sama (Tapi sekarang otomatis lewat satpam kirimCmd)
function setSkor(tim, poin) {
    if(tim === 'a') {
        skorA = Math.max(0, skorA + poin);
        document.getElementById('skor_a').innerText = String(skorA).padStart(2, '0');
    } else {
        skorB = Math.max(0, skorB + poin);
        document.getElementById('skor_b').innerText = String(skorB).padStart(2, '0');
    }
    // Kirim data (Akan dicek oleh kirimCmd apakah skor_s aktif atau tidak)
    kirimCmd(`act=skor&a=${skorA}&b=${skorB}&bk=${babak}`);
}

// 3. Tukar Posisi Skor (Swap)
function swapSkor() {
    let temp = skorA;
    skorA = skorB;
    skorB = temp;
    
    document.getElementById('skor_a').innerText = String(skorA).padStart(2, '0');
    document.getElementById('skor_b').innerText = String(skorB).padStart(2, '0');
    
    kirimCmd(`act=skor&a=${skorA}&b=${skorB}&bk=${babak}`);
}

// 4. Set Babak / Period
function setBabak(val) {
    babak = val;
    // Misal ada elemen UI untuk babak
    if(document.getElementById('label_babak')) {
        document.getElementById('label_babak').innerText = babak;
    }
    kirimCmd(`act=skor&a=${skorA}&b=${skorB}&bk=${babak}`);
}

// 4. PERINTAH SISTEM (Restart, Ganti Mode, dll)
function cmd(action) {
    console.log("Command:", action);
    fetch(`/cmd?act=${action}`)
        .then(res => {
            if(action === 'restart' && res.ok) {
                alert("Perangkat sedang restart...");
                location.reload();
            }
        })
        .catch(err => alert("Koneksi ke ESP terputus"));
}

let frameWeb = 0;

// Variabel global untuk menyimpan data terbaru dari ESP
let configESP = {
    idEfekTes_mode: 0,
    modeEfekAktif: false,
    kecerahan: 255
};



setInterval(updateJamUI, 50);

