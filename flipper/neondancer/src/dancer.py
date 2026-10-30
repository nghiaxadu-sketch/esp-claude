"""Nhan vat dancer: skeleton + 3 style (A street, B chibi, C cyber)."""
from PIL import ImageDraw

from neon_lib import INK, BG, dither_pattern, shape_mask, draw_limb

GROUND_Y = 62

# Skeleton goc (cell 42x64). Moi pose la dict khop -> (x, y).
POSES = {
    "idle": dict(neck=(21, 22), hip=(21, 40),
                 l_elb=(14, 30), l_hand=(15, 38), r_elb=(28, 30), r_hand=(27, 38),
                 l_knee=(17, 51), l_foot=(15, 61), r_knee=(25, 51), r_foot=(27, 61)),
    "bounce": dict(neck=(21, 24), hip=(21, 42),
                   l_elb=(13, 31), l_hand=(15, 38), r_elb=(29, 31), r_hand=(27, 38),
                   l_knee=(15, 52), l_foot=(15, 61), r_knee=(27, 52), r_foot=(27, 61)),
    "step": dict(neck=(22, 22), hip=(21, 40),
                 l_elb=(13, 27), l_hand=(9, 22), r_elb=(29, 31), r_hand=(26, 37),
                 l_knee=(16, 51), l_foot=(13, 61), r_knee=(29, 47), r_foot=(30, 56)),
    "arms_up": dict(neck=(21, 23), hip=(21, 41),
                    l_elb=(10, 20), l_hand=(7, 11), r_elb=(32, 20), r_hand=(35, 11),
                    l_knee=(16, 51), l_foot=(13, 61), r_knee=(26, 51), r_foot=(29, 61)),
}

STYLES = {
    "A_street": dict(head_r=6, torso_w=11, arm_w=4, leg_w=5, body="solid",
                     vscale=1.0, cap=True),
    "B_chibi": dict(head_r=10, torso_w=10, arm_w=4, leg_w=4, body="outline",
                    vscale=0.62, hair=True),
    "C_cyber": dict(head_r=5, torso_w=9, arm_w=3, leg_w=3, body="dither",
                    vscale=1.05, visor=True),
}


def _scale_pose(pose, vscale, ox, oy):
    out = {}
    for k, (x, y) in pose.items():
        y2 = GROUND_Y - (GROUND_Y - y) * vscale
        out[k] = (round(x + ox), round(y2 + oy))
    return out


def _fill_region(img, draw_fn, mode):
    """To vung kin: solid = dac, outline = vien rong, dither = vien + ruot checker."""
    m = shape_mask(img.size, draw_fn)
    mpx, px = m.load(), img.load()
    for y in range(m.height):
        for x in range(m.width):
            if mpx[x, y] != INK:
                continue
            edge = (x in (0, m.width - 1) or y in (0, m.height - 1) or
                    BG in (mpx[x - 1, y], mpx[x + 1, y], mpx[x, y - 1], mpx[x, y + 1]))
            if mode == "solid" or edge:
                px[x, y] = INK
            elif mode == "outline":
                px[x, y] = BG
            else:
                px[x, y] = INK if dither_pattern("checker", x, y) else BG


def draw_headphones(d, cx, cy, r, style, shake=0):
    """Tai nghe over-ear. shake: lech doc cua cup (-1..1) de tao rung."""
    band_r = r + (5 if style == "B_chibi" else 3)
    box_b = [cx - band_r, cy - band_r, cx + band_r, cy + band_r]
    d.arc([box_b[0] - 1, box_b[1] - 1, box_b[2] + 1, box_b[3] + 1], 190, 350, fill=BG, width=4)
    d.arc(box_b, 190, 350, fill=INK, width=2)
    cup_h = max(5, r)
    cup_w = 3 if r < 8 else 4
    for side in (-1, 1):
        x0 = cx + side * (r + 1) - (cup_w if side < 0 else 0)
        y0 = cy - cup_h // 2 + shake * side
        box = [x0, y0, x0 + cup_w, y0 + cup_h]
        d.rectangle([box[0] - 1, box[1] - 1, box[2] + 1, box[3] + 1], fill=BG)
        if style == "B_chibi":
            d.rectangle(box, outline=INK, fill=BG)
            for yy in range(box[1] + 1, box[3]):
                for xx in range(box[0] + 1, box[2]):
                    if (xx + yy) % 2 == 0:
                        d.point((xx, yy), fill=INK)
        else:
            d.rectangle(box, fill=INK)
        if style == "C_cyber":
            # anten LED + quang sang
            ax = box[2] if side > 0 else box[0]
            d.line([ax, box[1] - 1, ax, box[1] - 4], fill=INK)
            d.point((ax, box[1] - 6), fill=INK)
            gx = box[2] + 2 if side > 0 else box[0] - 2
            for yy in range(box[1], box[3] + 1, 2):
                d.point((gx, yy), fill=INK)


def _draw_head(img, d, cx, cy, st, name, facing=1):
    r = st["head_r"]
    if st["body"] == "outline":
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=BG, outline=INK, width=1)
        if st.get("hair"):
            spikes = [(cx - r, cy - 2), (cx - r + 2, cy - r - 3), (cx - 3, cy - r + 1),
                      (cx, cy - r - 4), (cx + 3, cy - r + 1), (cx + r - 2, cy - r - 3),
                      (cx + r, cy - 2), (cx + r - 2, cy - r + 3), (cx - r + 2, cy - r + 3)]
            d.polygon(spikes, fill=INK)
        # mat to + mieng cuoi
        ex = cx + facing * 1
        d.rectangle([ex - 4, cy, ex - 3, cy + 2], fill=INK)
        d.rectangle([ex + 3, cy, ex + 4, cy + 2], fill=INK)
        d.line([ex - 1, cy + 5, ex + 1, cy + 5], fill=INK)
        return
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=INK)
    if st.get("cap"):
        d.rectangle([cx - r, cy - r, cx + r, cy - r + 2], fill=INK)
        bx = cx + facing * r
        d.line([bx, cy - r + 2, bx + facing * 4, cy - r + 2], fill=INK, width=2)
        d.point((cx + facing * 3, cy), fill=BG)  # mat
    if st.get("visor"):
        d.rectangle([cx - r + 1, cy - 1, cx + r - 1, cy], fill=BG)
        d.point((cx + facing * 2, cy), fill=INK)


def _draw_shoe(d, x, y, st):
    if st["body"] == "outline":
        d.ellipse([x - 3, y - 2, x + 4, y + 1], fill=BG, outline=INK)
    elif st["body"] == "dither":
        d.rectangle([x - 2, y - 2, x + 3, y + 1], fill=INK)
    else:
        d.ellipse([x - 3, y - 2, x + 4, y + 1], fill=INK)
        d.line([x - 1, y - 1, x + 2, y - 1], fill=BG)


def draw_dancer(img, pose_name, style_name, ox=0, oy=0, facing=1, shake=0):
    """Ve dancer vao img. Tra ve dict khop da quy doi (de gan phu kien)."""
    st = STYLES[style_name]
    p = _scale_pose(POSES[pose_name], st["vscale"], ox, oy)
    d = ImageDraw.Draw(img)
    outline_limbs = st["body"] == "outline"

    for s in ("l", "r"):
        draw_limb(d, [p["hip"], p[f"{s}_knee"], p[f"{s}_foot"]], st["leg_w"] + (1 if outline_limbs else 0),
                  outline=outline_limbs)
        _draw_shoe(d, p[f"{s}_foot"][0], p[f"{s}_foot"][1], st)

    (nx, ny), (hx, hy) = p["neck"], p["hip"]
    w = st["torso_w"]
    torso = [(nx - w // 2, ny), (nx + w // 2, ny), (hx + w // 2 - 1, hy), (hx - w // 2 + 1, hy)]
    _fill_region(img, lambda dd: dd.polygon(torso, fill=INK), st["body"])
    if st.get("cap"):  # tui hoodie
        d.line([hx - 3, hy - 4, hx + 3, hy - 4], fill=BG)

    for s in ("l", "r"):
        pts = [(nx + (-w // 2 if s == "l" else w // 2), ny + 1), p[f"{s}_elb"], p[f"{s}_hand"]]
        draw_limb(d, pts, st["arm_w"] + (1 if outline_limbs else 0), outline=outline_limbs)
        hxp, hyp = p[f"{s}_hand"]
        d.ellipse([hxp - 1, hyp - 1, hxp + 1, hyp + 1], fill=INK)

    r = st["head_r"]
    cx, cy = nx, ny - r - 1
    _draw_head(img, d, cx, cy, st, style_name, facing)
    draw_headphones(d, cx, cy, r, style_name, shake)
    p["head"] = (cx, cy)
    return p
