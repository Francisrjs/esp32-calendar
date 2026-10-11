"""Render the supplied PDF as 400x300 monochrome preview and firmware assets.

Requires Pillow and Poppler's pdftoppm on PATH. Run from any directory.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageEnhance, ImageOps
from visual_design import refine_view, rest_view

ROOT = Path(__file__).resolve().parent
PDF = ROOT / "ESP32 - Design.pdf"
PREVIEW = ROOT / "preview" / "assets"
FIRMWARE = ROOT / "firmware" / "src"
NAMES = ("alarms", "tasks", "calendar", "home", "music", "listening", "speaking")


def build() -> None:
    renderer = shutil.which("pdftoppm")
    if not renderer:
        raise SystemExit("pdftoppm (Poppler) is required on PATH")
    PREVIEW.mkdir(parents=True, exist_ok=True)
    FIRMWARE.mkdir(parents=True, exist_ok=True)
    sources = []
    with tempfile.TemporaryDirectory() as temporary:
        prefix = Path(temporary) / "design"
        subprocess.run([renderer, "-r", "288", "-png", str(PDF), str(prefix)], check=True)
        pages = sorted(prefix.parent.glob("design-*.png"))
        if len(pages) != len(NAMES):
            raise SystemExit(f"Expected {len(NAMES)} PDF pages; found {len(pages)}")
        status = ImageOps.grayscale(Image.open(pages[5]).resize((400, 300), Image.Resampling.LANCZOS).crop((290, 0, 400, 23))).point(lambda value: 255 if value > 174 else 0, "1").convert("RGB")
        for index, (name, page) in enumerate(zip(NAMES, pages)):
            image = Image.open(page).convert("RGB")
            if index < 3:
                # These PDF exports include an outer margin; crop the 400x300 frame.
                image = image.crop((8, 4, 1608, 1204))
            image = image.resize((400, 300), Image.Resampling.LANCZOS)
            original_gray = ImageOps.grayscale(image)
            gray = ImageEnhance.Contrast(original_gray).enhance(1.55)
            mono = gray.point(lambda value: 255 if value >= 174 else 0, "1").convert("RGB")
            if name in ("home", "music"):
                # Keep detail in the central portrait while text stays crisp.
                top = 63 if name == "home" else 39
                size = 142 if name == "home" else 128
                left = (400 - size) // 2
                box = (left, top, left + size, top + size)
                portrait = ImageOps.autocontrast(original_gray.crop(box), cutoff=0.5).convert("RGB")
                if name == "music":
                    ImageDraw.Draw(mono).rectangle((129, 35, 271, 172), fill="white")
                mask = Image.new("L", (size, size), 0)
                ImageDraw.Draw(mask).ellipse((4, 4, size - 5, size - 5), fill=255)
                mono.paste(portrait, box, mask)
            outline = ImageDraw.Draw(mono)
            # All views share the assistant's compact environmental header.
            outline.rectangle((0, 0, 399, 57 if index < 3 else 29), fill="white")
            mono.paste(status, (290, 0))
            outline.line([(12, 15), (23, 5), (34, 15)], fill="black", width=2)
            outline.line([(15, 13), (15, 27), (31, 27), (31, 13)], fill="black", width=2)
            outline.rectangle((21, 19, 25, 27), outline="black", width=2)
            if name == "home":
                outline.rounded_rectangle((10, 33, 122, 113), radius=9, outline="#202020", width=1)
                outline.rounded_rectangle((278, 33, 391, 113), radius=9, outline="#202020", width=1)
                outline.rectangle((4, 265, 43, 299), fill="white")
                outline.line([(19, 290), (19, 272), (32, 269), (32, 287)], fill="black", width=3)
                outline.ellipse((12, 285, 20, 293), fill="black")
                outline.ellipse((25, 282, 33, 290), fill="black")
                outline.rectangle((353, 266, 397, 299), fill="white")
                outline.rounded_rectangle((372, 270, 380, 286), radius=4, outline="black", width=2)
                outline.arc((367, 277, 385, 292), 0, 180, fill="black", width=2)
                outline.line([(376, 291), (376, 297)], fill="black", width=2)
            elif name == "alarms":
                outline.rounded_rectangle((12, 60, 387, 177), radius=9, outline="#363636", width=1)
                outline.rounded_rectangle((12, 182, 387, 298), radius=9, outline="#363636", width=1)
            elif name == "tasks":
                for top in (108, 154, 200, 246):
                    outline.rounded_rectangle((12, top, 388, top + 44), radius=6, outline="#363636", width=1)
                    outline.rectangle((25, top + 13, 42, top + 30), outline="black", width=1)
            elif name == "music":
                # Larger, centered transport controls with consistent spacing.
                outline.rectangle((125, 222, 275, 272), fill="white")
                for cx in (151, 200, 249):
                    outline.ellipse((cx - 19, 225, cx + 19, 263), outline="black", width=1)
                outline.polygon([(155, 235), (155, 253), (144, 244)], fill="black")
                outline.line([(141, 235), (141, 253)], fill="black", width=2)
                outline.rectangle((194, 235, 198, 253), fill="black")
                outline.rectangle((202, 235, 206, 253), fill="black")
                outline.polygon([(245, 235), (245, 253), (256, 244)], fill="black")
                outline.line([(259, 235), (259, 253)], fill="black", width=2)
            elif name == "listening":
                outline.ellipse((147, 89, 253, 195), outline="#505050", width=1)
            elif name == "speaking":
                outline.rounded_rectangle((30, 77, 371, 191), radius=12, outline="#505050", width=1)
            mono = refine_view(name, mono)
            mono.save(PREVIEW / f"{name}.png", optimize=True)
            pixels = mono.resize((400,300), Image.Resampling.LANCZOS).convert("1", dither=Image.Dither.FLOYDSTEINBERG).load()
            packed = bytearray()
            for y in range(300):
                for x in range(0, 400, 8):
                    value = 0
                    for bit in range(8):
                        if pixels[x + bit, y] == 0:
                            value |= 1 << (7 - bit)
                    packed.append(value)
            sources.append((name, packed))
    rest = rest_view(Image.open(PREVIEW / 'home.png'))
    rest.save(PREVIEW / 'rest.png', optimize=True)
    pixels = rest.resize((400,300), Image.Resampling.LANCZOS).convert('1').load()
    packed = bytearray()
    for y in range(300):
        for x in range(0,400,8):
            packed.append(sum((1 << (7-bit)) for bit in range(8) if pixels[x+bit,y] == 0))
    sources.append(('rest',packed))
    lines = [
        "// Generated from ESP32 - Design.pdf by generate_assets.py.",
        "#pragma once",
        "#include <Arduino.h>",
        "constexpr int kScreenWidth = 400;",
        "constexpr int kScreenHeight = 300;",
        "constexpr int kScreenBytes = 15000;",
    ]
    for name, data in sources:
        lines.append(f"const uint8_t kScreen_{name}[kScreenBytes] PROGMEM = {{")
        for offset in range(0, len(data), 20):
            lines.append("  " + ", ".join(f"0x{value:02x}" for value in data[offset:offset + 20]) + ",")
        lines.append("};")
    lines.append("const uint8_t* const kScreens[] = {")
    lines.extend(f"  kScreen_{name}," for name, _ in sources)
    lines.append("};")
    (FIRMWARE / "screens.h").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("Generated 8 preview images and 8 packed firmware screens (400x300, 1 bit).")


if __name__ == "__main__":
    build()
