# NFSHP2 Jukebox Araçları v4

Kendi ISO'ndan çıkardığın `JukeboxM.JBC` dosyasındaki şarkıları, kendi müziklerinle değiştirmek için.

**Önemli:** Bu araçlar yalnızca kendi diskinin JBC dosyasını değiştirir. Başkasının müziğini dağıtma; telifli şarkıları herkese açık paylaşma.

## İçindekiler

- `jukebox_liste.py` — JBC'deki şarkıları listeler.
- `vagenc.py` — MP3/WAV dosyasını oyunun anladığı VAG dosyasına çevirir.
- `ffmpeg_win.exe` — MP3 çözmek için (Windows x86_64, pakete dahil).
- `ffmpeg_linux` — MP3 çözmek için (Linux x86_64, pakete dahil).
- `ffmpeg_android` — MP3 çözmek için (Android arm64 / Termux, pakete dahil).
- `jukebox_yeniden_kur.py` — JBC'yi yeni şarkılarla yeniden kurar.

Bilgisayarda da telefonda da çalışır: Windows, Linux ve Android (Termux).
Kurulum gerekmez — `vagenc.py` işletim sistemine uygun ffmpeg'i paketten
otomatik seçer. macOS'ta paketli ffmpeg yok, sistemdeki kullanılır.

Gereken tek şey Python 3 (3.8+). Başka hiçbir kurulum gerekmez.

## Adım adım kullanım

### 1. Şarkıları listele

```
python3 jukebox_liste.py JukeboxM.JBC
```

Çıktı numaralı listedir (1-23). Değiştirmek istediğin şarkının numarasını not et. Liste ayrıca `jukebox_sarkilar.txt` dosyasına yazılır.

### 2. Yeni şarkıyı VAG'a çevir

Müziğin MP3 veya WAV olabilir (mono/stereo, herhangi bir örnek hızı). MP3'ü doğrudan ver, WAV'a çevirmene gerek yok:

```
python3 vagenc.py sarkin.mp3
python3 vagenc.py sarkin.wav
```

Çıktı `sarkin.vag` olur. Örnek hızı otomatik 32000 Hz'e çevrilir (oyun 32000 Hz ister). İsim vermek istersen:

```
python3 vagenc.py sarkin.mp3 benim_sarkim.vag --ad "Benim Sarkim"
```

MP3 için ffmpeg pakete dahildir (minimal derleme, yalnız MP3 çöz + PCM):
- Windows x86_64: `ffmpeg_win.exe` (1.3 MB)
- Linux x86_64: `ffmpeg_linux` (1.4 MB)
- Android arm64: `ffmpeg_android` (15 MB, FFmpeg 8.0.1)

Kurulum gerekmez — `vagenc.py` işletim sistemine uygun olanı otomatik seçer,
o çalışmazsa sistemdeki ffmpeg'e düşer. Windows'ta `python3 vagenc.py` yerine
`python vagenc.py` yazılır (Python kurulu olmalı: https://www.python.org).

Paketli ffmpeg çalışmazsa (veya silindiyse) kendin kur:
- Termux (Android): `pkg install ffmpeg`
- Debian/Ubuntu: `sudo apt install ffmpeg`
- Windows: https://ffmpeg.org/download.html (veya gyan.dev)
- macOS: `brew install ffmpeg`

Hiçbiri yoksa MP3'ü önce WAV'a çevir (Audacity veya bir dönüştürücü uygulamasıyla) ve WAV ver. WAV için ffmpeg gerekmez.

Bir şarkı yaklaşık 1 dakika sürer (3 dakikalık şarkı ~1 dk). Bu normaldir.

### 3. Eşleşme dosyasını yaz

`eslesme.txt` adında bir metin dosyası yap. Her satır: `slot_no vag_dosyasi ["Yeni Ad"]`

```
2 benim_sarkim.vag "Sanatci-Sarki--Pop-4"
5 diger_sarkim.vag
```

- `slot_no`: listedeki şarkı numarası (1-23).
- `vag_dosyasi`: 2. adımda ürettiğin `.vag` dosyası.
- `"Yeni Ad"` (isteğe bağlı): oyunda Seçenekler > Ses'te görünecek ad. Biçim: `Sanatci-Baslik-Album-Tur-Durum`. Tür: Pop, Rock veya Techno. Durum: 0-5 (0=hiçbir listede yok, 2=yarış, 3=menü, 4=menü+yarış, 5=hepsi). Verilmezse eski ad kalır.
- `#` ile başlayan satırlar yorumdur.

### 4. JBC'yi yeniden kur

```
python3 jukebox_yeniden_kur.py JukeboxM.JBC eslesme.txt JukeboxM_yeni.JBC
```

Çıktı `JukeboxM_yeni.JBC` olur. Orijinal dosyana dokunulmaz. Önce yapıyı kontrol etmek istersen:

```
python3 jukebox_yeniden_kur.py --kontrol JukeboxM.JBC
```

"YAPI: TAMAM" demesi lazım. Uymuyorsa yeniden kurma, önce dosyana bak.

### 5. Oyuna koy

`JukeboxM_yeni.JBC` dosyasını oyunun `gamedata/SOUND/` klasöründeki `JUKEBOXM.JBC` dosyasının yerine koy (önce orijinali yedekle). Dosya adının `JUKEBOXM.JBC` olduğundan emin ol.

## Notlar

- Değişmeyen şarkılar bayt bayt aynen kalır; yalnızca seçtiğin slotlar değişir.
- Her şarkının arkasındaki 4000 baytlık kuyruk orijinalden aynen kopyalanır (ne olduğu bilinmiyor, silinmez).
- Yeni şarkı eskisinin uzunluğunda olmak zorunda değil; dosya boyutu değişebilir, sonraki şarkıların ofsetleri otomatik yeniden yazılır.
- VAG hızı JBC hızıyla (32000 Hz) aynı olmalı; değilse oyun hızlı/yavaş çalar. `vagenc.py` bunu otomatik yapar.
- Retail JBC'de VAG başlığındaki veri boyutu ile indexteki boyut farklı olabilir; bu normaldir, araç artık bunu hata saymaz.

## Sorun olursa

- "Dosya bulunamadi": dosya adını/yolunu doğru yaz.
- "YAPI: UYMUYOR": JBC dosyan bozuk veya eksik olabilir; tam dosyayla dene (ilk 1MB yetmez).
- "VAG hizi ... != JBC hizi": VAG'ı `vagenc.py` ile yeniden üret (hızı otomatik 32000 yapar).
- Oyun şarkıyı çalmıyorsa: önce `--kontrol` ile yapıyı doğrula, sonra şarkının VAG'ını `vagenc.py` ile yeniden üret.

v4 — 2026-11-22 (v3'ten farkı: ffmpeg Windows + Linux + Android, bilgisayarda da çalışır)
