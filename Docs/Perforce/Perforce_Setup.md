# Panduan Setup Perforce untuk Proyek Zoo

Panduan ini menjelaskan cara memakai **Perforce (Helix Core)** sebagai version control **seluruh** proyek (Source, Config,
Content, dll.) dengan server di **laptop teman**, dan tim tersambung lewat **Tailscale** (tanpa port forwarding).

> **Status panduan:** disusun dari riset dokumentasi resmi + sumber sekunder, lalu diperiksa ulang. Yang sudah dan
> belum terverifikasi dikumpulkan di bagian 11; yang belum ditandai ⚠️. Tampilan installer / nama menu bisa sedikit beda dari tulisan di sini
> (Perforce sedang mengganti nama produk dari "Helix Core" menjadi "P4"). Kalau beda, ikuti logikanya, bukan nama persisnya.

---

## Daftar isi

0. [Gambaran besar](#0-gambaran-besar)
1. [Keputusan yang harus diambil dulu](#1-keputusan-yang-harus-diambil-dulu)
2. [Bagian A - Laptop host (server)](#2-bagian-a---laptop-host-server)
3. [Bagian B - PC kamu: upload pertama](#3-bagian-b---pc-kamu-upload-pertama)
4. [Bagian C - Anggota tim lain](#4-bagian-c---anggota-tim-lain)
5. [Bagian D - Unreal Editor dan Rider](#5-bagian-d---unreal-editor-dan-rider)
6. [Aturan kerja sehari-hari](#6-aturan-kerja-sehari-hari)
7. [Backup (WAJIB)](#7-backup-wajib)
8. [Troubleshooting](#8-troubleshooting)
9. [Pindah host](#9-pindah-host)
10. [Kalau tim membesar](#10-kalau-tim-membesar)
11. [Status verifikasi](#11-status-verifikasi)
12. [Sumber](#12-sumber)

---

## 0. Gambaran besar

```
   PC kamu                         Laptop teman (HOST)                 PC teman lain
  ┌──────────────┐                ┌──────────────────────┐            ┌──────────────┐
  │ D:\...\Zoo   │  ── upload ──► │  Server Perforce     │ ◄─ sync ── │ workspace    │
  │ (workspace)  │ ◄── sync ───── │  (depot "Zoo")       │ ── submit─►│ (folder lokal)│
  └──────────────┘                │  port 1666           │            └──────────────┘
         ▲                        └──────────────────────┘                    ▲
         └──────────────  Tailscale (jaringan privat, alamat 100.x.x.x) ───────┘
```

Intinya:

- **Depot** = "gudang pusat" di laptop host. Ini sumber kebenaran (Source of Truth).
- **Workspace** = folder kerja di PC masing-masing orang. Isinya salinan dari depot.
- Kamu **tidak perlu copy-paste** folder proyek ke laptop host. Kamu *submit* dari PC-mu, Perforce yang mengunggahnya.
- **Tailscale** membuat semua laptop/PC seolah satu jaringan lokal. Tidak perlu buka port di router dan tidak terpengaruh
  ganti WiFi.
- Server cuma bisa dipakai kalau laptop host **menyala dan online**. Kalau mati, kalian masih bisa kerja lokal, tapi
  tidak bisa sync/submit sampai menyala lagi.

### Istilah yang akan sering muncul

| Istilah | Artinya |
|---|---|
| **Depot** | Penyimpanan pusat di server. Isi semua versi semua file. |
| **Workspace** (client) | Folder kerja di PC kamu yang terhubung ke depot. |
| **Sync** (Get Latest) | Ambil versi terbaru dari depot ke workspace. |
| **Check out** (edit) | "Saya mau mengubah file ini." Untuk file binary, orang lain dikunci tidak bisa mengubahnya. |
| **Changelist** | Kumpulan perubahan yang akan dikirim bersama (seperti satu commit). |
| **Submit** | Kirim changelist ke depot. Setelah ini orang lain bisa sync. |
| **Reconcile** | Perforce mendeteksi perubahan yang kamu buat *tanpa* check out (file baru, diubah, dihapus). |
| **Typemap** | Aturan tipe file. Di sini dipakai supaya `.uasset`/`.umap` otomatis "terkunci eksklusif". |
| **P4V** | Aplikasi GUI Perforce (seperti GitHub Desktop-nya). |
| **P4D / P4S** | Program server Perforce (P4S = versi yang jalan sebagai Windows service). |

---

## 1. Keputusan yang harus diambil dulu

Isi tabel ini sebelum mulai. Catat nilainya, karena akan dipakai berkali-kali.

| Keputusan | Rekomendasi | Nilai kalian |
|---|---|---|
| Siapa pemilik laptop host & bertanggung jawab atas server? | Satu orang jelas (bukan "kita semua") | |
| Nama depot | `Zoo` | |
| Folder data server (P4ROOT) | Disk dengan **100 GB+ kosong**, lebih baik SSD. Contoh `D:\PerforceData` | |
| Port | `1666` (default) | |
| Alamat Tailscale host | Diisi setelah Tailscale terpasang, contoh `100.64.12.34` | |
| Nama admin Perforce | Contoh `admin` | |
| Lokasi backup (di luar laptop host!) | Drive eksternal **dan/atau** folder cloud (Google Drive, dll.) | |
| Mode Unicode server | **Tidak** (non-Unicode). ⚠️ Setelah dinyalakan **tidak bisa dimatikan lagi** | |
| Nasib folder `.git` | Lihat bagian 3.7 | |

### ⚠️ Cek lisensi Tailscale

Plan gratis **Personal** Tailscale tertulis "hanya untuk non-commercial use", sedangkan ada pernyataan staf Tailscale di
forum bahwa batasannya soal jumlah user, bukan komersial atau tidak. Aturannya ambigu. Kalau proyek ini untuk
studio/bisnis (bukan hobi atau tugas), **baca dulu Terms of Service Tailscale**. Tailnet yang dibuat dengan email publik
(Gmail dll.) otomatis masuk plan Personal, **maksimal 6 user** (per April 2026).

---

## 2. Bagian A - Laptop host (server)

Semua langkah di bagian ini dikerjakan di **laptop teman** yang jadi host. Gunakan akun Windows yang sama setiap
kali (lihat catatan backup di bagian 7).

### A1. Pasang Tailscale

1. Unduh dan pasang Tailscale dari <https://tailscale.com/download>.
2. Login (disarankan pakai akun Gmail pemilik laptop). Ini membuat **tailnet** (jaringan privat kalian).
3. Catat **alamat Tailscale** laptop ini (`100.x.x.x`) dari aplikasinya atau jalankan di PowerShell:
   ```powershell
   tailscale ip -4
   ```
4. Buka Admin Console (<https://login.tailscale.com/admin/machines>), cari laptop ini, menu `...` →
   **Disable key expiry**. Tanpa ini, koneksi laptop bisa mati sendiri setelah beberapa bulan dan harus login ulang.
5. Undang anggota tim lain: Admin Console → halaman **Users** → **Invite external users**. Bisa lewat email, atau
   salin **invite link** lalu kirim ke teman. Hanya Owner/Admin yang bisa mengundang. Masing-masing harus punya akun
   sendiri (Gmail cukup) dan menerima undangan.

> **Ganti WiFi?** Tidak perlu setup ulang apa pun. Tailscale otomatis menyambung lagi dan alamat `100.x.x.x` tetap sama.
> Syaratnya cuma: laptop online dan aplikasi Tailscale berjalan.

### A2. Pasang server Perforce

1. Buka <https://www.perforce.com/downloads> dan unduh **Helix Core Server (P4D) untuk Windows** (nama bisa sudah jadi
   "P4 Server"). Unduh juga **P4V** dan **P4Admin** kalau terpisah.
2. Jalankan installer. Hal yang perlu dipilih/diisi:
   Menurut dokumentasi Perforce, installer server menanyakan:
   - **Fitur:** *Server (P4D)* dan *Command-Line Client (P4)*. P4V (GUI) dan P4Admin kemungkinan dipasang dari
     installer terpisah di halaman download (⚠️ belum terverifikasi); pasang juga.
   - **Port:** `1666` (default).
   - **Lokasi server (P4ROOT):** folder data, misalnya `D:\PerforceData` (jangan di tempat yang nyaris penuh).
   - **Konfigurasi klien:** nama mesin, port, dan **username** yang ingin dipakai. Isi dengan nama admin kalian.
   - Soal **Unicode mode**: dokumentasi yang kubaca tidak menyebutnya di installer Windows. Jangan aktifkan
     (lihat bagian 1). Kalau ada pilihannya, biarkan nonaktif.
3. Installer memasang server sebagai **Windows service**, jadi otomatis hidup lagi setelah laptop restart. Cek:
   `Win + R` → `services.msc` → cari **Perforce** → pastikan *Status = Running* dan *Startup type = Automatic*.
4. Server di Windows secara default **case-insensitive** (huruf besar/kecil nama file dianggap sama). Ini memang yang
   disarankan Epic untuk UE.
5. **Lisensi:** tanpa file lisensi, server gratis untuk **5 user dan 20 workspace**. Lewat itu, server dibatasi 1.000 file.

### A3. Konfigurasi awal server

Buka PowerShell di laptop host. Ganti `admin` dengan nama admin kalian. Perintah `p4` ini bicara ke server di laptop
itu sendiri (`127.0.0.1`).

```powershell
$env:P4PORT = "127.0.0.1:1666"
$env:P4USER = "admin"
```

**1. Atur password admin dan login**

```powershell
p4 passwd          # buat password admin (kalau belum ada)
p4 login
```

**2. Naikkan level keamanan** (server nanti dijangkau lewat jaringan, jadi jangan dibiarkan tanpa password)

```powershell
p4 configure set security=2
```

Level 2 mewajibkan password yang kuat (default minimal 8 karakter; bisa diubah lewat `dm.password.minlength`).
**Jangan password mengandung spasi** (Perforce tidak mengizinkannya).

> **Kenapa bukan level 3 atau 4?** Di level 3 dan 4 Perforce **mewajibkan login berbasis ticket** (tidak menerima password
> langsung). Ada laporan lama di forum Unreal bahwa Unreal Editor gagal konek di level ini dan baru bisa setelah
> login dulu lewat P4V. Level 2 aman untuk UE dan cukup untuk tim kecil lewat Tailscale (server tidak terbuka ke
> internet). Naikkan hanya kalau sudah dites dengan editor.

**3. Kunci hak akses dengan `p4 protect` (lakukan SEGERA, sebelum membuat user lain)**

Menurut dokumentasi Perforce, **sebelum `p4 protect` pertama kali dijalankan, setiap user adalah superuser** dan bisa
mengakses/mengubah apa pun. Saat pertama dijalankan, tabel hak akses dibuat: user yang menjalankannya jadi superuser, dan
user lain turun menjadi `write`. Jadi jalankan sebagai **admin**, segera setelah login:

```powershell
p4 protect        # editor terbuka; baca isinya, lalu simpan tanpa mengubah
```

Hasilnya kira-kira: admin = `super`, semua user lain = `write`. Itu pas untuk tim: bisa sync, check out, submit, tapi
tidak bisa mengubah konfigurasi server. (Untuk tim kecil yang saling percaya ini cukup.)

> Kalau `p4 protect` pertama dijalankan oleh orang yang salah, dokumentasi menyebut tabelnya bisa dikembalikan ke
> kondisi awal dengan menghapus file `db.protect` di P4ROOT. Itu sebabnya langkah ini dikerjakan admin sendiri,
> sebelum akun lain dibuat.

**4. Pasang Typemap (WAJIB sebelum file apa pun ditambahkan)**

Typemap menentukan file `.uasset`, `.umap`, dll. sebagai `binary+l`: binary dengan **exclusive lock** (hanya satu orang
yang boleh meng-edit pada satu waktu, karena file binary tidak bisa digabung).

```powershell
p4 typemap
```

Editor teks terbuka. Ganti isinya menjadi (pertahankan baris `TypeMap:` di atas, dan indentasinya pakai **TAB**):

```
TypeMap:
	binary+l //....uasset
	binary+l //....umap
	binary+l //....upk
	binary+l //....udk
	binary+l //....ubulk
```

Simpan dan tutup. Entri ini sesuai dokumentasi Epic.

> **Kenapa sebelum add?** File yang sudah terlanjur ditambahkan dengan tipe salah tidak otomatis berubah. Kalau
> terlanjur, perbaiki dengan `p4 reopen -t binary+l <file>` sebelum submit.

**5. Buat depot**

```powershell
p4 depot Zoo
```

Di editor yang terbuka, biarkan `Type: local`, simpan. Depot `Zoo` sekarang ada (kosong).

**6. Buat user untuk anggota tim**

Satu user per orang (jangan berbagi akun):

```powershell
p4 user -f andi        # simpan; ulangi untuk tiap orang (isi Email & FullName)
p4 passwd andi         # beri password awal, minta orang itu menggantinya
```

Ingat batas gratis: **maksimal 5 user**. Jangan hitung akun backup nanti sebagai "cadangan"; setiap user terdaftar
dihitung.

**7. Cek**

```powershell
p4 info
p4 users
```

### A4. Firewall Windows

Server di port 1666 jangan terbuka ke seluruh internet. Batasi hanya untuk jaringan Tailscale (rentang `100.64.0.0/10`).
Jalankan PowerShell **sebagai Administrator**:

```powershell
New-NetFirewallRule -DisplayName "Perforce via Tailscale" `
  -Direction Inbound -Protocol TCP -LocalPort 1666 `
  -RemoteAddress 100.64.0.0/10 -Action Allow
```

**Periksa apakah ada aturan lain yang membuka port 1666 ke semua alamat.** Dokumentasi instalasi Windows Perforce
tidak menyebut firewall sama sekali, jadi tidak diketahui apakah installer atau Windows membuat aturan sendiri
(Windows kadang membuat aturan berbasis program untuk aplikasi yang mendengarkan jaringan). Cek langsung di laptop
host, PowerShell **sebagai Administrator**:

```powershell
Get-NetFirewallRule -Direction Inbound -Enabled True -Action Allow |
  ForEach-Object {
    $p = $_ | Get-NetFirewallPortFilter
    $a = $_ | Get-NetFirewallApplicationFilter
    $r = $_ | Get-NetFirewallAddressFilter
    if ($p.LocalPort -contains "1666" -or $a.Program -like "*p4d*") {
      [pscustomobject]@{ Name = $_.DisplayName; Program = $a.Program; Port = ($p.LocalPort -join ","); Remote = ($r.RemoteAddress -join ","); Profile = $_.Profile }
    }
  } | Format-Table -AutoSize
```

Cara membaca hasilnya:
- Aturan **"Perforce via Tailscale"** yang kamu buat tadi harus muncul dengan `Remote = 100.64.0.0/10`. Itu benar.
- Aturan **lain** yang muncul dengan `Remote = Any` (atau `*`) berarti port/program Perforce terbuka untuk semua
  alamat. Nonaktifkan: `Disable-NetFirewallRule -DisplayName "<nama aturan>"`.
- Kalau hanya aturan Tailscale yang muncul, atau tidak ada yang lain, sudah aman.

(Skrip pemeriksaan ini sudah dijalankan untuk uji sintaks di PC lain tanpa Perforce dan berjalan tanpa error. Hasil pada
laptop host belum diketahui karena belum diuji di sana.)

Intinya: batasan `RemoteAddress` ke rentang Tailscale di firewall adalah lapis pengaman jaringannya, jadi pastikan
tidak ada aturan lain yang membukanya ke semua alamat.

Selain itu, **jangan** buat port forwarding apa pun di router. Tidak diperlukan.

### A5. Supaya laptop tidak tidur

Laptop yang tidur = server mati. Jalankan PowerShell sebagai Administrator:

```powershell
powercfg /change standby-timeout-ac 0       # tidak sleep saat dicolok listrik
powercfg /change hibernate-timeout-ac 0     # tidak hibernate saat dicolok listrik
# Tutup layar laptop = tidak ngapa-ngapain (saat dicolok)
powercfg /setacvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION 0
powercfg /setactive SCHEME_CURRENT
```

Tambahan:
- Selama jam kerja tim, **biarkan laptop tercolok listrik**.
- Windows Update bisa me-restart sendiri. Atur **Active hours** di Settings → Windows Update → Advanced options. Service
  Perforce otomatis hidup lagi setelah restart, tapi selama restart tim tidak bisa konek.
- Sepakati jam server menyala (misalnya "10.00-23.00") dan beri tahu tim kalau laptop mau dimatikan.

### A6. Tes dari laptop host

```powershell
p4 -p 127.0.0.1:1666 -u admin info
```

Kalau muncul info server (Server address, Server version, dll.), server sudah jalan.

---

## 3. Bagian B - PC kamu: upload pertama

Bagian ini dikerjakan di **PC kamu** (tempat folder `D:\UEProjects\Zoo` berada). Folder proyek tetap di tempatnya.

### B1. Pasang Tailscale dan P4V

1. Pasang Tailscale di PC-mu, login dengan akun yang sudah diundang (bagian A1 langkah 5).
2. Tes koneksi ke host:
   ```powershell
   tailscale ping <IP_HOST>
   ```
   Kalau membalas, jalur sudah tembus.
3. Pasang **P4V** dan command-line **p4** dari <https://www.perforce.com/downloads>.

### B2. Konek ke server

Di P4V: **Connection → Open Connection**:

| Field | Isi |
|---|---|
| Server | `<IP_HOST>:1666` (contoh `100.64.12.34:1666`) |
| User | nama user kamu |

Masukkan password. Setelah masuk, depot `Zoo` terlihat (masih kosong).

### B3. Buat workspace di folder proyek

Di P4V: **View → Workspaces → New Workspace** (atau **Connection → New Workspace**).

| Field | Isi |
|---|---|
| Workspace name | `zoo_<nama>_pc`, contoh `zoo_andi_pc` |
| Workspace root | `D:\UEProjects\Zoo` |
| Workspace mappings | `//Zoo/...  //zoo_andi_pc/...` |

Simpan. Sekarang folder proyek kamu menjadi **workspace** yang terhubung ke depot `Zoo`.

### B4. Buat file `.p4ignore`

File ini memberi tahu Perforce file mana yang **tidak boleh** masuk depot (file hasil generate yang besar dan bisa dibuat
ulang). Buat file `D:\UEProjects\Zoo\.p4ignore` dengan isi:

```
# Hasil build/generate UE (dibuat ulang tiap orang)
Binaries
Intermediate
Saved
DerivedDataCache

# IDE / solution (dibuat ulang dari .uproject)
.vs
.idea
*.sln
*.suo
*.VC.db
*.opensdf
*.sdf
*.opendb
*.xcodeproj
*.xcworkspace

# Git lama (lihat bagian 3.7)
.git

# Python
__pycache__
*.pyc
```

Beri tahu Perforce nama file ignore-nya (sekali saja per PC):

```powershell
p4 set P4IGNORE=.p4ignore
```

Aturan sintaks `.p4ignore` (dari dokumentasi Perforce):
- Baris diawali `#` = komentar.
- `/` di **awal** = relatif terhadap lokasi file `.p4ignore` (hanya cocok di folder itu). Tanpa `/` di awal, aturan
  berlaku di kedalaman folder mana pun.
- `/` di **akhir** = hanya cocok untuk folder, bukan file.
- `*` cocok dengan teks apa pun **kecuali** pemisah folder. `**` cocok termasuk pemisah folder.
- `!` di awal = pengecualian.

> **Tambahan khusus proyek ini:** `.gitignore` lama mengecualikan `Tools/**/out*/` (output generator seperti heightmap
> dan texture). Kalau kamu tidak mau output itu ikut masuk depot, tambahkan baris `Tools/**/out*/` ke `.p4ignore`
> (sesuaikan polanya). Pola ini **belum diuji** di proyekmu, jadi cek lewat preview di langkah B5.

### B5. Preview apa yang akan masuk (jangan lewati)

Buka PowerShell di folder proyek, atur koneksi, lalu **preview** (`-n` = hanya simulasi, tidak mengubah apa-apa):

```powershell
cd D:\UEProjects\Zoo
$env:P4PORT = "<IP_HOST>:1666"
$env:P4USER = "andi"
$env:P4CLIENT = "zoo_andi_pc"
p4 login

p4 reconcile -n ... | Select-Object -First 50      # lihat 50 baris pertama
(p4 reconcile -n ...).Count                          # total file
```

Periksa:
- Tidak ada `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.git/` di daftar.
- Ada `Source/`, `Config/`, `Content/`, `Zoo.uproject`, `Docs/`, `Tools/`.
- File `.uasset`/`.umap` terlihat tipenya `binary+l` (cek satu per satu dengan `p4 fstat` setelah add, atau lihat kolom
  *Type* di P4V).

Kalau ada yang tidak diinginkan, tambahkan ke `.p4ignore` dan ulangi preview.

### B6. Upload bertahap (submit pertama)

Total ~20 GB lewat internet bisa lama dan kalau putus di tengah harus mengulang. **Pecah jadi beberapa submit.**

**Tahap 1 - file kecil dulu (code, config, dokumen):**

```powershell
p4 reconcile Source/... Config/... Docs/... Tools/... Zoo.uproject .p4ignore
p4 submit -d "Import awal: Source, Config, Docs, Tools, uproject"
```

**Tahap 2 - Content per sub-folder.** Lihat dulu isi `Content\`, lalu satu per satu:

```powershell
p4 reconcile Content/NamaFolderA/...
p4 submit -d "Import Content/NamaFolderA"

p4 reconcile Content/NamaFolderB/...
p4 submit -d "Import Content/NamaFolderB"
# ...dst. sampai semua folder Content terkirim
```

Tips:
- Folder yang sangat besar (misalnya tekstur) bisa dipecah lagi per sub-folder.
- **Kalau submit gagal/terputus:** Perforce **tidak** otomatis melanjutkan. File tetap ada di sebuah *numbered pending
  changelist* (kalau tadinya di default changelist, ia diberi nomor baru). Cari nomornya dengan
  `p4 changes -s pending -u <user>`, perbaiki masalahnya, lalu submit ulang **berdasarkan nomor**:
  `p4 submit -c <nomor>`.
- Secara default, submit ulang mengirim **semua file lagi dari awal**. Supaya Perforce mencoba mendeteksi file yang
  sudah terkirim dan tidak mengirim ulang, aktifkan sekali di server (di laptop host, sebagai admin):
  ```powershell
  p4 configure set submit.noretransfer=1
  ```
  Dengan nilai `1`, Perforce membandingkan digest file untuk menghindari transfer ulang saat submit diulang
  (sesuai dokumentasi `p4 submit`).
- Alasan lain memecah submit: ada laporan pengguna di forum Unreal bahwa submit besar campuran asset gagal dengan
  "Submit aborted" dan berhasil setelah dipecah. Penyebabnya tidak dijelaskan di sana, jadi anggap ini praktik hati-hati,
  bukan fakta teknis.
- Lakukan saat laptop host dan PC-mu **stabil online** dan keduanya **tidak sleep**. Idealnya jangan bersamaan dengan
  kerja berat lain.
- Kalau bisa ketemu langsung: submit pertama di **WiFi/LAN yang sama** jauh lebih cepat. Alternatif: bawa PC-mu ke host
  dan submit dari sana.
- Pantau progres di P4V (Submitted Changelists) atau `p4 changes -m 5`.

Selesai tahap 2, cek apakah ada yang tertinggal:

```powershell
p4 reconcile -n ...     # kalau hasilnya "no file(s) to reconcile", berarti semua sudah masuk
```

### B7. Nasib folder `.git` (Git lama)

Sampai sekarang Git hanya menyimpan code (`Source/`, `Config/`, `Tools/`, `Docs/`) dan tidak melacak `Content/`.
**Jangan pakai dua version control aktif sekaligus** untuk file yang sama, karena akan membingungkan.

Rekomendasi:
- Perforce menjadi **satu-satunya** sumber kebenaran sejak submit pertama.
- Biarkan folder `.git` apa adanya sebagai **arsip riwayat lama** (read-only, jangan commit lagi). Sudah diabaikan oleh
  `.p4ignore`, jadi tidak ikut masuk depot.
- Histori commit Git **tidak ikut pindah** ke Perforce (depot dimulai dari snapshot saat ini).
- Kalau kelak yakin tidak butuh lagi, `.git` boleh dihapus.

### B8. Setelah submit: file jadi read-only

Setelah di-submit, file di workspace diatur **read-only** sampai di-*check out*. Ini normal dan disengaja. Unreal Editor
melakukan check out otomatis saat kamu menyimpan asset (lihat bagian 5). Untuk file code, lihat bagian 5 (Rider).

---

## 4. Bagian C - Anggota tim lain

Dikerjakan oleh tiap teman yang bergabung. Tidak perlu apa-apa di sisi server selain user yang sudah dibuat (A3 langkah 6).

1. Pasang **Tailscale**, terima undangan, login. Tes: `tailscale ping <IP_HOST>`.
2. Pasang **P4V**.
3. P4V → **Connection → Open Connection** → Server `<IP_HOST>:1666`, user + password dari admin. Ganti password awal
   dengan `p4 passwd` di PowerShell (set dulu `$env:P4PORT = "<IP_HOST>:1666"` dan `$env:P4USER = "<user>"`).
4. Buat workspace (seperti B3) dengan **root folder kosong** di PC masing-masing, misalnya `D:\UEProjects\Zoo`.
   Workspace name unik: `zoo_<nama>_pc`.
5. Di P4V, klik kanan depot `Zoo` → **Get Latest Revision**. Ini mengunduh seluruh proyek (~20 GB), jadi lakukan
   dengan koneksi stabil.
6. Hasil sync **tidak berisi** `Binaries/` dan `Intermediate/`. Buat sendiri:
   - Klik kanan `Zoo.uproject` → **Generate Visual Studio project files**.
   - Buka solution di Rider/Visual Studio dan **build** (Development Editor, Win64).
   - Setelah itu buka `Zoo.uproject`.
   - Versi Unreal Engine dan toolchain (Visual Studio/Windows SDK) harus **sama** dengan anggota lain.
7. Buat `.p4ignore` lokal? Tidak perlu. `.p4ignore` sudah masuk depot dan ikut di-sync. Cukup jalankan sekali:
   ```powershell
   p4 set P4IGNORE=.p4ignore
   ```

---

## 5. Bagian D - Unreal Editor dan Rider

### D1. Unreal Editor

1. Buka proyek di Unreal Editor.
2. Klik ikon **Source Control** di toolbar (dokumentasi Epic memakai nama ini; di UE5 ikon ini kemungkinan bernama
   **Revision Control**, ⚠️ belum terverifikasi). Pilih **Perforce** sebagai provider.
3. Isi data koneksi:

   | Field | Isi |
   |---|---|
   | Server | `<IP_HOST>:1666` |
   | User Name | user kamu |
   | Workspace | pilih workspace kamu (`zoo_<nama>_pc`) |
   | Password | password kamu |

4. **Accept Settings**. Ikon di Content Browser akan menunjukkan status tiap asset (checked out, terkunci orang lain, dll.).

Sehari-hari di editor:
- Saat kamu mengedit asset dan menyimpan, editor menawarkan **Check Out**. Setuju.
- Asset yang sedang di-check out orang lain **terkunci** dan tidak bisa kamu ubah. Hubungi orang itu.
- Asset baru yang kamu buat otomatis **Mark for Add**.
- **Submit**: bisa dari editor (menu klik kanan asset/folder di Content Browser, bagian Source/Revision Control; nama
  persis menunya ⚠️ belum terverifikasi) atau lewat P4V. Lewat P4V lebih bisa diandalkan.

### D2. Rider / Visual Studio (file C++)

File C++ adalah **teks** (bisa digabung otomatis kalau dua orang mengedit baris berbeda), tapi tetap read-only di
workspace sampai di-*check out*.

Rider punya integrasi Perforce, tapi **nonaktif secara default**. Aktifkan:
1. `Ctrl+Alt+S` → **Version Control → Directory Mappings** → **Add** → pilih folder proyek → VCS: **Perforce**.
2. **Version Control → Perforce** → centang **Perforce is online**, lalu isi Port, Client (workspace), User, Password
   (atau biarkan Rider membaca pengaturan P4CONFIG kalau ada).
3. Rider punya mode offline yang mencatat operasi (Edit, Add, Delete, Revert, dll.) dan memutar ulang saat online.

Alternatif tanpa integrasi Rider (cara manual yang selalu jalan):
- Hilangkan flag read-only file yang mau diedit, edit, lalu di akhir sesi di P4V klik kanan folder →
  **Reconcile Offline Work** (P4V membandingkan workspace dengan depot dan menawarkan check out untuk file yang
  berubah, mark for add untuk file baru, mark for delete untuk yang hilang), atau `p4 reconcile Source/...`.

---

## 6. Aturan kerja sehari-hari

1. **Mulai kerja = Get Latest dulu.** Sebelum membuka editor, sync supaya tidak mengedit versi usang.
2. **Check out sebelum edit asset.** Editor melakukannya otomatis. File `.umap` dan `.uasset` bersifat exclusive lock.
3. **Level (`.umap`) hanya satu orang pada satu waktu.** Kalau seluruh level `L_Zoo` ada di **satu file** `.umap`,
   hanya satu orang yang boleh mengedit level itu. ⚠️ Di UE5 bisa memakai **One File Per Actor** (External Actors)
   supaya beberapa orang bisa mengedit level yang sama; cek apakah level kalian memakainya (World Settings / Level
   → *Use External Actors*). Selain itu, koordinasikan lewat chat siapa yang memegang level.
4. **Submit sering dan kecil**, dengan deskripsi jelas ("Tambah kandang gajah" bukan "update").
5. **Jangan submit pekerjaan setengah jadi yang membuat proyek tidak bisa dibuka/dikompilasi.** Pastikan compile
   sukses dan editor bisa dibuka sebelum submit.
6. **Selesai edit = submit (atau revert) supaya lock dilepas.** Jangan pulang dengan masih meng-check out banyak file.
   Lihat siapa yang sedang meng-check out file: `p4 opened -a`.
7. **Jangan edit file di luar Perforce** lalu berharap otomatis terkirim. Selalu lewat check out atau **Reconcile
   Offline Work**.
8. Kalau salah mengubah dan ingin batal: klik kanan file → **Revert** (membuang perubahan lokal, mengembalikan versi
   depot).
9. Jangan hapus file/folder langsung di Explorer. Hapus lewat P4V/Editor (**Mark for Delete**), atau lakukan
   Reconcile sesudahnya.

---

## 7. Backup (WAJIB)

Seluruh sejarah proyek ada di **satu laptop**. Kalau SSD rusak, semuanya hilang. Backup harus berjalan **sejak hari
pertama**, dan salinannya harus berada **di luar laptop host** (drive eksternal dan/atau cloud).

Backup Perforce terdiri dari dua bagian:
1. **Checkpoint**: salinan "isi database" server (daftar user, workspace, riwayat, dll.).
2. **Versioned files**: file isi depot (semua versi aset kalian) di dalam folder P4ROOT.

### 7.1 Siapkan akun backup (sekali saja, di laptop host)

Akun khusus supaya skrip bisa jalan tanpa login manual. Perlu izin `super` (checkpoint adalah operasi admin) dan ticket
yang tidak kedaluwarsa:

```powershell
$env:P4PORT = "127.0.0.1:1666"; $env:P4USER = "admin"; p4 login

p4 user -f backup          # simpan
p4 passwd backup           # beri password kuat
p4 group backupgrp         # di editor: Users: backup   |  Timeout: unlimited
p4 protect                 # tambahkan satu baris di bagian Protections:
                           #   super user backup * //...
```

Lalu **login satu kali sebagai `backup` memakai akun Windows yang sama dengan yang akan menjalankan Task Scheduler**:

```powershell
p4 -p 127.0.0.1:1666 -u backup login
```

Ticket tersimpan untuk akun Windows itu dan tidak kedaluwarsa karena `Timeout: unlimited`.

> Akun `backup` ikut dihitung sebagai user (batas gratis 5 user). Tim 3 orang + backup = 4, masih aman.

### 7.2 Skrip backup

Simpan sebagai `C:\P4Scripts\backup.ps1` di laptop host. **Sesuaikan 3 baris variabel di atas.**

```powershell
# ===== SESUAIKAN =====
$P4      = "C:\Program Files\Perforce\p4.exe"   # lokasi p4.exe (cek dengan: where.exe p4)
$P4Root  = "D:\PerforceData"                    # folder P4ROOT server
$Dest    = "E:\P4Backup"                        # drive eksternal / folder cloud-sync (BUKAN di drive yang sama)
# =====================

$Port = "127.0.0.1:1666"
$User = "backup"

# 1) Buat checkpoint terkompresi (online, server tidak perlu dimatikan). Journal ikut dirotasi.
& $P4 -p $Port -u $User admin checkpoint -z
if ($LASTEXITCODE -ne 0) { Write-Error "Checkpoint gagal"; exit 1 }

# 2) Salin folder P4ROOT ke tujuan, KECUALI file database mentah db.* (itu diwakili checkpoint)
robocopy $P4Root $Dest /MIR /XF db.* /R:2 /W:5 /NFL /NDL
if ($LASTEXITCODE -ge 8) { Write-Error "Robocopy gagal (kode $LASTEXITCODE)"; exit 1 }   # 0-7 = sukses

# 3) Rapikan: simpan hanya 7 checkpoint dan 7 journal terbaru di server
foreach ($pattern in "checkpoint.*.gz", "journal.*.gz") {
  Get-ChildItem (Join-Path $P4Root $pattern) | Sort-Object LastWriteTime -Descending |
    Select-Object -Skip 7 | Remove-Item
}
```

Catatan:
- `p4 admin checkpoint` bisa dijalankan saat server hidup. Perlu akses `super` (atau `operator`). Journal aktif otomatis
  dirotasi. Dengan `-z`, **checkpoint dan journal** sama-sama dikompres (`checkpoint.N.gz` dan `journal.N-1.gz`).
  File-file ini ditulis ke folder P4ROOT. Karena itu robocopy yang menyalin P4ROOT sudah membawa checkpoint, journal
  rotasi, plus semua versioned files.
- `/MIR` bersifat **mirror**: tujuan dibuat identik dengan sumber, termasuk menghapus yang sudah tidak ada. Arahkan
  hanya ke folder khusus backup.
- Tes manual sekali: jalankan `powershell -ExecutionPolicy Bypass -File C:\P4Scripts\backup.ps1`, lalu cek folder
  tujuan terisi.

### 7.3 Jadwalkan otomatis

Buka **Task Scheduler** → **Create Task...**:
- **General:** nama `P4Backup`, jalankan dengan akun Windows yang sama saat login backup tadi.
- **Triggers:** harian pada jam laptop biasanya menyala (misalnya 22.00).
- **Actions:** Program `powershell.exe`, argumen `-ExecutionPolicy Bypass -File C:\P4Scripts\backup.ps1`.
- **Settings:** centang **Run task as soon as possible after a scheduled start is missed** (kalau laptop sedang mati
  pada jam jadwal).

Atau lewat command line:

```powershell
schtasks /create /tn "P4Backup" /sc daily /st 22:00 /tr "powershell -ExecutionPolicy Bypass -File C:\P4Scripts\backup.ps1"
```

### 7.4 Uji pemulihan (sekali, sebelum benar-benar butuh)

Backup yang belum pernah diuji pemulihannya belum bisa dianggap ada. Garis besar pemulihan ke server baru:

1. Pasang Perforce server yang sama (atau lebih baru) di mesin baru, dengan P4ROOT kosong.
2. Hentikan service, salin isi folder backup (versioned files + `checkpoint.N.gz` + file `journal*`) ke P4ROOT baru.
3. Pulihkan database dari checkpoint. Sejak versi 2018.1 tidak perlu `-z` lagi untuk file terkompresi, dan nama file
   ditulis relatif terhadap P4ROOT. Contoh dari dokumentasi Perforce (checkpoint 42, lalu journal yang menyusulnya):
   ```powershell
   p4d -r <P4ROOT_baru> -jr checkpoint.42.gz journal.42 journal.43 journal
   ```
   Sesuaikan nomor dengan file yang kamu punya: checkpoint terbaru lalu semua journal yang lebih baru darinya.
4. Jalankan service lagi, lalu `p4 verify -q //...` untuk memeriksa integritas file.

Dokumentasi Perforce menegaskan bahwa checkpoint/journal hanya menyimpan **metadata**; file versi (isi aset)
ada di bawah P4ROOT dan **harus ikut dibackup**, makanya robocopy di atas menyalin keseluruhan P4ROOT.

⚠️ Penomoran persis antara `checkpoint.N` dan `journal.N` belum kuverifikasi. Selalu uji prosedur ini sekali di mesin
lain dan baca bagian *Backup and recovery* di dokumentasi resmi (link di bagian 12).

### 7.5 Aturan emas

- Backup di **disk yang sama dengan P4ROOT** bukan backup.
- Minimal dua salinan terpisah: drive eksternal **dan** cloud/PC orang lain.
- Pantau ruang disk laptop host. Depot yang penuh membuat server error.

---

## 8. Troubleshooting

| Gejala | Kemungkinan penyebab | Solusi |
|---|---|---|
| `Connect to server failed; check $P4PORT` / P4V tidak bisa konek | Laptop host mati/sleep, Tailscale mati, atau firewall | 1) Cek laptop host menyala. 2) Di PC kamu: `tailscale ping <IP_HOST>`. 3) Pastikan Tailscale berjalan di kedua sisi. 4) Cek aturan firewall A4. 5) Di host: pastikan service Perforce *Running*. |
| `Perforce password (P4PASSWD) invalid or unset` | Ticket login habis | Login ulang: `p4 login` atau P4V → Connection → Open Connection. |
| Sync/submit sangat lambat | Koneksi jatuh ke relay Tailscale, atau upload internet host kecil | `tailscale status` lihat apakah `direct` atau `relay`. Coba saat jaringan host lebih longgar. Pecah submit besar. |
| `Can't clobber writable file` saat sync | File lokal writable tapi berbeda dari depot | Kalau perubahan lokal tidak penting: `p4 sync -f <file>`. Kalau penting: `p4 reconcile` / check out dulu. |
| File tidak bisa disimpan (read-only) | Belum di-check out | Check out lewat editor/P4V (`p4 edit <file>`). |
| `.uasset` terkunci oleh orang lain | Exclusive lock `binary+l` | Cek siapa: `p4 opened -a <file>`; minta dia submit/revert. |
| File yang seharusnya diabaikan ikut masuk | `.p4ignore` tidak terbaca | Pastikan `p4 set P4IGNORE=.p4ignore` sudah dijalankan; cek isi `.p4ignore`; ulangi preview `p4 reconcile -n`. |
| File asset terlanjur masuk dengan tipe salah | Typemap belum di-set sebelum add | Sebelum submit: `p4 reopen -t binary+l <file>`. Kalau sudah submit: `p4 edit -t binary+l <file>` lalu submit lagi. |
| `You've reached the license limit` / hanya 1.000 file | Lebih dari 5 user atau 20 workspace | Hapus user/workspace yang tidak terpakai, atau beli lisensi. |
| Disk host hampir penuh | Depot membesar | Kosongkan disk, pindahkan P4ROOT ke disk lebih besar (lihat bagian 9). |
| Editor tidak menemukan workspace saat connect | Root workspace tidak mencakup folder proyek | Pastikan root workspace = folder yang berisi `Zoo.uproject`. |

Perintah diagnostik yang berguna:

```powershell
p4 info                 # status koneksi, user, workspace
p4 opened               # file yang sedang kamu check out
p4 opened -a            # semua file yang sedang di-check out (semua orang)
p4 changes -m 10        # 10 submit terakhir
tailscale status        # status jaringan Tailscale
```

---

## 9. Pindah host

Skenario utama: **sementara server ada di PC kamu, nanti dipindah ke laptop teman** (atau ke VPS/hosted). Perforce
mendukung ini. Dokumentasi menyebut dua hal yang harus ikut pindah: **checkpoint** (metadata: user, workspace, riwayat,
typemap, pengaturan server) dan **versioned files** (isi aset di bawah P4ROOT). Karena workspace dan daftar file yang
sudah di-sync tiap orang tersimpan di database, workspace anggota tim tidak perlu dibuat ulang. Pekerjaan di sisi klien
hanya mengganti alamat server.

### 9.1 Persiapan sejak server masih di PC kamu

Lakukan dari awal supaya pindahnya mulus:

- **P4ROOT di folder terpisah dari proyek**, misalnya `D:\PerforceData`, **bukan** di dalam `D:\UEProjects\Zoo`
  (folder proyek adalah workspace, bukan depot server).
- **Disk:** depot server (~20 GB + pertumbuhan) dan workspace (~20 GB) sama-sama ada di PC kamu. Siapkan ruang kosong
  100 GB atau lebih.
- **Pengaturan yang tidak bisa diubah setelah depot dibuat harus sama di host lama dan baru:** case-insensitive dan
  non-Unicode (lihat bagian 1 dan A2).
- **Catat versi server:** `p4 info` menampilkan *Server version*. Host baru harus memasang versi yang **sama atau lebih
  baru**.
- **Backup jalan sejak hari pertama** (bagian 7). Bahan yang sama dipakai untuk pindah.
- **Latihan dulu:** sebelum hari pindah, coba restore backup ke mesin lain (7.4) supaya tahu prosedurnya jalan dan
  berapa lama. Jangan baru mencobanya saat pindah sungguhan.
- **Catat alamat server di satu tempat** (misalnya channel tim) supaya gampang diganti semua orang nanti.
- Selama server di PC kamu: PC harus menyala dan tidak sleep saat tim butuh sync (atur seperti A5).

### 9.2 Checklist hari pindah

Rencanakan di luar jam kerja. Lamanya terutama ditentukan waktu menyalin 20 GB+ (belum diukur; jajal saat latihan).

**Langkah 1 - Freeze (semua orang)**
- Umumkan: semua submit pekerjaan terakhir atau revert, lalu **berhenti submit**.
- Pastikan tidak ada file tertinggal ter-check out: `p4 opened -a` (di PC lama). Kalau masih ada, minta pemiliknya
  submit atau revert.

**Langkah 2 - Siapkan laptop host baru**
1. Pasang Tailscale (A1) dan bergabung ke tailnet yang sama. Catat alamat `100.x.x.x` baru.
2. Pasang Perforce server dengan versi sama/lebih baru (A2). Pilih P4ROOT baru, **non-Unicode**.
3. **Jangan** membuat depot, user, typemap, atau mengubah konfigurasi di server baru. Semuanya datang dari checkpoint.
4. Hentikan service Perforce di laptop baru (services.msc) sebelum restore. ⚠️ Restore butuh P4ROOT tanpa database
   lama; kalau service sempat membuat file `db.*` sendiri, hapus file `db.*` itu (server masih kosong) sebelum restore.

**Langkah 3 - Backup final dari PC lama**
```powershell
# di PC lama, sebagai user backup/admin
p4 admin checkpoint -z                 # checkpoint terbaru (butuh super)
# lalu hentikan service Perforce di PC lama (services.msc) supaya TIDAK ada submit baru
robocopy D:\PerforceData E:\P4Migrasi /MIR /XF db.* /R:2 /W:5
```
(`E:\P4Migrasi` = drive eksternal atau folder cloud-sync. Sesuaikan path.) Setelah service dihentikan, tidak ada yang
bisa submit lagi, jadi salinannya konsisten dengan checkpoint yang barusan dibuat.

**Langkah 4 - Pindahkan dan restore di laptop baru**
1. Salin isi `E:\P4Migrasi` (versioned files + `checkpoint.N.gz` + file `journal*`) ke P4ROOT baru, lewat drive
   eksternal atau cloud.
2. Pulihkan database (format dari 7.4; sesuaikan nomor dengan file yang ada):
   ```powershell
   p4d -r <P4ROOT_baru> -jr checkpoint.N.gz journal.N journal
   ```
   Checkpoint terbaru lebih dulu, lalu semua journal yang lebih baru darinya.
3. Nyalakan service Perforce. Atur firewall (A4) dan power plan (A5) di laptop baru. Pengaturan Windows itu tidak
   ikut pindah.

**Langkah 5 - Verifikasi di laptop baru**
```powershell
$env:P4PORT = "127.0.0.1:1666"; $env:P4USER = "admin"; p4 login
p4 info                       # server jalan, versi benar
p4 users                      # semua user ada
p4 depots                     # depot Zoo ada
p4 typemap -o                 # typemap binary+l ikut terbawa
p4 configure show security    # level keamanan terbawa
p4 verify -q //...            # integritas file (bisa lama untuk 20 GB; tanpa keluaran = tidak ada masalah)
```

**Langkah 6 - Klien mengganti alamat server (semua orang, termasuk kamu)**

| Alat | Yang diganti |
|---|---|
| P4V | Connection → Open Connection → server baru `<IP_HOST_BARU>:1666` |
| Unreal Editor | pengaturan koneksi Source/Revision Control → kolom Server |
| Rider | Settings → Version Control → Perforce → Port |
| Command line | `p4 set P4PORT=<IP_HOST_BARU>:1666` |

- Kemungkinan perlu **login ulang** di server baru. ⚠️ Detail ticket per server belum kuverifikasi.
- **Tes workspace:** `p4 sync -n` seharusnya melaporkan tidak ada yang perlu di-sync (artinya daftar file yang sudah
  kamu punya ikut terbawa). Lalu coba submit perubahan kecil dan sync dari akun lain.

**Langkah 7 - Rapikan**
- **Jangan pernah menyalakan lagi server lama** setelah server baru dipakai. Dua server yang masing-masing menerima
  submit akan menghasilkan data yang berbeda dan tidak bisa digabung begitu saja.
- Simpan P4ROOT lama (service tetap mati) beberapa minggu sebagai cadangan, hapus setelah server baru terbukti stabil
  dan backup-nya jalan.
- Pasang ulang backup di host baru (bagian 7): skrip dan Task Scheduler. Login ulang akun `backup` sekali di host
  baru memakai akun Windows yang menjalankan Task Scheduler (7.1).

### 9.3 Pindah ke VPS atau hosted

- Prosesnya sama: checkpoint + versioned files, lalu klien ganti alamat.
- **Pindah dari Windows ke Linux lebih rumit**: aturan huruf besar/kecil dan path berbeda antar platform. Baca dokumentasi
  *Case sensitivity and multi-platform development* sebelum mencoba (link di bagian 12). Jangan dikerjakan tanpa
  latihan.
- Layanan hosted (Assembla, Helix Core Cloud) punya prosedur migrasi sendiri. ⚠️ Belum kuriset; tanyakan ke penyedia.

### 9.4 Kalau tiba-tiba harus pindah (host rusak)

Kalau host mati mendadak, kamu hanya bisa memulihkan dari backup terakhir. Itu alasan backup di bagian 7 wajib dan
salinannya harus berada di luar host. Prosedurnya sama seperti 9.2 langkah 2, 4, 5, 6, dan 7, tanpa langkah freeze.
Submit yang terjadi setelah backup terakhir hilang di server, tapi file-nya masih ada di workspace masing-masing
dan bisa dikirim ulang dengan **Reconcile Offline Work** di P4V (lihat D2). ⚠️ Perilaku persis workspace yang
"lebih baru dari server" setelah restore belum kuverifikasi; uji saat latihan.

---

## 10. Kalau tim membesar

- **Lebih dari 5 user atau 20 workspace:** server gratis dibatasi 1.000 file. Opsinya beli lisensi Perforce
  atau pakai layanan hosted (mis. Assembla Perforce Cloud mulai ~US$52,25 per user per bulan, atau Helix Core Cloud
  ~US$39 per user per bulan; harga dari hasil pencarian, cek ulang di situs resminya).
- **VPS gratis (Oracle Always Free) tidak cocok:** server Perforce di Linux hanya didukung Intel x86_64 (bukan ARM) dan
  jatah ARM gratis Oracle sudah dipotong sejak 18 Agustus 2026.
- Pertimbangkan **Streams** (mainline + development stream) kalau tim sudah banyak dan butuh build demo yang stabil.
  Belum perlu untuk 3 orang.
- Tailscale Personal maksimal 6 user. Lebih dari itu perlu plan berbayar atau alternatif.
- Pindah ke server yang lebih besar: lihat bagian 9.

---

## 11. Status verifikasi

Panduan ini sudah diperiksa ulang terhadap dokumentasi. Ringkasannya:

**Terverifikasi dari dokumentasi/sumber resmi**
- Batas gratis Perforce: 5 user, 20 workspace; lewat itu dibatasi 1.000 file.
- Server Linux hanya Intel x86_64 (tidak ada ARM); Windows tidak membedakan huruf besar/kecil; Epic menyarankan server
  case-insensitive dan server Epic sendiri non-Unicode.
- Typemap: sintaks `//....ext` (contoh resmi `binary //....pdf`); `binary+l` untuk ekstensi UE dari dokumentasi Epic.
- Security level: level 3 dan 4 mewajibkan login ticket; level 0-2 mendukung password. Minimal panjang password default 8.
- `p4 admin checkpoint -z`: online, butuh `super` atau `operator`, mengompres checkpoint dan journal, rotasi journal
  otomatis, file di P4ROOT.
- Restore: `p4d -r <root> -jr checkpoint.N.gz journal...` (tanpa `-z` sejak 2018.1); versioned files harus dibackup
  terpisah.
- `.p4ignore`: sintaks (`#`, `/` awal/akhir, `*`, `**`, `!`); `p4 reconcile` menghormati P4IGNORE, `-n` = preview,
  tanpa flag = add + edit + delete, file masuk default changelist.
- Submit yang gagal tidak otomatis lanjut; ada `submit.noretransfer` (nilai `1`) untuk menghindari transfer ulang.
- Group `Timeout: unlimited` untuk ticket yang tidak kedaluwarsa.
- Tailscale: Disable key expiry di halaman Machines; undang lewat Users → Invite external users; rentang
  `100.64.0.0/10`; Personal plan maksimal 6 user (halaman pricing resmi).
- P4V: Connection → New Workspace, klik kanan folder → Reconcile Offline Work. Rider: integrasi Perforce nonaktif
  secara default, diaktifkan lewat Directory Mappings.

**Belum terverifikasi (⚠️) - cek sebelum diandalkan**
1. **Halaman instalasi Windows** di dokumentasi Perforce kosong saat riset. Langkah installer (A2) berasal dari dokumen
   lama dan hasil pencarian. Tampilan installer terbaru bisa berbeda. Lokasi `p4.exe` di skrip hanyalah perkiraan;
   cari dengan `where.exe p4`.
2. **Apakah installer/Windows membuat aturan firewall sendiri untuk Perforce** (A4): dokumentasi instalasi Windows
   Perforce tidak menyebut firewall sama sekali, jadi tidak diketahui. Skrip pemeriksaan di A4 sudah diuji
   sintaksnya, tapi hasilnya di laptop host belum diketahui. Jalankan sendiri setelah instalasi.
3. **Lisensi Tailscale Personal:** ambigu antara "hanya non-commercial" di halaman pricing dan pernyataan staf di forum
   soal jumlah user (bagian 1). Baca Terms of Service.
4. **Nama menu Unreal Editor:** "Source Control" (Epic) vs "Revision Control" (UE5), dan nama menu Submit; juga apakah
   level `L_Zoo` memakai One File Per Actor. Belum diperiksa di proyek ini.
5. **Penomoran `checkpoint.N` / `journal.N`** saat restore (7.4) dan seluruh skrip backup belum diuji. Uji sendiri.
6. **Pola `.p4ignore`** (`Binaries`, `Intermediate`, ..., `Tools/**/out*/`) belum diuji di proyek ini; **wajib uji
   dengan `p4 reconcile -n`** (B5).
7. **Apakah ada ekstensi UE lain yang perlu `+l`** (mis. `.uexp`) tidak diperiksa; hanya lima ekstensi dari Epic dipakai.
8. **Perintah Windows** `powercfg` (sleep/hibernate/lid) dan perintah CLI Tailscale (`tailscale ip/ping/status`) berasal
   dari pengetahuan umum, bukan dari riset di sesi ini.
9. **Harga hosted Perforce** dari hasil pencarian, bisa berubah.
10. **Unicode mode tidak bisa dimatikan lagi** setelah dinyalakan: dari dokumentasi instalasi Linux; diasumsikan
    berlaku di Windows juga.
11. **Prosedur pindah host (bagian 9) secara keseluruhan belum diuji.** Khususnya: apakah installer membuat file `db.*`
    yang harus dihapus sebelum restore, apakah user perlu login ulang di server baru, apakah `p4 sync -n` melaporkan
    bersih setelah restore, dan perilaku workspace yang lebih baru dari server setelah restore dari backup lama.
    Yang terverifikasi hanya bahan dasarnya: checkpoint + journal + versioned files, format `p4d -jr`, dan
    `p4 admin checkpoint`.
12. **Pindah Windows → Linux** (9.3) dan **prosedur migrasi hosted** belum diriset.

---

## 12. Sumber

- Epic: [Using Perforce as source control for Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-perforce-as-source-control-for-unreal-engine)
- Perforce: [Linux package-based installation (arsitektur server: x86_64)](https://help.perforce.com/helix-core/server-apps/p4sag/current/Content/P4SAG/install.linux.packages.html)
- Perforce: [Windows installation](https://perforce.com/manuals/p4guide/Content/P4Guide/install.windows.html)
- Perforce: [Case sensitivity on Windows](https://perforce.com/manuals/v22.2/p4sag/Content/P4SAG/superuser.advanced.case_sensitivity.windows.html)
- Perforce: [What do you get for free? (batas 5 user / 20 workspace)](https://www.perforce.com/node/323)
- Perforce: [Server Administrator Guide (backup & recovery)](https://help.perforce.com/helix-core/server-apps/p4sag/current/)
- Perforce: [Case sensitivity and multi-platform development](https://perforce.com/manuals/p4sag/Content/P4SAG/case-sensitivity-and-platforms.html)
- Tailscale: [Pricing](https://tailscale.com/pricing), [Pricing FAQ](https://tailscale.com/kb/1251/pricing-faq), [Terms of Service](https://tailscale.com/legal/terms)
- Hosted Perforce: [Assembla](https://get.assembla.com/blog/perforce-pricing/), [Helix Core Cloud](https://www.perforce.com/node/434621)
