#!/usr/bin/env python3
# JukeboxM.JBC yeniden kurucu (NFSHP2) - sarki degisimleri icin
# Kullanim:
#   python3 jukebox_yeniden_kur.py JukeboxM.JBC eslesme.txt [cikis.JBC]
#   python3 jukebox_yeniden_kur.py --kontrol JukeboxM.JBC
#
# eslesme.txt satirlari:  slot_no yeni_sarki.vag ["Yeni Ad"]
#   slot_no: 1..sarki sayisi (jukebox_sarkilar.txt'teki sira)
#   yeni_sarki.vag: vagenc ile uretilmis VAG dosyasi (48 bayt baslik + veri)
#   "Yeni Ad" (opsiyonel): oyunda Secenekler > Ses'te gorunecek ad.
#   Verilmezse eski ad aynen kalir. Satir basinda # = yorum.
# Listede olmayan slotlar bayt bayt aynen kopyalanir.
#
# FORMAT (jukebox_liste.py + ofset farklarindan dogrulandi):
#   [32 bayt baslik] [index_size bayt index] [23 sarki slotu]
#   Baslik (8 x LE uint32): [2]=ornek hizi, [3]=kategori sayisi,
#   [4]=index boyutu (orijinalde 4096).
#   Index: 64 bayt kategori (sayi + ad) + 23 x 96 bayt sarki
#   (ofset@0, boyut@4, sync@8, ad@12..96). 64 + 23*96 = 2272 bayt;
#   indexin kalan 1824 bayti ACIKLANMAMIS - aynen korunur, dokunulmaz.
#   Sarki slotu: [48 bayt VAG basligi][boyut bayt VAG verisi]
#   [4000 bayt kuyruk]. Komsu ofset farki = boyut + 4048 her slotta
#   (48 + 4000). Kuyrugun ne oldugu BILINMIYOR (senkron/dongu verisi
#   olabilir) - bu betik her slotun kuyrugunu ORIJINALDEN aynen
#   kopyalar, yeni sarkinin arkasina ekler. Asla silmez/uretimez.
#   NOT (retail JBC): VAG basligindaki veri boyutu (ofset 12) ham ADPCM
#   boyutudur; indexteki `boyut` ise ciftlere tamamlanmis (+48) boyuttur.
#   Bu fark normaldir - betik artik bunu hata saymaz, yalniz bildirir.
#   Indexteki `boyut` alani VAG veri boyutudur (48 bayt baslik haric);
#   bu, vagenc'in yazdigi VAGp basligindaki veri boyutu alaniyla
#   (ofset 12, BE uint32) karsilastirilarak dogrulanir.
#
# ONEMLI - bu betik calismadan once gercek dosyada dogrulanmasi
# gerekenler (ilk 1MB + tam dosya ile):
#   1. Tum slotlarda ofset farki = boyut + 4048 mu? (--kontrol bunu
#      dener; uymazsa betik HICBIR SEY yazmadan durur.)
#   2. 1. sarkinin VAG basligindaki veri boyutu (ofset 12) indexteki
#      boyuta esit mi? Esit degilse `boyut` alani baska anlama
#      geliyordur - betik uyarir, devam etmeden once bakilmali.
#   3. Yeni VAG'larin ornek hizi JBC basligindaki hiza esit olmali;
#      degilse oyun hizli/yavas calar. Betik uyarir.
#   4. vagenc STEREO uretir (0x8000 sol + 0x8000 sag blok ciftleri,
#      jbstream.h/jbs_decode ile dogrulandi). Betik kanal sayisini
#      bilemez, yalniz dosya boyutunu yazar; VAG hizi JBC basligiyla
#      karsilastirilir (uyusmazsa uyari verir).
#
# Cikis dosyasi orijinalle ayni boyutta OLMAYABILIR (yeni sarkilar
# farkli uzunlukta). Sonraki slotlarin ofsetleri yeniden yazilir.
# Baslik + indexin geri kalani + degismeyen slotlar bayt bayt aynidir.

import struct
import sys
import os

HDR = 32
CAT = 64
ENTRY = 96
VAG_HDR = 48
TAIL = 4000
GAP = VAG_HDR + TAIL  # 4048

def rd32(b, off):
    return struct.unpack_from('<I', b, off)[0]

def rd32be(b, off):
    return struct.unpack_from('>I', b, off)[0]

def cstr(raw):
    return raw.split(b'\x00', 1)[0].decode('ascii', 'replace')

def parse_jbc(path):
    with open(path, 'rb') as f:
        data = f.read()
    if len(data) < HDR:
        raise SystemExit('Dosya cok kisa: %s' % path)
    header = struct.unpack('<8I', data[:HDR])
    rate = header[2]
    cat_count = header[3]
    index_size = header[4]
    if cat_count != 1:
        print('UYARI: kategori sayisi 1 degil: %d (betik 1 kategori varsayar)' % cat_count)
    index = bytearray(data[HDR:HDR + index_size])
    if len(index) < index_size:
        raise SystemExit('Index kesik: %d / %d bayt' % (len(index), index_size))
    cat_tunes = rd32(index, 0)
    entries = []
    at = CAT
    for t in range(cat_tunes):
        e = index[at:at + ENTRY]
        at += ENTRY
        entries.append({
            'no': t + 1,
            'off': rd32(e, 0),
            'size': rd32(e, 4),
            'sync': struct.unpack_from('<I', e, 8)[0],
            'name': cstr(e[12:96]),
            'raw': bytes(e),
        })
    return data, header, rate, index_size, index, entries, cat_tunes

def check_structure(data, entries):
    """Ofset/boyut tutarliligini dogrula. (True, mesajlar) doner."""
    msgs = []
    ok = True
    total = len(data)
    for i, e in enumerate(entries):
        if i + 1 < len(entries):
            nxt = entries[i + 1]
            gap = nxt['off'] - (e['off'] + e['size'])
            if gap != GAP:
                ok = False
                msgs.append('  slot %d -> %d: fark %d, beklenen %d (ofset %d boyut %d)' %
                            (e['no'], nxt['no'], gap, GAP, e['off'], e['size']))
        else:
            gap = total - (e['off'] + e['size'])
            if gap != GAP:
                ok = False
                msgs.append('  slot %d -> dosya sonu: fark %d, beklenen %d' %
                            (e['no'], gap, GAP))
        # VAG basligindaki veri boyutu indexteki boyuta esit mi?
        vh = data[e['off']:e['off'] + VAG_HDR]
        if len(vh) == VAG_HDR and vh[:4] == b'VAGp':
            dsize = rd32be(vh, 12)
            vrate = rd32be(vh, 16)
            if dsize != e['size']:
                # Retail JBC'de normaldir: VAG basligi ham ADPCM boyutunu,
                # index ise ciftlere tamamlanmis (+48) boyutu tutar.
                # Yeniden kurma index boyutunu kullandigi icin sorun degil.
                msgs.append('  (not) slot %d: VAG basligi veri boyutu %d != index boyutu %d (retail icin normal)' %
                            (e['no'], dsize, e['size']))
            msgs.append('  slot %d: VAG hiz %d Hz, veri %d bayt (index %d)' %
                        (e['no'], vrate, dsize, e['size']))
        else:
            msgs.append('  slot %d: VAGp basligi yok (ilk 4 bayt: %r)' % (e['no'], vh[:4]))
    return ok, msgs

def read_mapping(path, max_slot=23):
    mapping = {}
    with open(path, 'r', encoding='utf-8') as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) < 2:
                raise SystemExit('eslesme satir %d: slot_no + vag dosyasi gerekli: %r' % (ln, line))
            try:
                slot = int(parts[0])
            except ValueError:
                raise SystemExit('eslesme satir %d: slot numarasi degil: %r' % (ln, parts[0]))
            if not 1 <= slot <= max_slot:
                raise SystemExit('eslesme satir %d: slot 1-%d arasi olmali: %d' % (ln, max_slot, slot))
            if slot in mapping:
                raise SystemExit('eslesme satir %d: slot %d iki kez yazilmis' % (ln, slot))
            vag = parts[1]
            name = None
            rest = line.split(None, 2)
            if len(rest) == 3:
                name = rest[2].strip().strip('"')
            mapping[slot] = (vag, name)
    return mapping

def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == '--kontrol':
        data, header, rate, index_size, index, entries, ntunes = parse_jbc(args[1])
        print('Dosya: %s (%d bayt)' % (args[1], len(data)))
        print('Ornek hizi: %d Hz, kategori: %d, index: %d bayt' %
              (rate, header[3], index_size))
        print('Sarki sayisi: %d' % ntunes)
        print('Index aciklanmamis kuyruk: %d bayt' % (index_size - CAT - ntunes * ENTRY))
        ok, msgs = check_structure(data, entries)
        for m in msgs:
            print(m)
        print('YAPI: %s' % ('TAMAM (ofset farklari + VAG basliklari tutarli)' if ok else 'UYMUYOR - yeniden kurma!'))
        return 0 if ok else 2

    if len(args) < 2:
        print(__doc__.split('\n')[1])
        print('Kullanim: python3 jukebox_yeniden_kur.py JukeboxM.JBC eslesme.txt [cikis.JBC]')
        print('       veya: python3 jukebox_yeniden_kur.py --kontrol JukeboxM.JBC')
        return 1
    src, map_path = args[0], args[1]
    dst = args[2] if len(args) > 2 else 'JukeboxM_yeni.JBC'
    if os.path.abspath(src) == os.path.abspath(dst):
        raise SystemExit('Cikis dosyasi giris dosyasinin uzerine yazilamaz.')

    data, header, rate, index_size, index, entries, ntunes = parse_jbc(src)
    ok, msgs = check_structure(data, entries)
    for m in msgs:
        print(m)
    if not ok:
        raise SystemExit('Yapi dogrulamasi BASARISIZ - cikis yazilmadi. Once ustteki satirlara bak.')

    mapping = read_mapping(map_path, ntunes)
    if not mapping:
        raise SystemExit('Eslesme dosyasi bos: %s' % map_path)
    print('Degisecek slotlar: %s' % sorted(mapping))

    # Yeni VAG'lari oku + dogrula
    new_vag = {}
    for slot, (vag_path, new_name) in mapping.items():
        with open(vag_path, 'rb') as f:
            vb = f.read()
        if len(vb) < VAG_HDR or vb[:4] != b'VAGp':
            raise SystemExit('slot %d: %s gecerli VAG degil (VAGp basligi yok)' % (slot, vag_path))
        dsize = rd32be(vb, 12)
        vrate = rd32be(vb, 16)
        if dsize != len(vb) - VAG_HDR:
            raise SystemExit('slot %d: VAG basligi veri boyutu %d != dosya boyutu %d' %
                             (slot, dsize, len(vb) - VAG_HDR))
        if vrate != rate:
            print('UYARI slot %d: VAG hizi %d Hz != JBC hizi %d Hz (oyunda hizli/yavas calabilir)' %
                  (slot, vrate, rate))
        if new_name is not None and len(new_name.encode('ascii', 'replace')) > 83:
            raise SystemExit('slot %d: yeni ad 83 karakteri gecemez: %r' % (slot, new_name))
        new_vag[slot] = (vb, new_name)
        print('  slot %d: %s (%d bayt VAG, hiz %d Hz)%s' %
              (slot, vag_path, len(vb), vrate,
               ' -> ad: %s' % new_name if new_name else ''))

    # Yeniden kur: baslik + index (ofset/boyut/ad guncel) + slotlar
    out_index = bytearray(index)
    out_slots = []
    pos = HDR + index_size  # ilk slot ofseti (orijinalle ayni)
    for e in entries:
        slot = e['no']
        at = CAT + (slot - 1) * ENTRY
        if slot in new_vag:
            vb, new_name = new_vag[slot]
            new_size = len(vb) - VAG_HDR
            tail = data[e['off'] + VAG_HDR + e['size']: e['off'] + VAG_HDR + e['size'] + TAIL]
            if len(tail) != TAIL:
                raise SystemExit('slot %d: orijinal kuyruk okunamadi (%d bayt)' % (slot, len(tail)))
            struct.pack_into('<I', out_index, at + 0, pos)
            struct.pack_into('<I', out_index, at + 4, new_size)
            if new_name is not None:
                nb = new_name.encode('ascii', 'replace')[:83]
                out_index[at + 12:at + 96] = nb + b'\x00' * (84 - len(nb))
            out_slots.append(vb + tail)
            print('  slot %d: yeni ofset %d, yeni boyut %d (eski %d)' % (slot, pos, new_size, e['size']))
            pos += len(vb) + TAIL
        else:
            end = e['off'] + VAG_HDR + e['size'] + TAIL
            if slot < ntunes:
                end = entries[slot]['off']  # bir sonraki slotun basi (sonraki ofset = bu slotun sonu)
            struct.pack_into('<I', out_index, at + 0, pos)
            # boyut degismedi
            out_slots.append(data[e['off']:end])
            pos += (end - e['off'])

    with open(dst, 'wb') as f:
        f.write(data[:HDR])
        f.write(bytes(out_index))
        for s in out_slots:
            f.write(s)
    print('Yazildi: %s (%d bayt, orijinal %d bayt)' % (dst, os.path.getsize(dst), len(data)))
    print('Index boyutu degismedi: %d bayt (%d bayt aciklanmamis kuyruk aynen korundu)' % (index_size, index_size - CAT - ntunes * ENTRY))
    return 0

if __name__ == '__main__':
    sys.exit(main())
