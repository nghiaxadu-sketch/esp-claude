"""Phase 1: xuat 3 phuong an nhan vat + mock man hinh 128x64 thuc te."""
import os

from PIL import Image, ImageDraw

from dancer import draw_dancer, STYLES
from neon_lib import new_canvas, draw_eq_bars, upscale, check_png, INK

ROOT = os.path.join(os.path.dirname(__file__), "..")
PREVIEW = os.path.join(ROOT, "preview", "phase1")
CELL_W, CELL_H = 42, 64
POSE_ORDER = ("idle", "bounce", "step", "arms_up")
SCALE = 4
LABEL_H = 14
LABELS = {
    "A_street": "A - Street (silhouette dac, hoodie, sneaker)",
    "B_chibi": "B - Chibi (dau to, vien rong, toc dung)",
    "C_cyber": "C - Cyber (manh, kinh visor, ao dither)",
}


def mock_screen(style):
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_dancer(img, "arms_up", style, ox=4, oy=0)
    draw_eq_bars(d, 60, 62, [8, 14, 20, 12, 26, 18, 10, 22, 16], bar_w=3, gap=2)
    for x in range(0, 128, 3):  # san nhay
        d.point((x, 63), fill=INK)
    return img


def pose_row(style):
    row = new_canvas(CELL_W * len(POSE_ORDER), CELL_H)
    for i, pose in enumerate(POSE_ORDER):
        draw_dancer(row, pose, style, ox=i * CELL_W)
    return row


def build_sheet(rows):
    """rows: list (label, poses_img, mock_img) -> sheet RGB da phong to."""
    gap = 8
    w = (CELL_W * len(POSE_ORDER) + 128) * SCALE + gap * 3
    h = len(rows) * (CELL_H * SCALE + LABEL_H + gap) + gap
    sheet = Image.new("RGB", (w, h), (30, 30, 30))
    d = ImageDraw.Draw(sheet)
    y = gap
    for label, poses, mock in rows:
        d.text((gap, y), label, fill=(255, 140, 0))
        y += LABEL_H
        sheet.paste(upscale(poses, SCALE).convert("RGB"), (gap, y))
        sheet.paste(upscale(mock, SCALE).convert("RGB"), (gap * 2 + poses.width * SCALE, y))
        y += CELL_H * SCALE + gap
    return sheet


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    rows, errors = [], []
    for style in STYLES:
        poses, mock = pose_row(style), mock_screen(style)
        mock_path = os.path.join(PREVIEW, f"{style}_screen_128x64.png")
        mock.save(mock_path)
        errors += [f"{mock_path}: {e}" for e in check_png(mock_path, (128, 64))]
        rows.append((LABELS[style], poses, mock))
    build_sheet(rows).save(os.path.join(PREVIEW, "phase1_style_sheet_x4.png"))
    print("Self-check:", "OK - tat ca 128x64, mode '1', chi 0/255" if not errors else errors)


if __name__ == "__main__":
    main()
