"""Writes a 128-byte EDID 1.3 for 1920x1080 at 60 Hz (CEA timing, 148.5 MHz),
for a DVO output whose bridge passes no EDID (Loongson LS7A1000 + MS7210)."""
import sys

e = bytearray(128)
e[0:8] = bytes([0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00])
e[8:10] = bytes([0x31, 0xD8])          # manufacturer "LNX"
e[16], e[17] = 5, 2012 - 1990          # week, year
e[18], e[19] = 1, 3                    # EDID 1.3
e[20] = 0x80                           # digital input
e[21], e[22] = 53, 30                  # image size, cm
e[23] = 120                            # gamma 2.2
e[24] = 0x02                           # preferred timing is the first descriptor
e[25:35] = bytes([0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54])
e[38:54] = bytes([0x01] * 16)          # no standard timings

def dtd(clock_khz, ha, hb, va, vb, hso, hsw, vso, vsw, hmm, vmm, flags):
    c = clock_khz // 10
    return bytes([
        c & 0xFF, c >> 8,
        ha & 0xFF, hb & 0xFF, ((ha >> 8) << 4) | (hb >> 8),
        va & 0xFF, vb & 0xFF, ((va >> 8) << 4) | (vb >> 8),
        hso & 0xFF, hsw & 0xFF, ((vso & 0xF) << 4) | (vsw & 0xF),
        ((hso >> 8) << 6) | ((hsw >> 8) << 4) | ((vso >> 4) << 2) | (vsw >> 4),
        hmm & 0xFF, vmm & 0xFF, ((hmm >> 8) << 4) | (vmm >> 8),
        0, 0, flags,
    ])

# 1920x1080@60: 148.5 MHz, H 1920+88+44+148, V 1080+4+5+36, +hsync +vsync
e[54:72] = dtd(148500, 1920, 280, 1080, 45, 88, 44, 4, 5, 527, 296, 0x1E)
# Range limits: 50-75 Hz, 30-80 kHz, 160 MHz
e[72:90] = bytes([0, 0, 0, 0xFD, 0, 50, 75, 30, 80, 16, 0, 0x0A] + [0x20] * 6)
name = b"LS7A DVO 1080\n"[:13].ljust(13, b" ")
e[90:108] = bytes([0, 0, 0, 0xFC, 0]) + name
e[108:126] = bytes([0, 0, 0, 0x10, 0] + [0] * 13)
e[126] = 0                             # no extension blocks
e[127] = (256 - sum(e[:127]) % 256) % 256
assert sum(e) % 256 == 0 and len(e) == 128

open(sys.argv[1], "wb").write(e)
print("wrote", sys.argv[1])
