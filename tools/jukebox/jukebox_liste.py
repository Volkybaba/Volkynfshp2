#!/usr/bin/env python3
# JukeboxM.JBC sarki listesi cikarici (NFSHP2)
# Kullanim:  python3 jukebox_liste.py [JukeboxM.JBC]
# Dosya verilmezse bulundugu klasordeki JukeboxM.JBC okunur.
# Cikti hem ekrana hem de jukebox_sarkilar.txt dosyasina yazilir.
# 193MB dosyanin tamami gerekmez: sarki listesi (index) dosyanin
# basindadir, ilk 1MB bile yeterlidir.

import struct
import sys
import os

JB_HEADER_SIZE = 32

def rd32(b, off):
    return struct.unpack_from('<I', b, off)[0]

def cstr(raw):
    # null-terminated ASCII, bosluklari koru, sondaki sifirlari at
    s = raw.split(b'\x00', 1)[0]
    return s.decode('ascii', 'replace')

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'JukeboxM.JBC'
    if not os.path.isfile(path):
        print('Dosya bulunamadi: %s' % path)
        print('Kullanim: python3 jukebox_liste.py [JukeboxM.JBC]')
        return 1

    with open(path, 'rb') as f:
        head = f.read(JB_HEADER_SIZE)
        if len(head) < JB_HEADER_SIZE:
            print('Dosya cok kisa (header okunamadi): %s' % path)
            return 1
        header = struct.unpack('<8I', head)
        cat_count = header[3]
        index_size = header[4]
        print('Kategori sayisi: %d' % cat_count)
        print('Index boyutu: %d bayt' % index_size)
        index = f.read(index_size)
        if len(index) < index_size:
            print('UYARI: index kesik (%d / %d bayt okundu) - dosya ilk 1MB ise liste eksik olabilir' % (len(index), index_size))

    lines = []
    at = 0
    total = 0
    for c in range(cat_count):
        if at + 64 > len(index):
            lines.append('[kategori %d: index bitti, liste kesik]' % (c + 1))
            break
        cat = index[at:at + 64]
        at += 64
        tunes = rd32(cat, 0)
        cname = cstr(cat[4:64])
        lines.append('')
        lines.append('== %s (%d sarki) ==' % (cname, tunes))
        for t in range(tunes):
            if at + 96 > len(index):
                lines.append('  [sarki %d: index bitti, liste kesik]' % (t + 1))
                break
            tu = index[at:at + 96]
            at += 96
            off = rd32(tu, 0)
            size = rd32(tu, 4)
            tname = cstr(tu[12:96])
            total += 1
            lines.append('  %2d. %-60s  (ofset %d, boyut %d)' % (t + 1, tname, off, size))

    lines.append('')
    lines.append('Toplam: %d sarki, %d kategori' % (total, cat_count))
    text = '\n'.join(lines) + '\n'

    print(text)
    out = os.path.join(os.path.dirname(os.path.abspath(path)), 'jukebox_sarkilar.txt')
    with open(out, 'w', encoding='utf-8') as f:
        f.write(text)
    print('Liste yazildi: %s' % out)
    return 0

if __name__ == '__main__':
    sys.exit(main())
