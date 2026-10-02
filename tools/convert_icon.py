"""Converte assets/plane.png in una bitmap 1-bit impaccata (assets/plane_icon.h).

Uso: python tools/convert_icon.py
Rigenera l'header ogni volta che si sostituisce plane.png.
"""
from PIL import Image
import os

SRC = os.path.join(os.path.dirname(__file__), "..", "plane.png")
DST = os.path.join(os.path.dirname(__file__), "..", "assets", "plane_icon.h")
SIZE = 96  # px, quadrato

def main():
    img = Image.open(SRC).convert("RGBA")
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, img).convert("L")
    img = img.resize((SIZE, SIZE), Image.LANCZOS)

    bytes_per_row = SIZE // 8
    data = bytearray(bytes_per_row * SIZE)
    for y in range(SIZE):
        for x in range(SIZE):
            lum = img.getpixel((x, y))
            is_plane_pixel = lum < 128  # silhouette nera = "acceso"
            if is_plane_pixel:
                byte_index = y * bytes_per_row + (x // 8)
                bit_index = 7 - (x % 8)
                data[byte_index] |= (1 << bit_index)

    with open(DST, "w") as f:
        f.write("// Generato da tools/convert_icon.py — non modificare a mano.\n")
        f.write("// Bitmap 1-bit, MSB-first, packed per riga. 1 = pixel della sagoma aereo.\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n\n")
        f.write(f"#define PLANE_ICON_SIZE {SIZE}\n\n")
        f.write(f"static const uint8_t PLANE_ICON_BITS[{len(data)}] = {{\n")
        for i in range(0, len(data), 12):
            row = data[i:i+12]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in row) + ",\n")
        f.write("};\n")

    print(f"Scritto {DST} ({len(data)} byte, {SIZE}x{SIZE})")

if __name__ == "__main__":
    main()
