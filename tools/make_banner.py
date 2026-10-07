"""Draws docs/assets/banner.png (the picture at the top of the README) from the logo.

    python tools/make_banner.py <logo.png> [out.png]

The logo is the full Project Foxy logo (emblem + wordmark, transparent background). The banner is the emblem on a
light tile, the name and the tagline on a dark background with a warm glow, like the other banners of this author.
Needs Pillow and numpy; the fonts are Windows' Segoe UI.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1280, 640
ORANGE = (255, 106, 19)
FONTS = Path("C:/Windows/Fonts")


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def background():
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    t = np.clip((x / W) * 0.55 + (y / H) * 0.45, 0, 1)            # diagonal blend dark -> warm dark
    top = np.array([20, 21, 26], np.float32)
    bottom = np.array([66, 30, 12], np.float32)
    img = top[None, None, :] * (1 - t[..., None]) + bottom[None, None, :] * t[..., None]
    # a soft orange glow behind the emblem
    d = np.sqrt((x - 265) ** 2 + (y - 320) ** 2)
    glow = np.clip(1 - d / 430, 0, 1) ** 2
    img += glow[..., None] * np.array([120, 52, 14], np.float32)[None, None, :] * 0.85
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


def main():
    logo = Image.open(sys.argv[1]).convert("RGBA")
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parent.parent / "docs" / "assets" / "banner.png"
    out.parent.mkdir(parents=True, exist_ok=True)

    # the emblem alone (without the wordmark under it)
    bbox = logo.crop((0, 0, logo.width, int(logo.height * 0.755))).getbbox()
    emblem = logo.crop(bbox)

    canvas = background()

    # the tile
    tile_size, tile_x, tile_y, radius = 330, 100, (H - 330) // 2, 74
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((tile_x, tile_y + 14, tile_x + tile_size, tile_y + tile_size + 14), radius, fill=(0, 0, 0, 150))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(22)))
    tile = Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0))
    grad = np.linspace(0, 1, tile_size, dtype=np.float32)[:, None, None]
    top, bottom = np.array([255, 251, 246], np.float32), np.array([255, 236, 220], np.float32)
    fill = (top * (1 - grad) + bottom * grad) * np.ones((tile_size, tile_size, 1), np.float32)
    tile_rgb = Image.fromarray(fill.astype(np.uint8), "RGB").convert("RGBA")
    mask = Image.new("L", (tile_size, tile_size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, tile_size - 1, tile_size - 1), radius, fill=255)
    tile.paste(tile_rgb, (0, 0), mask)
    inner = int(tile_size * 0.80)
    scale = inner / max(emblem.size)
    e = emblem.resize((int(emblem.width * scale), int(emblem.height * scale)), Image.LANCZOS)
    tile.alpha_composite(e, ((tile_size - e.width) // 2, (tile_size - e.height) // 2))
    canvas.alpha_composite(tile, (tile_x, tile_y))

    d = ImageDraw.Draw(canvas)
    x = 490
    d.text((x, 160), "Project", font=font("segoeuib.ttf", 128), fill=(255, 255, 255, 255))
    d.text((x, 280), "Foxy", font=font("segoeuib.ttf", 128), fill=ORANGE + (255,))
    d.text((x + 6, 438), "Visor y editor de imágenes para Windows.", font=font("segoeui.ttf", 40), fill=(255, 255, 255, 255))
    d.text((x + 6, 494), "Rápido, ligero, con editor, GIF, collage y comparador.", font=font("segoeui.ttf", 28), fill=(255, 190, 150, 255))
    canvas.convert("RGB").save(out, optimize=True)
    print("ok", out, canvas.size)


if __name__ == "__main__":
    main()
