"""Generates this project's icons procedurally (no external art assets,
same spirit as the game's own procedural car bodies/terrain) -- a car-
silhouette .ico for CarGame3D.exe, a map-pin .ico for MapEditor.exe, and a
wordmark PNG logo for the README/release page.

Run: python make_icons.py
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).parent
ASSETS_ICON_DIR = Path(__file__).resolve().parents[2] / "assets" / "icons"
ASSETS_ICON_DIR.mkdir(parents=True, exist_ok=True)

RED = (216, 30, 30, 255)
RED_DARK = (122, 13, 13, 255)
BG = (18, 20, 26, 255)
WHITE = (240, 240, 245, 255)
GLASS = (90, 150, 235, 255)


def rounded_bg(size, color=BG, radius_frac=0.22):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=int(size * radius_frac), fill=color)
    return img, d


def car_icon(size):
    img, d = rounded_bg(size)
    s = size
    # Low-poly car silhouette, side profile: body + cabin + two wheels.
    body = [(s*0.12, s*0.62), (s*0.20, s*0.52), (s*0.34, s*0.52), (s*0.40, s*0.40),
            (s*0.62, s*0.40), (s*0.70, s*0.52), (s*0.86, s*0.52), (s*0.90, s*0.62),
            (s*0.90, s*0.70), (s*0.12, s*0.70)]
    d.polygon(body, fill=RED)
    d.polygon(body, outline=RED_DARK, width=max(1, s // 64))
    # Cabin glass.
    d.polygon([(s*0.42, s*0.41), (s*0.60, s*0.41), (s*0.655, s*0.515), (s*0.365, s*0.515)], fill=GLASS)
    # Wheels.
    wr = s * 0.09
    for wx in (s*0.28, s*0.72):
        d.ellipse([wx-wr, s*0.70-wr, wx+wr, s*0.70+wr], fill=(20, 20, 24, 255))
        d.ellipse([wx-wr*0.45, s*0.70-wr*0.45, wx+wr*0.45, s*0.70+wr*0.45], fill=(150, 155, 165, 255))
    return img


def map_icon(size):
    img, d = rounded_bg(size)
    s = size
    # Folded-map / pin hybrid: a map-pin silhouette over a faint grid, to
    # read distinctly from the car icon while still being "this project".
    d.ellipse([s*0.18, s*0.14, s*0.82, s*0.78], fill=RED)
    d.polygon([(s*0.5, s*0.92), (s*0.30, s*0.60), (s*0.70, s*0.60)], fill=RED)
    d.ellipse([s*0.37, s*0.31, s*0.63, s*0.57], fill=BG)
    # Tiny road line through the pin's head, echoing the game's own road.
    d.line([(s*0.40, s*0.46), (s*0.60, s*0.40)], fill=WHITE, width=max(1, s // 40))
    return img


def save_ico(img_fn, path, sizes=(16, 24, 32, 48, 64, 128, 256)):
    imgs = [img_fn(sz) for sz in sizes]
    imgs[-1].save(path, format="ICO", sizes=[(sz, sz) for sz in sizes], append_images=imgs[:-1])


def wordmark():
    w, h = 900, 260
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w-1, h-1], radius=28, fill=BG)
    badge = car_icon(180)
    img.paste(badge, (36, 40), badge)
    try:
        font_big = ImageFont.truetype("arialbd.ttf", 64)
        font_small = ImageFont.truetype("arial.ttf", 26)
    except Exception:
        font_big = ImageFont.load_default()
        font_small = font_big
    d.text((250, 62), "3D CAR GAME", font=font_big, fill=WHITE)
    d.text((252, 140), "low-poly open-world driving  -  raylib + C++", font=font_small, fill=(170, 175, 185, 255))
    img.save(OUT / "logo.png")
    img.save(ASSETS_ICON_DIR / "logo.png")


car = car_icon(512)
car.save(OUT / "car_icon_preview.png")
mapi = map_icon(512)
mapi.save(OUT / "map_icon_preview.png")

save_ico(car_icon, ASSETS_ICON_DIR / "game.ico")
save_ico(map_icon, ASSETS_ICON_DIR / "mapeditor.ico")
wordmark()

print("done:", list(ASSETS_ICON_DIR.iterdir()))
