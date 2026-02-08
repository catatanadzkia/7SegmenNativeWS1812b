// 1. DATABASE LOKAL (CACHE)
let DB = {
    tanggal: {},
    jam :{},
    ultra: {},  // Data Hama
    global: {}, // Data System
    sholat: [],
    efek: [],
    status:{}
};

let konfig = { efek: [] }; 
let slotAktif = 0;
let tempPola = [];
let cacheDataSholat = [{}, {}, {}, {}, {}, {}];

// 2. FUNGSI PINTU UTAMA: AMBIL SEMUA DATA SEKALI JALAN
async function loadSemuaDataGlobal() {
    console.log("📥 Menarik data utuh dari ESP32...");
    try {
        const [tgl, hama, sys, shlt, efk, jamDigital, cuaca] = await Promise.all([
            fetch('/load?set=tanggal').then(r => r.json()),
            fetch('/load?set=ultra').then(r => r.json()),
            fetch('/load?set=system').then(r => r.json()),
            fetch('/load?set=sholat').then(r => r.json()),
            fetch('/load?set=efek').then(r => r.json()),
            fetch('/load?set=jam').then(r => r.json()),
            fetch('/load?set=cuaca').then(r => r.json())
        ]);

        // Simpan ke Cache sesuai struktur config_manajer.cpp
        DB.tanggal = tgl.tanggal;           // Langsung di root
        DB.ultra   = hama.ultra;    // Dari doc["ultra"]
        DB.global  = sys.global;    // Dari doc["global"]
        DB.sholat  = shlt;          // Array 6 slot
        DB.efek    = efk;           // Array 15 slot
        DB.jam     = jamDigital.jam;
        DB.status   = cuaca.status;

        //console.log(DB);
        

        console.log("✅ Data Sinkron. Mendistribusikan ke UI...");
        //console.log(DB.status);
        distribusikanKeForm();


    } catch (err) {
        console.error("❌ Gagal Sinkronisasi Global:", err);
    }
}
// Fungsi pembantu konversi warna (taruh di paling atas JS)
const keHex = (val) => {
    if(!val) return "#ffffff";
    return val.toString().replace("0x", "#");
};

function distribusikanKeForm() {
    // Gabungkan data objek tunggal
    const flatData = { ...DB.tanggal, ...DB.ultra, ...DB.global, ...DB.jam, ...DB.status };

    Object.keys(flatData).forEach(key => {
            // --- PROTEKSI UNTUK ARRAY WARNA JAM (w) ---
        if (key === 'w' && Array.isArray(flatData[key])) {
                flatData[key].forEach((colorVal, index) => {
                    // Sekarang kita panggil ID w0, w1, w2, w3
                    const elWarna = document.getElementById('w' + index); 
                    if (elWarna) {
                        elWarna.value = colorVal.replace("0x", "#");
                    }
                });
                return;
            }

        const el = document.getElementById(key); 
        if (el) {
            if (el.type === 'checkbox') {
                el.checked = !!flatData[key];
            } else if (el.type === 'color') {
                let val = flatData[key];
                el.value = val.replace("0x", "#");
            } else {
                el.value = flatData[key];
                const lbl = document.getElementById('val_' + key);
                if (lbl) lbl.innerText = flatData[key];
            }
        }
    });
    //console.log(flatData);

    // Distribusi lainnya...
    cacheDataSholat = DB.sholat;
    isiFormSholat(document.getElementById('pilih-sholat')?.value || 0);
    konfig.efek = DB.efek;
    loadSlot(0); 
}
// 3. FUNGSI ISI FORM (Tampilkan data ke layar)
function isiFormSholat(idx) {
    // Ambil data dari cache, jika kosong kasih object kosong
    const d = cacheDataSholat[idx] || {};

    // Masukkan ke input (Gunakan '|| 0' agar kalau kosong jadi angka 0)
    document.getElementById('mP').value = d.mP || 0;
    document.getElementById('iEP').value = d.iEP || 0;
    document.getElementById('cDown').value     = d.cDown || 0;
    document.getElementById('durA').value      = d.durA || 0;
    document.getElementById('iEA').value      = d.iEA || 0;
    document.getElementById('mH').value       = d.mH || 0;
    document.getElementById('brH').value   = d.brH || 0;
    document.getElementById('valHemat').innerText     = d.brH || 0;
    document.getElementById('clP').value = keHex(d.clP);
    document.getElementById('clA').value = keHex(d.clA);
    document.getElementById('clH').value = keHex(d.clH);
    document.getElementById('clC').value = keHex(d.clC);
}

// 4. HANDLER GANTI DROPDOWN
function gantiDataSholat(val) {
    isiFormSholat(val);
}

// 5. FUNGSI SIMPAN (Kirim Data)
function simpanNotifSholat() {
    // 1. Ambil ID slot yang sedang dipilih di dropdown (0-5)
    const idx = parseInt(document.getElementById('pilih-sholat').value);
    
    // 2. Susun objek TUNGGAL. Kita tambahkan properti "id" agar C++ tahu slot mana yang diupdate.
    const dataBaru = {
        id:    idx, // PENTING: Penanda slot untuk C++
        mP:    parseInt(document.getElementById('mP').value) || 0,
        iEP:   parseInt(document.getElementById('iEP').value) || 0,
        cDown: parseInt(document.getElementById('cDown').value) || 0,
        durA:  parseInt(document.getElementById('durA').value) || 0,
        iEA:   parseInt(document.getElementById('iEA').value) || 0,
        mH:    parseInt(document.getElementById('mH').value) || 0,
        brH:   parseInt(document.getElementById('brH').value) || 0,
        // Kirim warna apa adanya (0x...), C++ kita sudah bisa handle strtoul
        clP:   document.getElementById('clP').value.replace("#", "0x"),
        clA:   document.getElementById('clA').value.replace("#", "0x"),
        clH:   document.getElementById('clH').value.replace("#", "0x"),
        clC:   document.getElementById('clC').value.replace("#", "0x")
    };

    // 3. Update Cache Lokal (Agar jika user ganti dropdown lalu balik lagi, datanya sudah berubah)
    cacheDataSholat[idx] = dataBaru;

    console.log("📤 Mengirim data TUNGGAL Sholat ID:", idx, dataBaru);

    // 4. Kirim dataBaru ke ESP (Bukan lagi kirim seluruh array cacheDataSholat)
    fetch('/save?set=sholat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(dataBaru) // <--- Sekarang kirim objek tunggal, lebih irit!
    })
    .then(res => {
        if(res.ok) alert("Data Sholat Berhasil Disimpan & Disinkronkan!");
        else alert("Gagal Simpan ke ESP!");
    })
    .catch(err => alert("Koneksi Error: " + err));
}


function simpanTanggal() {
    const data = {
        tanggal : {
            // Key harus SAMA PERSIS dengan yang di C++ (doc["..."])
            a:    document.getElementById('a').checked,      // bool
            dtke: parseInt(document.getElementById('dtke').value) || 0,
            lma:  parseInt(document.getElementById('lma').value) || 0,
            ft:   parseInt(document.getElementById('ft').value) || 0,
            
            ev:   document.getElementById('ev').checked,     // bool
            id:   parseInt(document.getElementById('id').value) || 0,
            dpt:   parseInt(document.getElementById('dpt').value) || 0,

            // --- TAMBAHAN WARNA (Sesuai ID di Struct & Save C++) ---
            clAg: document.getElementById('clAg').value.replace("#", "0x"),
            clNr: document.getElementById('clNr').value.replace("#", "0x"),
            clLb: document.getElementById('clLb').value.replace("#", "0x")
        }
    };

    fetch('/save?set=tanggal', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    })
    .then(res => {
        if(res.ok) alert("Data Tanggal Tersimpan Bos!");
        else alert("Gagal Simpan ke ESP!");
    })
    .catch(err => alert("Gagal simpan tanggal: " + err));
}
async function simpanSystem(e) {
    e.preventDefault();
    const btn = e.target;
    const form = document.querySelector('#formSystem'); // Pakai query selector

    // Ambil semua input di dalam form
    const inputs = form.querySelectorAll('input, select');
    const dataObj = {};

    // Ambil data satu-satu berdasarkan name
    inputs.forEach(input => {
        let value = input.value;
        
        // Konversi otomatis jika tipe inputnya number/range agar jadi Integer di JSON
        if (input.type === 'number' || input.type === 'range' || input.tagName.toLowerCase() === 'select') {
            value = parseInt(value);
        }
        // Khusus untuk t2k jika ada (boolean)
        if (input.name === 't2k') {
            value = (value === 'true' || value === '1');
        }

        dataObj[input.name] = value;
    });

    // BUNGKUS KE DALAM 'global' SESUAI FORMAT ESP32 BOS
    const payload = {
        global: dataObj
    };

    btn.ariaBusy = "true";
    btn.disabled = true;

    try {
        const res = await fetch('/save?set=system', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload) // Sekarang isinya {"global": {...}}
        });
        console.log(payload);
        const hasil = await res.json();
        alert(hasil.msg || "Data Berhasil Disimpan!");
    } catch (err) {
        alert("Gagal konek ke ESP32 atau JSON Error");
    } finally {
        btn.ariaBusy = "false";
        btn.disabled = false;
    }
}
function simpanHama() {
    // Pastikan ID di HTML kamu adalah: on, fMin, fMax, int, malam
    const payload = {
        ultra: { // WAJIB dibungkus "ultra" sesuai config_manajer.cpp
            on:    document.getElementById('on').checked,
            fMin:  parseInt(document.getElementById('fMin').value) || 20000, // Paksa jadi INT
            fMax:  parseInt(document.getElementById('fMax').value) || 40000, // Paksa jadi INT
            int:   parseInt(document.getElementById('int').value) || 600000,  // Paksa jadi INT
            malam: document.getElementById('malam').checked
        }
    };

    // Kirim data ini ke ESP
    fetch('/save?set=ultra', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => {
        if(res.ok) alert("Sip Bos! Data Hama Terupdate.");
    });
}
function simpanKonfigJam() {
    // Ambil semua data dari input
    let data = {
    jam: {
        w: [
            document.getElementById("w0").value.replace("#", "0x")
        ],
        on_a: document.getElementById("on_a").checked,
        id_a: parseInt(document.getElementById("id_a").value)
    }
};
    // Kirim data ke ESP32
    fetch("/save?set=jam", { // Sesuaikan URL ini dengan handler di webHandler.cpp Bos
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data)
    })
    .then(response => {
        if(response.ok) {
            alert("Setelan Jam Berhasil Disimpan Permanen Bos!");
        } else {
            alert("Gagal Simpan, Cek Koneksi!");
        }
    });
}
// ========================================
// FUNGSI SIMPAN EFEK (PER SLOT) - ENDPOINT BARU!
// ========================================

function simpanKeObjekLokal() {
    if (typeof slotAktif === 'undefined') return;

    const polaSegmen = tempPola.length ? [...tempPola] : [0];
// --- KOREKSI 1: Konversi Warna HEX ke Integer ---
    // ESP32 lebih suka angka (16711680) daripada string ("0xff0000")
    const warnaHex = document.getElementById("c").value.replace("#", "0x");
    const elemenDP = document.getElementById('dp');
    const statusDP = (elemenDP && elemenDP.checked) ? 1 : 0;
    const warnaInt = parseInt(warnaHex, 16);

    // --- KOREKSI 2: Pastikan data bertipe Number/Integer ---
    konfig.efek[slotAktif] = {
        id: slotAktif + 1,
        m:  parseInt(document.getElementById('m')?.value || 1),
        s:  parseInt(document.getElementById('s')?.value || 150),
        b:  parseInt(document.getElementById('b')?.value || 200),
        // ESP32 (ArduinoJson) lebih stabil menerima 1/0 daripada true/false
        dp: statusDP, 
        c : warnaHex, 
        f:  polaSegmen.length,
        p:  polaSegmen
    };
    alert("berhasil disimpan");
    kirimSemuaEfekKeESP();
    console.log(`🧠 Efek slot ${slotAktif} disiapkan`, konfig.efek[slotAktif]);
}
function kirimSemuaEfekKeESP() {
    console.log("📤 Mengirim data efek slot aktif ke ESP32...");

    const payload = konfig.efek[slotAktif];
    fetch('/save?set=efek', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload) 
    })
    .then(response => {
        if(response.ok) alert("✅ Animasi " + (slotAktif + 1) + " Tersimpan di ESP!");
        else alert("❌ ESP Menolak Data (Bad Request)");
    })
    .catch(err => {
        alert("❌ Gagal Simpan! Pastikan WiFi terhubung.");
        console.error("Fetch Error:", err);
    });
}

// ========================================
// FUNGSI LOAD SLOT EFEK (Untuk Edit)
// ========================================

function loadSlot(idx) {
    slotAktif = parseInt(idx);
    let d = konfig.efek[slotAktif];
    
    if (!d) return;

    document.getElementById('m').value = String(d.m || 1);
    document.getElementById('s').value = d.s || 150;
    document.getElementById('b').value = d.b || 200;
    document.getElementById('dp').checked = !!d.dp;
    
    // PERBAIKAN DI SINI:
    if(document.getElementById("c")) {
        document.getElementById("c").value = keHex(d.c);
    }
    
    tempPola = d.p ? [...d.p] : [0];
    renderPreviewFrames();
    
    document.querySelectorAll('.seg').forEach(s => {
        s.classList.remove('active');
        s.style.backgroundColor = ""; 
    });
}

// ========================================
// FUNGSI TAMBAH FRAME (Painter)
// ========================================

function tambahFrame() {
    let mask = 0;
    const segmenAktif = document.querySelectorAll('.seg.active');
    
    segmenAktif.forEach(s => {
        let nilaiBit = parseInt(s.getAttribute('data-bit'));
        mask |= nilaiBit;
    });

    console.log("Segmen Aktif:", segmenAktif.length, "Nilai Mask:", mask);

    
    if (tempPola.length >= 8) {
        alert("⚠️ Maksimal 8 frame per efek!");
        return;
    }
    
    tempPola.push(mask);
    renderPreviewFrames();
    
    // Reset painter setelah tambah
    document.querySelectorAll('.seg').forEach(s => {
        s.classList.remove('bg-danger', 'active');
        s.classList.add('bg-secondary');
    });
}


// ========================================
// FUNGSI RENDER PREVIEW FRAMES
// ========================================

function renderPreviewFrames() {
    const con = document.getElementById('previewContainer');
    if (!con) return;

    if (tempPola.length === 0) {
        con.innerHTML = '<div class="w-100 text-center py-2 text-muted small"><i>Belum ada frame...</i></div>';
        return;
    }

    con.innerHTML = tempPola.map((p, i) => `
        <div class="frame-item" onclick="hapusFrame(${i})" title="Klik untuk hapus">
            <div class="frame-head">F${i + 1}</div>
            <div class="frame-body">0x${p.toString(16).toUpperCase().padStart(2, '0')}</div>
        </div>
    `).join('');
}


// ========================================
// FUNGSI HAPUS FRAME
// ========================================

function hapusFrame(idx) {
    tempPola.splice(idx, 1);
    renderPreviewFrames();
}

// ========================================
// INISIALISASI EDITOR EFEK
// ========================================


function initEfekEditor() {
    // Isi dropdown slot jika kosong
    const sel = document.getElementById('slot');
    if(sel && sel.options.length === 0) {
        for(let i=0; i<15; i++) {
            let opt = document.createElement('option');
            opt.value = i;
            opt.innerHTML = "Animasi " + (i+1);
            sel.appendChild(opt);
        }
    }
    
    // Event klik segmen painter
    document.querySelectorAll('.seg').forEach(s => {
        s.onclick = function() {
            // Kita ganti logikanya pakai toggle class 'active'
            this.classList.toggle('active');
            console.log("Bit diklik:", this.getAttribute('data-bit'));
        };
    });
}
function pilihanimasi() {
    // Cari semua elemen select yang punya atribut 'data-slot'
    document.querySelectorAll('select[data-slot]').forEach(sel => {
        // Biar nggak ngisi berulang-ulang
        if(sel.options.length === 0) {
            // Ambil jumlah animasi dari atribut, kalau nggak ada default 15
            let jumlah = sel.getAttribute('data-slot') || 15; 
            
            for(let i = 0; i < jumlah; i++) {
                let opt = document.createElement('option');
                opt.value = i + 1;
                opt.innerHTML = "Animasi " + (i + 1);
                sel.appendChild(opt);
            }
        }
    });
}