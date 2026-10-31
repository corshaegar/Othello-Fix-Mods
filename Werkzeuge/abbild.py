"""abbild.py <OTHELLO_ABI.DLL> <ziel.img> [basis-hex] - Speicherabbild einer DLL fuer den Test schreiben.

Die DLL wird wie vom Lader auf die Basis verschoben (Relocations angewendet) und als rohes Abbild abgelegt:
4 Byte Basis (LE) + Abbild. Test\\unlock_test.cpp legt es an genau diese Basis und laesst den echten Patch-Code darauf
laufen. Vorgabe-Basis 0x70670000 - dort lag OTHELLO_ABI.DLL im Speicherabbild des Spiels.

Braucht pefile (pip install pefile).
"""
import struct
import sys

import pefile

if len(sys.argv) < 3:
    sys.exit(__doc__)
quelle, ziel = sys.argv[1], sys.argv[2]
basis = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x70670000

pe = pefile.PE(quelle)
pe.relocate_image(basis)
abbild = pe.get_memory_mapped_image()
with open(ziel, 'wb') as f:
    f.write(struct.pack('<I', basis))
    f.write(abbild)
print('%s -> %s: Basis %08X, %d Byte' % (quelle, ziel, basis, len(abbild)))
