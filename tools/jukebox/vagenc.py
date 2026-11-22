#!/usr/bin/env python3
# vagenc.py - Ses dosyasi -> PS2 VAG donusturucu (NFSHP2 Jukebox icin)
#
# Kullanim:
#   python3 vagenc.py giris.mp3 [cikis.vag] [--hiz 32000] [--ad "Sarki Adi"]
#   python3 vagenc.py giris.wav [cikis.vag] [--hiz 32000] [--ad "Sarki Adi"]
#
# Giris: MP3 veya WAV (16-bit PCM; mono/stereo, herhangi bir ornek hizi).
#   MP3 icin ffmpeg pakete dahildir (minimal derleme, yalniz MP3 coz + PCM):
#   Windows x86_64 : ffmpeg_win.exe   | Linux x86_64 : ffmpeg_linux
#   Android arm64  : ffmpeg_android (Termux)
#   Paketli ffmpeg calismazsa sistemdeki ffmpeg kullanilir; o da yoksa
#   asagidaki kurulum yontemiyle kurulur. macOS'ta paketli binary yok.
#   Ornek hizi 32000 degilse 32000 Hz'e cevrilir (dogru calinmasi icin sart).
#   Mono ise iki kanala da aynisi yazilir (stereo).
# Cikis: 48 bayt VAGp basligi + PS2 SPU ADPCM stereo veri
#   (0x8000 sol + 0x8000 sag blok ciftleri, jbstream.c ile uyumlu).
#   Veri, 0x10000 baytlik ciftlere tamamlanir; son gercek kareye
#   bayrak 1, sonraki dolgu karelerine bayrak 7 yazilir.
#
# MP3 icin ffmpeg pakete dahildir (minimal derleme, FFmpeg 7.1):
#   ffmpeg_win.exe (Windows x86_64, 1.3 MB), ffmpeg_linux (Linux x86_64, 1.4 MB),
#   ffmpeg_android (Android arm64, FFmpeg 8.0.1, 15 MB).
#   Paketli ffmpeg calismazsa (veya silindiyse) sistemdeki ffmpeg kullanilir.
#   Hicbiri yoksa once MP3'u WAV'a cevir (Audacity veya bir donusturucu
#   uygulamasıyla) ve WAV ver, ya da ffmpeg kur:
#   - Termux (Android):  pkg install ffmpeg
#   - Debian/Ubuntu:     sudo apt install ffmpeg
#   - Windows:           https://ffmpeg.org/download.html (veya gyan.dev)
#   - macOS:             brew install ffmpeg
#
# Yalnizca Python standart kutuphanesi kullanir (numpy gerekmez).
# Uzun sarkilarda yavas olabilir (birkaç dakika); bu normaldir.

import array
import math
import os
import platform
import shutil
import struct
import subprocess
import sys
import wave

COEF = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))
FRAME = 16
SAMPLES = 28
BLOCK_FRAMES = 2048
BLOCK = 0x8000
PAIR = 0x10000
RATE = 32000


def clamp16(v):
    if v > 32767:
        return 32767
    if v < -32768:
        return -32768
    return v


def read_wav(path):
    """WAV oku -> (ornek_hizi, kanal_sayisi, sol array('h'), sag array('h')).
    16-bit PCM disindaki WAV'ler icin None doner (ffmpeg'e dusulur)."""
    try:
        w = wave.open(path, 'rb')
    except Exception:
        return None
    nch = w.getnchannels()
    sw = w.getsampwidth()
    rate = w.getframerate()
    n = w.getnframes()
    raw = w.readframes(n)
    w.close()
    if sw != 2 or nch not in (1, 2) or n == 0:
        return None
    pcm = array.array('h')
    pcm.frombytes(raw)
    if sys.byteorder == 'big':
        pcm.byteswap()
    if nch == 1:
        left = pcm
        right = array.array('h', pcm)
    else:
        left = array.array('h', pcm[0::2])
        right = array.array('h', pcm[1::2])
    return rate, nch, left, right


def find_ffmpegs():
    """ffmpeg adaylari (sirasiyla): once platforma uygun paketli binary,
    sonra sistemdeki ffmpeg. Paketli dosya calistirilabilir degilse
    +x yapmayi dener (Windows'ta anlamsiz, zararsiz).
    Paketli binary'ler (hepsi minimal: yalniz MP3 coz + PCM cikar):
      Windows x86_64 : ffmpeg_win.exe
      Linux x86_64   : ffmpeg_linux
      Android arm64  : ffmpeg_android (Termux; v3'teki 'ffmpeg' de kabul edilir)
    Diger platformlar (macOS, aarch64 Linux): yalniz sistem ffmpeg'i."""
    here = os.path.dirname(os.path.abspath(__file__))
    plat = sys.platform
    mach = platform.machine().lower()
    is_termux = ('TERMUX_VERSION' in os.environ) or os.path.isdir('/data/data/com.termux')
    names = []
    if plat == 'win32':
        names = ['ffmpeg_win.exe', 'ffmpeg.exe']
    elif plat == 'linux':
        if is_termux:
            names = ['ffmpeg_android', 'ffmpeg']
        elif mach in ('x86_64', 'amd64'):
            names = ['ffmpeg_linux']
        # aarch64 Linux (orn. RG353V): paketli binary yok, sistem ffmpeg'i
    cands = []
    for name in names:
        p = os.path.join(here, name)
        if os.path.isfile(p):
            if plat != 'win32':
                try:
                    if not os.access(p, os.X_OK):
                        os.chmod(p, 0o755)
                except OSError:
                    pass
            cands.append(p)
    sys_ff = shutil.which('ffmpeg')
    if sys_ff and sys_ff not in cands:
        cands.append(sys_ff)
    return cands


def read_ffmpeg(path, rate):
    """ffmpeg ile herhangi bir ses dosyasini coz -> (rate, 2, sol, sag).
    ffmpeg dogrudan hedef hiza + stereo'ya cevirir.
    Adaylari sirasiyla dener (paketli -> sistem); hepsi basarisiz olursa None."""
    cands = find_ffmpegs()
    if not cands:
        return None
    last_err = ''
    for ff in cands:
        cmd = [ff, '-v', 'error', '-i', path,
               '-f', 's16le', '-ac', '2', '-ar', str(rate), '-']
        try:
            proc = subprocess.run(cmd, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=3600)
        except (OSError, subprocess.SubprocessError) as e:
            last_err = 'ffmpeg calistirilamadi (%s): %s' % (ff, e)
            continue
        if proc.returncode != 0 or not proc.stdout:
            err = proc.stderr.decode('utf-8', 'replace').strip().splitlines()
            last_err = err[-1] if err else 'bilinmeyen hata'
            continue
        raw = proc.stdout
        break
    else:
        raise SystemExit('ffmpeg dosyayi cozemedi (%s): %s' % (path, last_err))
    raw = proc.stdout
    if len(raw) % 4 != 0:
        raw = raw[:len(raw) - (len(raw) % 4)]
    pcm = array.array('h')
    pcm.frombytes(raw)
    if sys.byteorder == 'big':
        pcm.byteswap()
    left = array.array('h', pcm[0::2])
    right = array.array('h', pcm[1::2])
    return rate, 2, left, right


def read_audio(path, rate):
    """Ses dosyasi oku (WAV veya MP3/diger). (rate, nch, sol, sag) doner."""
    ext = os.path.splitext(path)[1].lower()
    is_wav = ext == '.wav'
    if is_wav:
        got = read_wav(path)
        if got is not None:
            return got
        # 16-bit PCM olmayan WAV: ffmpeg'e dus
        print('  (WAV 16-bit PCM degil, ffmpeg ile cozuluyor...)')
    got = read_ffmpeg(path, rate)
    if got is not None:
        return got
    if is_wav:
        raise SystemExit(
            'Bu WAV 16-bit PCM degil ve ffmpeg bulunamadi.\n'
            '  Cozum: dosyayi 16-bit PCM WAV olarak yeniden kaydet, '
            'veya ffmpeg kur (Termux: pkg install ffmpeg).')
    raise SystemExit(
        'MP3/diger format icin ffmpeg gerekli ama bulunamadi.\n'
        '  Paketli ffmpeg calismadiysa, '
        'Termux\'ta sunu kur: pkg install ffmpeg\n'
        '  veya once MP3\'u WAV\'a cevirip WAV ver.')


def resample(pcm, src_rate, dst_rate):
    """Dogru orantili (linear) yeniden ornekleme. Ayni hizda aynen doner."""
    if src_rate == dst_rate:
        return pcm
    n_in = len(pcm)
    n_out = int(round(n_in * dst_rate / float(src_rate)))
    if n_out < 1:
        n_out = 1
    out = array.array('h', bytes(2 * n_out))
    # dogrusal enterpolasyon
    for i in range(n_out):
        pos = i * (n_in - 1) / float(n_out - 1) if n_out > 1 else 0.0
        j = int(pos)
        frac = pos - j
        if j + 1 < n_in:
            v = pcm[j] + (pcm[j + 1] - pcm[j]) * frac
        else:
            v = pcm[j]
        out[i] = clamp16(int(round(v)))
    return out


def estimate_shift(samples, f0, f1, h1, h2):
    """Orijinal orneklerle tahmini kalinti araligindan shift bul (0-12)."""
    oh1, oh2 = h1, h2
    maxabs = 0
    for s in samples:
        pred = (f0 * oh1 + f1 * oh2 + 32) >> 6
        d = s - pred
        a = -d if d < 0 else d
        if a > maxabs:
            maxabs = a
        oh2, oh1 = oh1, s
    if maxabs == 0:
        return 12
    # en buyuk shift: maxabs << shift <= 7*4096 = 28672
    # bit uzunlugundan: shift = 12 - (bitlen-3), [0,12] araliginda
    sh = 12 - (maxabs.bit_length() - 3)
    if sh < 0:
        sh = 0
    elif sh > 12:
        sh = 12
    return sh


def encode_frame(samples, h1, h2):
    """28 orneklik kareyi kodla. (16 bayt kare, yh1, yh2) doner."""
    best = None
    best_err = None
    for p in range(5):
        f0, f1 = COEF[p]
        sh = estimate_shift(samples, f0, f1, h1, h2)
        # tek simülasyon: tahmini shift ile kodla, hatayi olc
        hh1, hh2 = h1, h2
        nib = bytearray(14)
        err = 0
        for i, s in enumerate(samples):
            pred = (f0 * hh1 + f1 * hh2 + 32) >> 6
            d = s - pred
            v = (d << sh) >> 12
            if v > 7:
                v = 7
            elif v < -8:
                v = -8
            r = ((v << 12) >> sh) + pred
            r = clamp16(r)
            err += (s - r) * (s - r)
            # nibble: cift index alt, tek index ust
            if i % 2 == 0:
                nib[i // 2] = v & 0x0F
            else:
                nib[i // 2] |= (v & 0x0F) << 4
            hh2, hh1 = hh1, r
        if best_err is None or err < best_err:
            best_err = err
            frame = bytearray(16)
            frame[0] = (p << 4) | sh
            frame[1] = 0
            frame[2:16] = nib
            best = (bytes(frame), hh1, hh2)
    return best


def encode_channel(pcm):
    """Bir kanalin tum orneklerini ADPCM karelerine kodla.
    Donus: (kare baytlari [bayrak 0], kare sayisi). Bayraklar sonradan isaretlenir."""
    n = len(pcm)
    nframes = (n + SAMPLES - 1) // SAMPLES
    out = bytearray(nframes * FRAME)
    h1 = h2 = 0
    for fi in range(nframes):
        chunk = pcm[fi * SAMPLES:(fi + 1) * SAMPLES]
        if len(chunk) < SAMPLES:
            # son eksik kareyi 0 ile doldur
            chunk = chunk + array.array('h', bytes(2 * (SAMPLES - len(chunk))))
        frame, h1, h2 = encode_frame(chunk, h1, h2)
        out[fi * FRAME:(fi + 1) * FRAME] = frame
    return bytes(out), nframes


def interleave(left_frames, right_frames, nframes):
    """Sol/sag kare akislarini 0x8000 blok ciftlerine diz.
    Donus: (veri baytlari [ciftlere tamamlanmis], gercek_kare_sayisi)."""
    nblocks = (nframes + BLOCK_FRAMES - 1) // BLOCK_FRAMES
    total_frames = nblocks * BLOCK_FRAMES
    # her kanali bloklara tamamla (dolgu kareleri bayrak 7 olacak)
    def pad(frames_bytes, nframes):
        b = bytearray(total_frames * FRAME)
        b[:nframes * FRAME] = frames_bytes
        # dolgu kareleri: bayrak 7 (bayt1=7), gerisi 0
        for fi in range(nframes, total_frames):
            b[fi * FRAME + 1] = 7
        return bytes(b)
    lb = pad(left_frames, nframes)
    rb = pad(right_frames, nframes)
    # son gercek kareye bayrak 1 (her iki kanalda da)
    if nframes > 0:
        li = (nframes - 1) * FRAME
        lb = lb[:li + 1] + bytes([1]) + lb[li + 2:]
        rb = rb[:li + 1] + bytes([1]) + rb[li + 2:]
    # ciftle: [sol blok][sag blok] tekrarla
    out = bytearray(nblocks * PAIR)
    for bi in range(nblocks):
        o = bi * PAIR
        out[o:o + BLOCK] = lb[bi * BLOCK:(bi + 1) * BLOCK]
        out[o + BLOCK:o + PAIR] = rb[bi * BLOCK:(bi + 1) * BLOCK]
    return bytes(out), nframes


def vag_header(data_size, rate, name):
    """48 bayt VAGp basligi."""
    h = bytearray(48)
    h[0:4] = b'VAGp'
    struct.pack_into('>I', h, 4, 2)            # surum (retail: 2)
    struct.pack_into('>I', h, 8, 0)            # ayrildi
    struct.pack_into('>I', h, 12, data_size)   # veri boyutu (BE)
    struct.pack_into('>I', h, 16, rate)        # ornek hizi (BE)
    struct.pack_into('>I', h, 20, 0)
    struct.pack_into('>I', h, 24, 0)
    struct.pack_into('>I', h, 28, 0)
    nb = name.encode('ascii', 'replace')[:16]
    h[32:32 + len(nb)] = nb
    return bytes(h)


def main():
    args = sys.argv[1:]
    if not args or args[0] in ('-h', '--help'):
        print('Kullanim: python3 vagenc.py giris.mp3 [cikis.vag] [--hiz 32000] [--ad "Sarki Adi"]')
        print('          python3 vagenc.py giris.wav [cikis.vag] [--hiz 32000] [--ad "Sarki Adi"]')
        print('  giris     : MP3 veya 16-bit PCM WAV (mono/stereo, herhangi bir hiz)')
        print('              MP3 icin ffmpeg pakette var (Windows/Linux/Android); calismazsa sistem ffmpeg\'i')
        print('  cikis.vag : verilmezse giris adiyla .vag olur')
        print('  --hiz     : hedef ornek hizi (varsayilan 32000, JBC ile ayni olmali)')
        print('  --ad      : VAG basligina yazilacak ad (en fazla 16 karakter)')
        return 1
    src = args[0]
    dst = None
    rate = RATE
    name = ''
    i = 1
    while i < len(args):
        a = args[i]
        if a == '--hiz' and i + 1 < len(args):
            rate = int(args[i + 1])
            i += 2
        elif a == '--ad' and i + 1 < len(args):
            name = args[i + 1]
            i += 2
        elif dst is None:
            dst = a
            i += 1
        else:
            raise SystemExit('Bilinmeyen arguman: %s' % a)
    if dst is None:
        base = os.path.splitext(src)[0]
        dst = base + '.vag'
    if not name:
        name = os.path.splitext(os.path.basename(src))[0][:16]
    if not os.path.isfile(src):
        raise SystemExit('Dosya bulunamadi: %s' % src)

    print('Okunuyor: %s' % src)
    srate, nch, left, right = read_audio(src, rate)
    print('  Ses: %d Hz, %s, %d ornek/kanal (%.1f sn)' %
          (srate, 'stereo' if nch == 2 else 'mono', len(left), len(left) / float(srate)))
    if srate != rate:
        print('  %d Hz -> %d Hz cevriliyor...' % (srate, rate))
        left = resample(left, srate, rate)
        right = resample(right, srate, rate)
        print('  cevrildi: %d ornek/kanal' % len(left))
    print('Kodlaniyor (PS2 ADPCM, sol kanal)...')
    lf, nframes = encode_channel(left)
    print('Kodlaniyor (sag kanal)...')
    rf, nframes2 = encode_channel(right)
    assert nframes == nframes2
    print('  %d kare/kanal, %.1f sn' % (nframes, nframes * SAMPLES / float(rate)))
    data, _ = interleave(lf, rf, nframes)
    print('  veri: %d bayt (%d cift)' % (len(data), len(data) // PAIR))
    vag = vag_header(len(data), rate, name) + data
    with open(dst, 'wb') as f:
        f.write(vag)
    print('Yazildi: %s (%d bayt, VAGp, %d Hz)' % (dst, len(vag), rate))
    return 0


if __name__ == '__main__':
    sys.exit(main())
