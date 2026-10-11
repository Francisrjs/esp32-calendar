"""Convert reference renders to packed 1-bit buffers, without changing firmware."""
from pathlib import Path
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parent
out = root / 'handoff' / 'mono'
out.mkdir(parents=True, exist_ok=True)
for file in (root / 'handoff' / 'screens').glob('*.png'):
    original = Image.open(file).convert('L')
    assert original.size == (400, 300), (file.name, original.size)
    # Text retains sharp black. Error diffusion keeps mascot tonal detail.
    mono = original.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
    mono.save(out / file.name)
    # PIL uses 1=white; flip for the documented framebuffer convention 1=black.
    packed = bytes(v ^ 255 for v in mono.tobytes())
    assert len(packed) == 15000
    (out / (file.stem + '.bin')).write_bytes(packed)
mascot = Image.open(root / 'handoff' / 'mascot.png').convert('L').convert('1', dither=Image.Dither.FLOYDSTEINBERG)
mascot.save(out / 'mascot.png')
# Mascot rows are padded to 27 bytes (210 pixels). Expose stride in handoff guide.
(root / 'handoff' / 'mono' / 'mascot.bin').write_bytes(bytes(v ^ 255 for v in mascot.tobytes()))
order = ['home','alarms','tasks','calendar','music','rest','listening','speaking']
sheet = Image.new('RGB', (1680, 720), '#f5f4ee')
draw = ImageDraw.Draw(sheet)
for i, name in enumerate(order):
    x, y = 20 + (i % 4) * 420, 20 + (i // 4) * 350
    sheet.paste(Image.open(root / 'handoff' / 'screens' / (name + '.png')).convert('RGB'), (x, y))
    draw.text((x, y + 313), name.upper(), fill='#24352e')
sheet.save(root / 'handoff' / 'contact-sheet.png')
print('Packed references: 12 x 15000 bytes; mascot stride 27 bytes x 202 rows.')
