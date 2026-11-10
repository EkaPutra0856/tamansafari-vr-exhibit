# ZooEka2VR â€” salinan workspace Perforce EkaZoo2 + mode VR (Meta Quest via Link / Air Link)

Sumber: `C:\Users\USER\Perforce\EkaZoo2\Zoo` (tidak diubah sama sekali). Salinan: `D:\Projek UE\ZooEka2VR`.
Tujuan: fitur, VFX, BP, map, dan setting render project tetap sama persis; VR hanya lapisan tambahan.

## Prinsip
- Pawn, controller, HUD tetap `BP_ZooCharacter`, `BP_ZooPlayerController`, `BP_Zoo_HUD` (BP lain cast ke kelas ini:
  BP_AnimalBase, BP_AnimalDetection, peta 2D, WBP_AR_Media). Tidak ada BP Zoo yang diedit.
- Semua VR ada di satu komponen `UZooVRComponent` yang ditempel ke PlayerController oleh `AZooVRGameMode`.
- Tanpa headset komponen diam total: main di desktop identik dengan project asli.
- Setting render di Config tidak diubah. Profil performa VR hanya diterapkan saat headset aktif dan dikembalikan saat selesai.

## File yang ditambah / diubah
- Baru C++: `Source/Zoo/VR/ZooVRComponent.h/.cpp`, `Source/Zoo/VR/ZooVRGameMode.h/.cpp`
- `Source/Zoo/Zoo.Build.cs`: private deps `HeadMountedDisplay`, `XRBase`
- `Zoo.uproject`: plugin OpenXR, OpenXRHandTracking, XRBase, OculusXR (Meta XR 207.0 di engine UE 5.8)
- `Config/DefaultEngine.ini`: hanya `GlobalDefaultGameMode` -> `/Game/ZooVR/Blueprints/BP_ZooVRGameMode`
- Aset baru `/Game/ZooVR/`:
  - `Blueprints/BP_ZooVRGameMode` (duplikat BP_ZooGameMode, parent `ZooVRGameMode`; pawn/PC/HUD sama dengan asli)
  - `Blueprints/BP_ZooVRComponent` (aset input, material panel, nama widget yang disembunyikan)
  - `Input/IA_ZooVR_Move`, `IA_ZooVR_SnapTurn`, `IA_ZooVR_Select`, `IMC_ZooVR`

## UZooVRComponent
- Rig (runtime, ke pawn yang dipossess): kamera dipindah ke `ZooVR_Origin` di kaki kapsul dan mengikuti headset
  (`bLockToHmd`, tracking origin LocalFloor); controller kiri/kanan dengan model dari runtime; laser kanan
  (`WidgetInteraction`, channel Visibility, 10 m).
- Panel: widget layar `UWzWidget` (WBP_PathGuide, WBP_Zoo_NarrativeOverlay, dll.) dipindah ke panel 3D 170 cm
  di 120 cm depan mata, lazy-follow (diam saat menoleh < 35Â°, kembali ke depan setelah menoleh jauh / berjalan),
  material `M_ARWidget_OnTop`. `WaypointPin` (penanda proyeksi layar) disembunyikan; rute 3D tetap ada.
- AR exhibit (panel info MEDIA / KARAKTERISTIK / GENERAL DESCRIPTION / HABITAT, klik hologram hewan, hover):
  satu-satunya perubahan pada kode Zoo yang ada: `AZooARExhibit::GetMouseRay` (ZooARExhibit.cpp) memakai ray laser
  kanan saat VR aktif (`UZooVRComponent::GetPointerRay`); desktop tetap mouse / arah pandang. Trigger kanan
  mensimulasikan klik kiri, jadi seluruh logika klik exhibit PC (HandleMousePressed/Released, celah panel ke
  hologram, dll.) berjalan sama persis. Klik kiri juga = `IA_DialogueSkip` (sama seperti PC).
- Routing trigger: laser di atas panel VR (Navigasi / quest / narrative) -> klik langsung oleh laser; selain itu ->
  klik kiri simulasi (diproses exhibit / IMC_Zoo seperti di PC).
- Analog kanan atas/bawah: scroll widget di bawah laser (daftar "Mau ke mana?", isi panel info AR).
- Semua widget layar (`UUserWidget` top-level: WBP_PathGuide, WBP_Zoo_NarrativeOverlay, WBP_AR_MediaExtend, ...)
  dipindah ke panel VR, masing-masing di dalam `UZooVRPanelHost` (Source/Zoo/VR/ZooVRPanelHost.h/.cpp, root Overlay).
  Karena widget punya parent slot sungguhan, `RemoveFromParent` milik game (mis. `BP_Zoo_HUD.CloseMediaExtend`)
  benar-benar melepasnya, lalu panel VR-nya otomatis dihapus. Panel baru (video extend) muncul tepat di depan mata,
  paling depan.
- Performa (hanya di VR): `r.DynamicGlobalIlluminationMethod 0`, `r.ReflectionMethod 2`, `r.Shadow.Virtual.Enable 0`,
  `r.Shadow.CSM.MaxCascades 2`, `r.Streaming.PoolSize 2000`, `vr.PixelDensity 0.9`. Bisa diubah di
  BP_ZooVRComponent > VR Console Variables (hapus baris untuk kualitas penuh).

## Kontrol Quest
| Input | Fungsi |
|---|---|
| Analog kiri | Jalan (arah kepala); klik analog = sprint |
| Analog kanan kiri/kanan | Snap turn 45Â° |
| Analog kanan atas/bawah | Scroll widget di bawah laser |
| Trigger kanan (tap) | Klik apa pun yang ditunjuk laser (panel VR, panel info AR, hologram, lanjut dialog) |
| Trigger kanan (tahan + gerak laser naik/turun) | Scroll |
| A (tap) | Buka/tutup Navigasi ("Mau ke mana?") |
| A (tahan + gerak laser naik/turun) | Scroll |
| B | Tutup menu |

Teks petunjuk keyboard di UI diganti otomatis saat VR aktif (`PromptReplacements` di BP_ZooVRComponent):
`[N]` -> `[A]`, `N / Esc` -> `A / B`, `[Esc]` -> `[B]`, `Spasi` -> `Trigger`, `[Alt]` -> `[Trigger]`.

Panel VR ada di aktor terpisah bertag `ARIgnoreTrace`: laser (WidgetInteraction) mengabaikan komponen milik
pemiliknya sendiri, jadi panel tidak boleh menempel di pawn; tag membuat ray exhibit AR menembus area transparan panel.
Laser memakai InteractionSource Custom: `UpdatePointerTarget` men-trace tiap panel dan memilih panel terdekat yang punya
tombol / scroll box di titik itu (panel full-screen bertumpuk; trace bawaan berhenti di area transparan panel depan).
Hover: tombol di bawah laser membesar (`HoverScale` 1.06) dan diberi outline glow tipis (`HoverGlowColor`),
digambar oleh `UZooVRPanelHost::SetHighlight`; BP Zoo tidak diubah.
## Catatan
- Instanced stereo tetap OFF (default). Di UE 5.8 instanced stereo + Nanite translucency crash.
- Untuk dipindah ke Perforce: `p4 add` file VR baru + `/Game/ZooVR`, `p4 edit` uproject, Zoo.Build.cs, DefaultEngine.ini.

- `Config/DefaultInput.ini`: `EnhancedInputDeveloperSettings.DefaultMappingContexts` += IMC_ZooVR (bAddImmediately=False). OpenXR hanya membuat action dari daftar ini; tanpa ini tombol Quest tidak sampai ke game.


## Demo / rekaman (autopilot tanpa headset) — 2026-10-24

Tujuan: video gameplay VR (>= 5 menit, 1080p30) tanpa orang memakai headset. Meta XR Simulator v205 terpasang
(`C:\Program Files\MetaXRSimulator\v205.0`) tapi tidak punya API skrip pose kepala/tangan, jadi dipakai opsi B:
**autopilot demo** yang menjalankan lapisan VR asli tanpa HMD dan menyuntikkan input lewat jalur controller yang sama.

### Kode (semua default OFF; mode desktop & VR normal tidak berubah)
- `Source/Zoo/VR/Demo/ZooVRDemoDirector.h/.cpp` — `UZooVRDemoDirector` (komponen di PlayerController, dibuat oleh
  `UZooVRComponent` hanya saat `zoo.vr.Demo 1` atau `-ZooVRDemo`; `IsRequested()` selalu false di Shipping).
  - Memaksa `bVRActive` tanpa HMD: rig, panel 3D, laser, hover, snap turn, dll. sama seperti VR.
  - Pose kepala & tangan dari skrip JSON (`Content/ZooVR/Demo/ZooVRDemoScript.json`, override `-ZooVRDemoScript=<path>`):
    kepala di-interp halus + sway kecil, bob saat jalan; tangan = `SKM_MannyXR_left/right` + `ABP_MannequinsXR`
    (aset template XR Epic, dipindah ke `/Game/ZooVR/Hands`), pose `PoseAlphaGrasp/IndexCurl/Point/ThumbUp` diset via
    reflection: idle / relaxed / point / grab / fist, gesture tap (telunjuk menekuk) saat klik, squeeze saat tombol.
    Laser keluar dari ujung telunjuk (bone `index_03_r`); frame jari diukur dari bone mesh (`MeasureHand`).
  - Input disuntik sebagai **key Quest yang dipetakan IMC_ZooVR** lewat `APlayerController::InputKey`
    (`OculusTouch_Right_Trigger_Click`, `_Right_A_Click`, `_Right_B_Click`, `_Left_Thumbstick_X/Y` (pasangan 2D),
    `_Right_Thumbstick_X/Y`, `_Left_Thumbstick_Click`), jadi `OnSelect*/OnMenu*/OnMove/OnSnapTurn/OnScroll` di
    `UZooVRComponent` dan `IA_Sprint`/`IA_PG_CloseMenu` milik Zoo berjalan persis seperti dengan controller sungguhan.
    Key dicari dari mapping context saat runtime (`KeyForAction`), bukan hard-code.
  - Target UI (`ui:<teks tombol/nama widget>`) dicari di panel VR dan panel info AR yang terlihat, titik dunia dihitung
    dari geometry widget (invers `UWidgetComponent::GetLocalHitLocation`); kartu yang terpotong scroll box di-scroll dulu
    (sweep laser sambil menahan trigger = hold-to-scroll VR). Jalan memakai NavMesh (`NavigationSystem`) atau rute
    Path Guide (`route`).
  - Capture: `zoo.vr.DemoCapture 1` / `-ZooVRDemoCapture` → tiap frame `FScreenshotRequest` (tanpa UI Slate) → delegate
    `OnScreenshotCaptured` → JPEG q95 lewat `ImageWriteQueue` (async) ke `Saved/DemoVideo/Frames/frame_%06d.jpg`.
    Bersama `-UseFixedTimeStep -FPS=30` tiap frame = 1/30 s game, jadi video halus walau render lebih lambat.
    Marker (`{"Type":"marker"}`) ditulis ke `Saved/DemoVideo/markers.txt` (frame, detik, waktu game, nama).
- `UZooVRComponent` (hook kecil): `bDemo`, `SetDemoPoses()`, getter rig/panel/action, `Camera->bLockToHmd = !bDemo`,
  tanpa model controller runtime saat demo (tangan mesh menggantikan).
- `Zoo.Build.cs` private deps + `NavigationSystem`, `Json`, `JsonUtilities`, `ImageWriteQueue`, `ImageWrapper`.

### Perbaikan VR yang ditemukan saat demo (berlaku juga untuk headset asli)
- **Klik/hover panel VR tidak pernah jalan**: `UWidgetInteractionComponent` (source Custom) mengambil panel dari
  `HitResult.GetComponent()`, tapi hit dari `LineTraceComponent` tidak mengisi `Component`. `UpdatePointerTarget` kini
  mengisi `Component`/`HitObjectHandle` panel yang dipilih. Laser juga memakai `VirtualUserIndex`/`PointerIndex` 1
  agar tidak berebut pointer virtual 0 dengan `MouseInteraction` exhibit AR.
- **Laser hanya saat diperlukan** (`UpdateLaserVisibility`, `LaserLingerTime` 0,6 s): tampil bila laser di atas widget
  interaktif panel VR, mengarah ke panel info / hologram exhibit AR, daftar "Mau ke mana?" terbuka, atau ada panel ekstra
  (video extend). Selebihnya `bShowDebug` dimatikan.
- **Video extend (EXTEND di kartu MEDIA) BELUM jalan di VR** (ditunda atas permintaan pemilik): `WBP_AR_MediaExtend`
  punya fungsi BP `FinishExtend` (latent) yang memanggil `RemoveFromParent` begitu widget tidak lagi di viewport, jadi relay
  ke panel VR langsung menutupnya (log `ZooVR panel: ... removed from its host by ...FinishExtend`). Percobaan menunda relay
  (`MediaExtendRelayDelay`) tidak menolong. Rencana: panel VR mencerminkan `ContentImage`-nya (widget tetap di viewport),
  klik laser pertama = `P` (putar), berikutnya = `X` (tutup) — kode `Click()`/`IsMediaExtendPanel`/`SimulateKey` sudah ada,
  bagian cermin belum. Di desktop tidak berubah.

### Perubahan UI/Duma bersama (kode Zoo, berlaku desktop & VR — juga untuk project asli)
- `Source/Zoo/UI/ZooViewClutter.h/.cpp` — `FZooViewClutter::Query()`: elemen HUD yang terlihat (anak langsung root
  overlay: Dialogue, Tracker, Toast, DestinationList, NavigateButton, ArrivalToast, MediaExtend) sebagai rect sudut
  pandang (yaw/pitch). Desktop: dari viewport + FOV kamera. VR: `UZooVRComponent::QueryClutter` (sumber terdaftar)
  memproyeksikan geometry widget di panel 3D ke sudut kamera.
- `AZooDumaController`: `FollowOffset {220,0,0}` (tengah pandangan, setinggi mata), `FacingOffset {160,0,5}`.
  `KeepClearOfHUD()` menggeser spot (samping dulu, baru sedikit ke atas, makin jauh; shift terakhir dicoba dulu agar
  tidak loncat-loncat) sampai piringan Duma (`ClearRadius` 45 cm + `ClearMargin` 2°) tidak menyentuh elemen HUD.
  Dipakai untuk spot follow tiap tick, penempatan pertama, blink facing/flying dan jump saat hilang.
- `Source/Zoo/UI/ZooQuestTrackerPeek.h/.cpp` + hook di `AZooHUD::BeginPlay`: panel misi (Tracker kanan atas) tampil
  6 s di awal dan tiap objective/quest baru, lalu melipat jadi tombol kecil "MISI ▸". Klik tombol → terbuka; 5 s setelah
  pointer (mouse / laser VR) tidak lagi di atasnya → melipat lagi. Dibangun runtime di canvas overlay (BP tidak diubah).
- Project asli (`C:\Users\USER\Perforce\EkaZoo2\Zoo`): 8 file yang sama (`UI/ZooHUD.*`, `Duma/ZooDumaController.*`,
  `UI/ZooViewClutter.*`, `UI/ZooQuestTrackerPeek.*`) disalin langsung (read-only dihapus, TANPA `p4 edit` karena sesi
  Perforce kedaluwarsa) atas permintaan pemilik, lalu `ZooEditor` di-compile di sana (sukses). Sebelum submit:
  `p4 login`, lalu `p4 reconcile Source/Zoo/UI/... Source/Zoo/Duma/...` (atau `p4 edit` 4 file lama + `p4 add` 4 file baru).
  `Tools/VRDemo/apply_to_original.cmd` tetap ada untuk menyalin ulang lewat p4.

### Revisi ke-3 (2026-10-24, kode bersama: salinan + project asli)
- `UZooHologramComponent` / `UZooHologramProfile.AmbientSeconds` (2,5 s): cakram/cincin hologram (AmbientEffect) kini hanya
  tampil saat spawn, berhenti `AmbientSeconds` setelah appear selesai (`TickAmbient`); appear berikutnya (setelah despawn)
  memutarnya lagi. Berlaku untuk Duma, jerapah, wombat.
- `AZooDuma::TickRibbons`: trail halus. `RibbonHoldSeconds` (1,2 s) menahan ribbon saat Duma melambat (jalan putus-putus
  tetap bertrail), `RibbonPace` meluruh pelan, fade-out lebih lambat (interp 2,5), dan saat mati ribbon dibiarkan selesai
  (`Deactivate`, bukan `DeactivateImmediate`; clear hanya saat teleport).
- `AZooDumaController::Perform`: saat Facing (bicara) Duma bergoyang kecil (`FacingSway` Y 9 cm / Z 5 cm + roll 3°).
- Level `L_Zoo` (hanya salinan): `AR_Wombat_Overlook` AnimalMesh scale 2,2 -> 6,6 (x3), `PanelOffset.Z` +220
  (`Tools/VRDemo/scale_wombat.py`). Project asli tidak diubah untuk ini.
- Skrip demo: teleport awal `-5320,-60,95` yaw 1,5 (gate tusk + pendopo + papan di tengah frame), saat dialog kepala
  melihat sedikit di bawah Duma agar Duma dan subtitle sama-sama terlihat.

### Revisi ke-4 (2026-10-24)
- `AZooDuma::ReleaseRibbons` (kode bersama): saat Duma loncat (blink dialog, teleport "lost") ribbon lama DILEPAS dari Duma
  (`DetachFromComponent` + `Deactivate`) dan memudar di tempat selama `RibbonFadeSeconds` (4 s), lalu dihapus; ribbon baru
  (duplikat komponen, asset + parameter sama) menempel di telinganya. Tidak ada lagi trail yang hilang seketika / reset.
  Ribbon menyala dengan `Activate(false)` (resume) agar jeda pendek tidak memutus garis.
- `UZooHologramComponent` (kode bersama): efek Niagara dipasang dengan skala dunia 1 (`SetUsingAbsoluteScale`), radius
  cincin dibatasi `UZooHologramProfile.MaxRingRadius` (default 160 cm; mesh wombat x3 tidak lagi membesarkan cincin), dan
  opsi `bRingOnGround` (+`GroundSearch` 800 cm): cincin + cakram diletakkan di tanah di bawah pivot (line trace) sehingga
  "memindai medan", tidak menutupi hewan. Aset profil (salinan, `Tools/VRDemo/hologram_profiles.py`): profil hewan
  `bRingOnGround=true`, `MaxRingRadius` 140; profil Duma tetap di pivot, `MaxRingRadius` 90.
- `WBP_AR_Media` (salinan, `Tools/VRDemo/media_card.py`): SizeBox `Body` ditinggikan +110 px agar baris PAUSE/EXTEND tidak
  terpotong (kartu ini bukan untuk di-scroll).
- Skrip demo: tiap kartu info (MEDIA, KARAKTERISTIK, GENERAL DESCRIPTION, HABITAT) di-scroll dulu sampai bawah (2x -5 notch,
  MEDIA -4) sebelum ke kartu berikutnya.
- Pipeline lengkap: `Tools/VRDemo/pipeline_full.ps1` (compile salinan -> commandlet aset -> sync 13 file C++ ke project asli +
  compile -> rekam -> encode). Jangan tutup jendela game saat merekam (menutupnya mengakhiri rekaman).

### Catatan wombat x3 (2026-10-24)
- Skala 6,6 (x3) membuat `AR_Wombat_Overlook` dinonaktifkan (`disabled: its animal or information card crosses the habitat
  boundary`), lalu setelah `HabitatBounds` diperluas (`Tools/VRDemo/fix_wombat_habitat.py`: x 400..5400, y -7200..-1800,
  z -1800..2600, corner 300) hologramnya memenuhi pandangan dari pagar (jarak 4 m) dan panel info tidak muncul. Dipakai
  skala 4,4 (x2) dengan exhibit digeser +120 cm menjauhi pagar dan panel +120 cm (`Tools/VRDemo/wombat_adjust.py`).
- Kartu MEDIA: `WBP_AR_Media.Body` tidak punya height override (dipakai BP saat buka), jadi perbaikan di runtime VR:
  `UZooVRComponent::ReplacePromptsIn` menaikkan `MinDesiredHeight` Body ke tinggi isi `Scroller` + 8 px (tidak ada tombol
  terpotong). Desktop/asli tidak diubah.

### Revisi wombat (2026-10-24 malam)
- Penyebab hologram wombat "tidak muncul": geometri `SK_Wombat` berada ~486 unit dari pivot-nya (sepanjang sumbu badan),
  sehingga skala mesh ikut melempar badan menjauh (x4,4 -> ~21 m di belakang pagar, x=4630). Skala sebelumnya pun
  tidak ada di tempat yang dikira.
- `Tools/VRDemo/wombat_place.py` (level `L_Zoo` salinan saja, BP tidak diubah): skala 6,6 (x3 sesuai permintaan awal),
  pivot dihitung supaya BADAN wombat tepat di (2950,-4320) di lantai kandang — tengah pandangan dari dek overlook, di bawah
  kartu info — menghadap pengunjung 3/4 (yaw -145), masuk berjalan 600 cm sepanjang arah hadap (`EntranceMoveSpeed` 450).
  `bEnableArrivalCameraFocus` instance dimatikan: fokus itu memutar kamera ke wombat asli yang dipilih, yang sekarang
  dituju ke pivot jauh (navigasinya gagal -> wombat asli kembali berkeliaran, tidak terlihat aneh).
- Kode bersama (salinan + asli): `UZooHologramComponent::GetEffectLocation` memakai pusat bounds aset mesh (bukan pivot)
  untuk cincin di tanah, jadi cincin spawn wombat ada di bawah badannya.
- Demo: step `probe <exhibit>` (log pivot/badan/mata/arah pandang) di `ZooVRDemoDirector`; uji cepat
  `Content/ZooVR/Demo/TestWombat.json`. Rekam ulang: `Tools/VRDemo/pipeline_wombat.ps1`, lalu
  `python Tools\VRDemo\splice_wombat.py` (frame sebelum marker `rute_ke_wombat` dari rekaman lama `Frames_run1`, sisanya baru).
- `PanelOffset.Z` kembali ke 330 (nilai asli): badan wombat tidak lagi di bawah kartu, jadi saat klik kartu wombat tetap
  terlihat di bawahnya.
- Hasil: `D:\Projek UE\ZooCinematic\Gameplay\ZooVR_Demo.mp4` (5:38, 10147 frame) + `ZooVR_Demo_timestamps.md`; video
  sebelumnya disimpan sebagai `ZooVR_Demo_sebelum_revisi_wombat.mp4`.

### Hasil (2026-10-24)
- `Saved/DemoVideo/ZooVR_Demo.mp4` — H.264 1920x1080 30 fps, 5:38 (10145 frame, CRF 18, ~880 MB), tanpa audio (rekaman terakhir 2026-10-24 sore, setelah revisi 3-5).
  Daftar timestamp per fitur: `Saved/DemoVideo/timestamps.md` (dari `markers.txt`). Frame sumber: `Saved/DemoVideo/Frames/`.
- Isi: spawn di depan gate (tusk + pendopo + papan Taman Safari), sambutan & dialog Navigasi Duma (klik lanjut), lihat
  sekeliling, jalan + snap turn 2x kanan/kiri + sprint, menu "Mau ke mana?" (A) + hover + scroll daftar + pilih Kandang
  Jerapah, rute 3D sampai toast tiba, exhibit AR jerapah (hologram materialize, MEDIA / KARAKTERISTIK / GENERAL DESCRIPTION /
  HABITAT + scroll isi), navigasi ke wombat: pilih, Stop guidance, buka/tutup (B), pilih lagi, rute, toast, exhibit AR wombat
  (panel yang sama), suasana penutup.
- Tidak terekam: panel video extend (lihat catatan di atas) dan tutup video; audio.
- Log diagnostik demo-only yang masih ada (hanya saat `bDemo`): `ZooVR AR click`, `ZooVR panel: relayed/closed/removed by`.

### Menjalankan ulang rekaman
1. Tutup editor. (Opsional) ubah `Content/ZooVR/Demo/ZooVRDemoScript.json`.
2. `Tools\VRDemo\run_demo.cmd [script.json]` =
   `UnrealEditor.exe Zoo.uproject /Game/Zoo/Maps/L_Zoo -game -nohmd -ZooVRDemo -ZooVRDemoCapture -ZooVRDemoScript=<json>
   -UseFixedTimeStep -FPS=30 -NoVSync -NoScreenMessages -Windowed -ResX=1920 -ResY=1080 -log -ABSLOG=Saved\DemoVideo\demo_run.log`
   (~28 fps real; 6 menit video ≈ 7–8 menit). Game keluar sendiri pada step `end`.
3. `python Tools\VRDemo\make_mp4.py` → `Saved\DemoVideo\ZooVR_Demo.mp4` (H.264 CRF 18, yuv420p, 30 fps) +
   `Saved\DemoVideo\timestamps.md` dari `markers.txt`. Opsi `--audio <wav>`.
- Skrip uji singkat: `Content/ZooVR/Demo/Calib.json`, `Test2..4.json`. Survey level/aset: `Tools/VRDemo/survey*.py`
  (`run_py.cmd`, editor tertutup).
- Tipe step JSON: `wait, look, walk, route, aim, click, press, scroll, snap, hands, rest, dialogue, wait_until, marker,
  stick, end` (field: `Type, Target, Offset, Seconds, Timeout, Yaw, Pitch, Radius, bSprint, bNav, bHead, Button,
  Notches, Cond, Left, Right, Name, Count`; lihat `ZooVRDemoDirector.h`).

## Cinematic & FPV (L_Zoo_Cine) — 2026-10-24

Dua video promosi direkam dengan Movie Render Queue dari map duplikat `/Game/Zoo/Maps/L_Zoo_Cine`. `L_Zoo`, `L_ZooEka`,
`L_ZooEko`, BP Zoo yang ada dan project Perforce asli tidak diubah. `vr.InstancedStereo` tetap False. Mode desktop/VR
normal tidak berubah: semua perilaku khusus cinematic ada di GameMode `AZooCinematicGameMode`, yang hanya dipakai sebagai
GameModeOverride di preset MRQ, plus console command probe yang dibungkus `#if !UE_BUILD_SHIPPING`.

### File yang ditambah / diubah
- `Zoo.uproject` — plugin `MovieRenderPipeline` aktif (backup: `Saved/Zoo.uproject.before_cine`).
- `Source/Zoo/Zoo.Build.cs` — dependensi `LevelSequence`, `MovieScene`.
- `Source/Zoo/Game/ZooCinematicGameMode.h/.cpp` (baru) — pawn spectator, tanpa HUD, tangan VR, laser, narrative
  atau input. Mengikuti marker ber-tag `CineVisitor` yang digerakkan sequence. Marker itu memicu aktivasi exhibit AR
  (hologram materialize + ring scan) dan rute PathGuide (tag `CineGuide=<PlaceTag>`, berurutan). Kartu info AR dibuka
  otomatis setelah `InformationOpenDelay`. Duma spawnable (tanpa controller) dipanggil `Appear()` supaya hologram
  dan trail pitanya aktif. Log berawalan `[ZooCinematic]`.
- `Source/Zoo/Game/ZooCinematicProbe.cpp` (baru, `#if !UE_BUILD_SHIPPING`) — `zoo.cine.Probe <in.json> <out.json>`:
  permukaan tanah/kanopi per grid, clearance jalur kamera (ray + sphere sweep WorldStatic/WorldDynamic), bounds aktor.
- `Content/Zoo/Maps/L_Zoo_Cine.umap` (baru, duplikat `L_Zoo`) — edit level:
  - `CINE_Visitor` (StaticMeshActor tersembunyi, movable, tanpa collision) dengan tag `CineVisitor`,
    `CineGuide=PathGuide.Place.Giraffe.Deck`, `CineGuide=PathGuide.Place.Wombat.Burrow`.
  - `AR_Giraffe_Deck`, `AR_Wombat_Overlook`: `walk_to_deck_center_before_approach` False,
    `enable_arrival_camera_focus` False, `animal_arrival_timeout` 0. Supaya hologram muncul tepat waktu untuk kamera.
    Penempatan wombat tidak diubah.
  - Jerapah (instance, hanya di map ini): `BP_Giraffe_2` wander 0 (target orbit FPV), `BP_Giraffe_3` wander 300
    + wait 0.3–0.8 s (shot jalan), `BP_Giraffe` 250, `BP_Giraffe_4` 200. Supaya hewan tetap di frame dan tidak
    menabrak jalur kamera.
- `Content/Cine/LS_Zoo_Cine_Cinematic` (30 fps, 4560 frame, 152 s, 16 shot) dan `Content/Cine/LS_Zoo_Cine_FPV`
  (60 fps, 3746 frame, 62.4 s, satu take): CineCamera spawnable per shot, camera cut, Duma spawnable dengan jalur
  terbang, marker visitor (key CONSTANT), matahari golden hour (hanya di sequence: `SUN_Directional` pitch -30 yaw 45,
  4100 K, intensitas 7), fade in/out.
- `Content/Cine/MRQ/MRQ_Cine_Preview_720p` (1280x720, temporal 1, setiap frame ke-10),
  `MRQ_Cine_Final_1080p` (1920x1080, temporal AA 4, warmup 32, Lumen HW RT + VSM, pool 2500 MB +
  `LimitPoolSizeToVRAM`), `MRQ_Cine_Final_1080p_FPV` (sama, temporal 3). Semua: GameModeOverride
  `ZooCinematicGameMode`, output PNG `Saved/Cine/Render/<Preview|Final>/<sequence>/`.
- `Tools/Cine/` (baru):
  - `dup_map.py` — duplikasi map.
  - `cine_plan.py` — perencana jalur kamera (Catmull-Rom, profil kecepatan, roll/pitch FPV, shake). Output
    `Saved/Cine/plan_*.json`. `--check` membaca hasil probe.
  - `build_cine.py` — bangun sequence dan preset dari plan, lalu edit level `L_Zoo_Cine`.
  - `probe.cmd`, `run_py.cmd`, `render.cmd`.
  - `make_mp4.py` — mix audio + encode + shot list.
  - `contact_sheet.py` — lembar review frame.
  - `survey_cine.py`, `probe_collision*.py`, `plot_map.py` — survey dan peta.

### Menjalankan ulang (editor harus tertutup)
1. `python Tools\Cine\cine_plan.py` — tulis plan dan input probe.
2. `Tools\Cine\probe.cmd <abs>\Saved\Cine\probe_paths_in.json <abs>\Saved\Cine\probe_paths_out.json`, lalu
   `python Tools\Cine\cine_plan.py --check`. Semua jalur harus OK: FPV ≥ 60 cm dari mesh solid, kamera lain ≥ 35 cm.
3. `Tools\Cine\run_py.cmd D:\Projek UE\ZooEka2VR\Tools\Cine\build_cine.py`. Path skrip harus absolut.
4. `Tools\Cine\render.cmd Cinematic|FPV Preview|Final`. Log ada di `Saved\Cine\render_<film>_<tag>.log`.
   `-FixedSeed` menjaga AI hewan sama antar render.
5. `python Tools\Cine\make_mp4.py Cinematic|FPV Final` menulis
   `D:\Projek UE\ZooCinematic\<Film>\ZooCine_<Film>.mp4` dan `_shotlist.md`. Encode H.264 CRF 16, yuv420p, AAC 192k.
   Audio adalah ambience CityPark (burung hutan, burung musim semi, air). FPV ditambah angin sintetis yang mengikuti
   kecepatan dan whoosh saat lewat dekat objek.
   Preview: `make_mp4.py <film> Preview` menulis `Saved\Cine\Preview_<film>.mp4`.

### Catatan
- Track focus distance per komponen gagal dibuat lewat Python ("Cannot call AddPossessable on a null object").
  Shot dengan target hewan memakai tracking focus, sisanya fokus manual statis di jarak tengah shot.
- Hewan (pawn) dan foliage tanpa collision tidak terdeteksi probe. Jarak ke jerapah dijaga dengan wander radius.
  Jarak ke hologram wombat (tidak ber-collision) dijaga dengan bounds `AR_Wombat_Overlook`.
- Tepi landscape ada di ±12600 cm. Di luar itu kosong, jadi kamera jangan menghadap ke sana.
- Shot 13 (kandang wombat) di-render ulang terpisah: `render.cmd Cinematic Shot13` (preset `MRQ_Cine_Final_1080p_Shot13`,
  frame 3300-3569 ke folder Final). Tracking `BP_ARWombat_0` diganti framing statis kandang, karena wombat masuk sarang
  atau berbelok cepat sehingga kamera look-at menyentak. Frame pertama range (3300) di-render ulang dengan sisa blur
  temporal shot sebelumnya, jadi frame 3301 disalin ke 3300.

### Hasil (2026-10-24)

| File | Resolusi / fps | Durasi | Frame | Audio |
|---|---|---|---|---|
| `D:\Projek UE\ZooCinematic\Cinematic\ZooCine_Cinematic.mp4` (+ `_shotlist.md`) | 1920x1080, 30 fps | 3:09 | 5670 | ambience (revisi malam; render Final ~50 menit) |
| `D:\Projek UE\ZooCinematic\FPV\ZooCine_FPV.mp4` (+ `_shotlist.md`) | 1920x1080, 60 fps | 1:02.4 | 3746 | ambience + angin/whoosh, mean -21 dB |

- Waktu render Final di RTX 3070:
  - Cinematic 39 menit (temporal 4, sekitar 1 frame/detik, VRAM sekitar 7.6/8 GB).
  - FPV 20 menit (temporal 3).
  - Patch shot 13 sekitar 2 menit.
  - FPV 60 fps masih wajar, jadi tidak diturunkan ke 30 fps.

### Revisi 2026-10-24 malam: demo kartu AR + shot follow Duma (Cinematic saja)
- `ZooCinematicGameMode` (C++) sekarang bisa mendemokan kartu info AR. Kartu dibuka satu per satu dengan klik
  `HeaderButton`. Accordion di Blueprint kartu tetap jalan, jadi kartu sebelumnya dilipat dulu. Isi kartu yang terbuka
  di-scroll dari atas ke bawah (`UScrollBox::SetScrollOffset`).
  - Jadwalnya dari marker level baru `CINE_Panels` (tag `CinePanels`, `CinePanelExhibit=AR_Giraffe_Deck`,
    `CinePanelExhibit=AR_Wombat_Overlook`). Lokasi marker di-key oleh sequence:
    - X = exhibit (key CONSTANT).
    - Y = kartu 1–4 (MEDIA, KARAKTERISTIK, GENERAL DESCRIPTION, HABITAT; 0 = semua terlipat; key CONSTANT).
    - Z = posisi scroll dalam % (key LINEAR).
  - Kartu dicari lewat `TObjectIterator<UUserWidget>` + `IsIn(panel)`. Walk `WidgetTree` tidak bisa dipakai karena
    tidak masuk ke child user widget.
  - Log `[ZooCinematic] Card placement ...` mencatat posisi dunia tiap kartu. Data ini dipakai untuk framing close-up
    (panel AR menghadap ke kamera, jadi kamera close-up ditaruh di garis kamera yang sama).
  - Tanpa marker `CinePanels` atau di luar render, tidak ada yang berubah. Mode desktop/VR tidak memakai GameMode ini.
- Timeline Cinematic menjadi 189 s (5670 frame, 18 shot):
  - `05_Duma_Follow` (39–51 s) menggantikan `05_Canal_Flyby`. Kamera mengejar Duma dari belakang di atas kanal dan
    mendekat dari 6.5 m ke 3 m (zoom in). Mulai 45.5 s kamera mengerem dan diam, lalu Duma terbang terus naik melewati
    pohon tanpa diikuti.
  - `11`/`11b` (94–120 s) dan `15`/`15b` (150–176 s): wide 2 detik, lalu close-up 45 mm yang pan dari kartu ke kartu.
    Kartu diklik tiap 6.5 s (jerapah 95/101.5/108/114.5 s, wombat 151/157.5/164/170.5 s).
  - Shot 06–16 digeser. Jadwal visitor marker dan spawn Duma kolam ikut digeser.
  - Jendela audio di `make_mp4.py` sekarang dihitung dari waktu shot.
- Tidak didemokan: tombol EXTEND di kartu MEDIA (layar penuh `WBP_AR_MediaExtend`). Widget ini masuk ke viewport,
  bukan ke panel dunia, sehingga tidak ikut ter-render MRQ tanpa UI renderer. Video MEDIA tetap diputar di dalam kartu.
- Patch hewan (AI hewan beda tiap render, juga dengan `-FixedSeed`):
  - Preset `MRQ_Cine_Final_1080p_Shot08` / `_Shot13` (range dari plan). Jalankan `render.cmd Cinematic Shot08`
    atau `Shot13`. Hasilnya menimpa frame shot itu saja di folder Final.
  - Shot 08 di-render ulang. Render penuh menampilkan belakang kepala lalu whip di akhir shot. Patch memberi close-up
    kepala yang bagus, tetapi ada whip 0.9 s di tengah (frame 2153-2179). Frame whip itu diganti crossfade
    2152 -> 2180.
  - Shot 13 di-render ulang. Wombat hanya terlihat sekitar 130-132.4 s, lalu masuk liang (perilaku AI, tidak diubah).
  - Frame pertama render range kotor (ghost frame sebelumnya). 2040 dan 3900 diganti dengan 2041 / 3901.
  - Cadangan frame lama: `Saved/Cine/Render/Backup_Shot08_13` (render penuh) dan `Backup_Patch` (frame patch
    sebelum crossfade).
