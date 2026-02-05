document.addEventListener("DOMContentLoaded", () => {
    muatDaftarFile(); // Load awal

    // Event Listener Upload
    const form = document.getElementById('uploadForm');
    if (form) {
        form.addEventListener('submit', function(e) {
            e.preventDefault();
            uploadFile();
        });
    }
});

// --- 1. AMBIL LIST FILE ---
function muatDaftarFile() {
    fetch('/list-files')
        .then(response => {
            if (response.status === 401) {
                alert("Sesi habis, login ulang.");
                window.location.href = "/"; // Redirect ke login/home
                return [];
            }
            return response.json();
        })
        .then(files => {
            const tbody = document.getElementById('fileListBody');
            tbody.innerHTML = ""; 

            if (!files || files.length === 0) {
                tbody.innerHTML = "<tr><td colspan='3'>Folder Kosong.</td></tr>";
                return;
            }

            // Sortir A-Z
            files.sort((a, b) => (a.name > b.name) ? 1 : -1);

            files.forEach(file => {
                let row = document.createElement('tr');
                
                // Nama
                let cellName = document.createElement('td');
                let cleanName = file.name.startsWith('/') ? file.name.substring(1) : file.name;
                cellName.textContent = cleanName;
                
                // Ukuran
                let cellSize = document.createElement('td');
                cellSize.textContent = formatBytes(file.size);
                
                // Tombol Hapus
                let cellAction = document.createElement('td');
                let btnDel = document.createElement('button');
                btnDel.textContent = "Hapus";
                btnDel.className = "btn-del";
                btnDel.onclick = function() { hapusFile(file.name); };
                
                cellAction.appendChild(btnDel);

                row.appendChild(cellName);
                row.appendChild(cellSize);
                row.appendChild(cellAction);
                tbody.appendChild(row);
            });
        })
        .catch(err => console.error("Gagal load file:", err));
}

// --- 2. UPLOAD FILE ---
function uploadFile() {
    const fileInput = document.getElementById('fileInput');
    const btn = document.getElementById('btnUpload');
    const status = document.getElementById('uploadStatus');

    if (fileInput.files.length === 0) {
        alert("Pilih file dulu!");
        return;
    }

    const file = fileInput.files[0];
    const formData = new FormData();
    formData.append("data", file); 

    btn.disabled = true;
    btn.textContent = "Mengunggah...";
    status.textContent = "⏳ Mengirim " + file.name + "...";
    status.style.color = "black";

    fetch('/upload', {
        method: 'POST',
        body: formData
    })
    .then(response => {
        if (response.status === 200) {
            status.textContent = "✅ Sukses! File tersimpan.";
            status.style.color = "green";
            fileInput.value = ""; 
            muatDaftarFile(); // Refresh tabel
        } else {
            status.textContent = "❌ Gagal Upload. Kode: " + response.status;
            status.style.color = "red";
        }
    })
    .catch(err => {
        console.error(err);
        status.textContent = "⚠️ Error Koneksi.";
        status.style.color = "red";
    })
    .finally(() => {
        btn.disabled = false;
        btn.textContent = "Unggah File";
    });
}

// --- 3. HAPUS FILE ---
function hapusFile(filename) {
    if (!filename.startsWith('/')) filename = '/' + filename;

    if (confirm("Yakin mau menghapus " + filename + "?")) {
        fetch('/delete?file=' + filename)
        .then(response => {
            if (response.ok) {
                muatDaftarFile();
            } else {
                alert("Gagal menghapus file.");
            }
        })
        .catch(err => alert("Error koneksi."));
    }
}

// --- HELPER: FORMAT BYTES ---
function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}
