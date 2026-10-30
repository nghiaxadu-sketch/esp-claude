"""NeonDancer - thu vien ve pixel 1-bit dung chung cho moi phase.

Quy uoc mau (theo asset Flipper): 0 = pixel den (sang tren man hinh), 255 = nen.
"""
from PIL import Image, ImageDraw

INK = 0
BG = 255
SCREEN_W, SCREEN_H = 128, 64


def new_canvas(w=SCREEN_W, h=SCREEN_H):
    return Image.new("1", (w, h), BG)


def dither_pattern(name, x, y):
    """True neu pixel (x, y) duoc to theo mau dither."""
    if name == "checker":
        return (x + y) % 2 == 0
    if name == "sparse":
        return x % 2 == 0 and y % 2 == 0
    if name == "dense":
        return not (x % 2 == 1 and y % 2 == 1)
    if name == "hlines":
        return y % 2 == 0
    if name == "diag":
        return (x + y) % 4 == 0
    return True


def dither_fill(img, mask, pattern, origin=(0, 0)):
    """To vung mask (anh '1', pixel 0 = thuoc vung) bang pattern dither."""
    ox, oy = origin
    mpx = mask.load()
    ipx = img.load()
    for y in range(mask.height):
        for x in range(mask.width):
            if mpx[x, y] == INK:
                gx, gy = x + ox, y + oy
                if 0 <= gx < img.width and 0 <= gy < img.height:
                    ipx[gx, gy] = INK if dither_pattern(pattern, gx, gy) else BG


def shape_mask(size, draw_fn):
    m = Image.new("1", size, BG)
    draw_fn(ImageDraw.Draw(m))
    return m


def draw_limb(d, pts, width, outline=False):
    """Ve chi (tay/chan) qua cac khop. outline=True: vien den, ruot trang."""
    d.line(pts, fill=INK, width=width, joint="curve")
    for p in pts:
        r = width // 2
        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=INK)
    if outline and width >= 4:
        d.line(pts, fill=BG, width=width - 2, joint="curve")


def draw_eq_bars(d, x, y_bottom, heights, bar_w=3, gap=1, peak=True):
    """Thanh equalizer: moi cot gom cac o 2px, co cham dinh (peak)."""
    for i, h in enumerate(heights):
        bx = x + i * (bar_w + gap)
        for yy in range(y_bottom - h, y_bottom, 2):
            d.rectangle([bx, yy, bx + bar_w - 1, yy], fill=INK)
        if peak and h > 0:
            d.rectangle([bx, y_bottom - h - 3, bx + bar_w - 1, y_bottom - h - 3], fill=INK)


def scanlines(img, every=4, phase=0):
    """Hieu ung scanline: xoa 1 hang moi `every` hang (chi tren vung den)."""
    px = img.load()
    for y in range(phase, img.height, every):
        for x in range(img.width):
            if px[x, y] == INK and x % 2 == 0:
                px[x, y] = BG


def glitch_shift(img, y0, y1, dx):
    """Dich ngang mot dai hang de tao glitch."""
    band = img.crop((0, y0, img.width, y1))
    img.paste(BG, (0, y0, img.width, y1))
    img.paste(band, (dx, y0))


def upscale(img, k=4):
    return img.resize((img.width * k, img.height * k), Image.NEAREST)


def to_pure_1bit(img):
    return img.convert("L").point(lambda v: 255 if v >= 128 else 0).convert("1")


def check_png(path, size=None):
    """Tra ve list loi: sai kich thuoc, khong phai mode '1', pixel ngoai {0,255}."""
    errs = []
    im = Image.open(path)
    if size and im.size != size:
        errs.append(f"size {im.size} != {size}")
    if im.mode != "1":
        errs.append(f"mode {im.mode} != '1'")
    vals = set(im.convert("L").getdata())
    if not vals <= {0, 255}:
        errs.append(f"pixel values {sorted(vals)[:6]}")
    return errs
