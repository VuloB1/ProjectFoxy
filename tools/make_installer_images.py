"""Draws the pictures of the installer (installer/art/*.bmp) from resources/icons/icon.png.

    python tools/make_installer_images.py

  welcome-<n>.bmp   the tall picture of the welcome page, at the sizes Inno Setup uses for 100..200 % scaling
  finish.bmp        the picture of the "installation complete" page (the biggest size; Inno scales it down)
  small-<n>.bmp     the little logo in the top corner of the other pages
"""
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "installer", "art")
LOGO = Image.open(os.path.join(ROOT, "resources", "icons", "icon.png")).convert("RGBA")
FONT = "C:/Windows/Fonts/seguisb.ttf"

BASE_W, BASE_H = 328, 628  # the biggest size (200 %); the others are this one scaled down


def background(w, h, glow_y, glow=(255, 120, 40)):
    """Dark, a little warm towards the bottom, with an orange glow behind where the logo goes."""
    top, bottom = (24, 24, 30), (46, 28, 20)
    img = Image.new("RGB", (w, h))
    px = ImageDraw.Draw(img)
    for y in range(h):
        t = y / (h - 1)
        px.line([(0, y), (w, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    halo = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    r = int(w * 0.62)
    ImageDraw.Draw(halo).ellipse([w // 2 - r, glow_y - r, w // 2 + r, glow_y + r], fill=glow + (120,))
    halo = halo.filter(ImageFilter.GaussianBlur(w * 0.18))
    return Image.alpha_composite(img.convert("RGBA"), halo)


def put_logo(img, size, cy):
    logo = LOGO.resize((size, size), Image.LANCZOS)
    img.alpha_composite(logo, ((img.width - size) // 2, cy - size // 2))


def centered_text(img, text, cy, size, fill):
    font = ImageFont.truetype(FONT, size)
    d = ImageDraw.Draw(img)
    w = d.textlength(text, font=font)
    d.text(((img.width - w) / 2, cy), text, font=font, fill=fill)


def welcome():
    img = background(BASE_W, BASE_H, int(BASE_H * 0.36))
    put_logo(img, int(BASE_W * 0.82), int(BASE_H * 0.36))
    centered_text(img, "Project Foxy", int(BASE_H * 0.62), 40, (255, 255, 255, 255))
    d = ImageDraw.Draw(img)
    y = int(BASE_H * 0.62) + 62
    d.line([(BASE_W * 0.38, y), (BASE_W * 0.62, y)], fill=(255, 122, 40, 255), width=3)
    return img.convert("RGB")


def finish():
    img = background(BASE_W, BASE_H, int(BASE_H * 0.36), glow=(255, 150, 60))
    put_logo(img, int(BASE_W * 0.82), int(BASE_H * 0.36))
    # a check badge on the lower right of the logo
    r = int(BASE_W * 0.17)
    cx, cy = int(BASE_W * 0.76), int(BASE_H * 0.36 + BASE_W * 0.30)
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).ellipse([cx - r, cy - r + 6, cx + r, cy + r + 6], fill=(0, 0, 0, 150))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(8)))
    # drawn 4 times bigger and scaled down, for smooth edges
    k = 4
    badge = Image.new("RGBA", (2 * r * k, 2 * r * k), (0, 0, 0, 0))
    bd = ImageDraw.Draw(badge)
    bd.ellipse([0, 0, 2 * r * k - 1, 2 * r * k - 1], fill=(255, 255, 255, 255))
    m = int(r * k * 0.14)
    bd.ellipse([m, m, 2 * r * k - 1 - m, 2 * r * k - 1 - m], fill=(46, 184, 114, 255))
    s = r * k
    bd.line([(s * 0.50, s * 1.02), (s * 0.88, s * 1.38), (s * 1.50, s * 0.66)], fill=(255, 255, 255, 255),
            width=int(r * k * 0.22), joint="curve")
    for x, y in ((s * 0.50, s * 1.02), (s * 1.50, s * 0.66)):  # round ends
        w = int(r * k * 0.11)
        bd.ellipse([x - w, y - w, x + w, y + w], fill=(255, 255, 255, 255))
    badge = badge.resize((2 * r, 2 * r), Image.LANCZOS)
    img.alpha_composite(badge, (cx - r, cy - r))
    centered_text(img, "Project Foxy", int(BASE_H * 0.62), 40, (255, 255, 255, 255))
    y = int(BASE_H * 0.62) + 62
    ImageDraw.Draw(img).line([(BASE_W * 0.38, y), (BASE_W * 0.62, y)], fill=(46, 184, 114, 255), width=3)
    return img.convert("RGB")


def small(size):
    img = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    pad = max(2, size // 14)
    img.alpha_composite(LOGO.resize((size - 2 * pad, size - 2 * pad), Image.LANCZOS), (pad, pad))
    return img.convert("RGB")


def main():
    os.makedirs(OUT, exist_ok=True)
    big = welcome()
    for pct, (w, h) in {100: (164, 314), 125: (192, 386), 150: (246, 459), 175: (273, 514), 200: (328, 628)}.items():
        big.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, f"welcome-{pct}.bmp"))
    finish().save(os.path.join(OUT, "finish.bmp"))
    for pct, n in {100: 55, 125: 69, 150: 83, 175: 96, 200: 110}.items():
        small(n).save(os.path.join(OUT, f"small-{pct}.bmp"))
    print("ok:", sorted(os.listdir(OUT)))


main()
