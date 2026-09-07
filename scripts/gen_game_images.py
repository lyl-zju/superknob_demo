# -*- coding: utf-8 -*-
"""
把 pictures/ 下的中文词 PNG 转成 LVGL 图像 C 文件（黑色文字 + 透明底）。

流程：
1. 按亮度把「深色(文字/边框)」与「浅色(背景)」分开：
      alpha = 255 - 亮度，近白背景(亮度>235)强制透明。
2. 去掉贴边的深色边框：从图像四边做 flood-fill，把与边缘连通的深色区域
   （外框）设为透明，只保留中间的文字。
3. 裁剪到文字外接框（留 1px），缩放到目标高度。
4. 输出 LVGL 8 TRUE_COLOR_ALPHA C 文件（LV_COLOR_DEPTH=16 时每像素 3 字节 =
   RGB565 低字节、RGB565 高字节、alpha）。
"""
import os
from collections import deque
from PIL import Image

SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "pictures")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "src", "ui_image_src")

# 中文文件名 -> (C 符号名, C 文件名, 目标高度 px)
IMAGES = [
    ("游戏.png",   "game_title_img",     "game_title_img.c",     22),
    ("开始.png",   "game_start_img",     "game_start_img.c",     18),
    ("难度.png",   "game_diff_img",      "game_diff_img.c",      18),
    ("最高分.png", "game_highscore_img", "game_highscore_img.c", 18),
    ("简单.png",   "game_easy_img",      "game_easy_img.c",      16),
    ("中等.png",   "game_medium_img",    "game_medium_img.c",    16),
    ("困难.png",   "game_hard_img",      "game_hard_img.c",      16),
    ("钓到了.png", "game_caught_img",    "game_caught_img.c",    20),
    ("没钓到.png", "game_missed_img",    "game_missed_img.c",    20),
]


def to_black_alpha(rgba):
    """深色→黑+alpha，浅色→透明。返回 (RGBA 图, 前景 bool 掩码)。"""
    w, h = rgba.size
    src = rgba.load()
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    dst = out.load()
    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            lum = 0.299 * r + 0.587 * g + 0.114 * b
            alpha = 255 - lum
            if lum > 235:          # 近白背景强制透明
                alpha = 0
            if alpha < 0:
                alpha = 0
            elif alpha > 255:
                alpha = 255
            dst[x, y] = (0, 0, 0, int(round(alpha)))
            mask[y][x] = lum < 200  # 前景（文字+边框，含抗锯齿）
    return out, mask


def remove_edge_border(rgba, mask):
    """从四边 flood-fill，把与边缘连通的深色区域（外框）设为透明。"""
    w, h = rgba.size
    visited = [[False] * w for _ in range(h)]
    q = deque()
    # 所有边缘上的前景像素作为种子
    for x in range(w):
        if mask[0][x] and not visited[0][x]:
            q.append((x, 0)); visited[0][x] = True
        if mask[h - 1][x] and not visited[h - 1][x]:
            q.append((x, h - 1)); visited[h - 1][x] = True
    for y in range(h):
        if mask[y][0] and not visited[y][0]:
            q.append((0, y)); visited[y][0] = True
        if mask[y][w - 1] and not visited[y][w - 1]:
            q.append((w - 1, y)); visited[y][w - 1] = True

    px = rgba.load()
    while q:
        x, y = q.popleft()
        px[x, y] = (0, 0, 0, 0)  # 移除边框
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1),
                       (x + 1, y + 1), (x + 1, y - 1), (x - 1, y + 1), (x - 1, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and mask[ny][nx] and not visited[ny][nx]:
                visited[ny][nx] = True
                q.append((nx, ny))
    return rgba


def crop_to_text(rgba, pad=1, thr=50):
    """裁剪到 alpha > thr 的外接框（排除边框残留的淡像素）。"""
    w, h = rgba.size
    px = rgba.load()
    minx, miny, maxx, maxy = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if px[x, y][3] > thr:
                if x < minx: minx = x
                if x > maxx: maxx = x
                if y < miny: miny = y
                if y > maxy: maxy = y
    if maxx < 0:
        return rgba
    l = max(0, minx - pad); t = max(0, miny - pad)
    r = min(w, maxx + 1 + pad); b = min(h, maxy + 1 + pad)
    return rgba.crop((l, t, r, b))


def rgb_to_rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def gen_c_file(symbol, path, rgba):
    w, h = rgba.size
    px = rgba.load()
    data = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            c = rgb_to_rgb565(r, g, b)
            data.append(c & 0xFF)
            data.append((c >> 8) & 0xFF)
            data.append(a)

    lines = ['#include "lvgl.h"\n', "\n",
             "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n\n",
             "const LV_ATTRIBUTE_MEM_ALIGN uint8_t %s_map[] = {\n" % symbol]
    for i in range(0, len(data), 16):
        lines.append("  %s,\n" % ", ".join("0x%02x" % v for v in data[i:i + 16]))
    lines.append("};\n\n")
    lines.append("const lv_img_dsc_t %s = {\n" % symbol)
    lines.append("  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n")
    lines.append("  .header.always_zero = 0,\n")
    lines.append("  .header.reserved = 0,\n")
    lines.append("  .header.w = %d,\n" % w)
    lines.append("  .header.h = %d,\n" % h)
    lines.append("  .data_size = %d * LV_IMG_PX_SIZE_ALPHA_BYTE,\n" % (w * h))
    lines.append("  .data = %s_map,\n" % symbol)
    lines.append("};\n")

    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for fname, symbol, cname, target_h in IMAGES:
        src = os.path.join(SRC_DIR, fname)
        if not os.path.exists(src):
            print("MISSING:", fname)
            continue
        rgba = Image.open(src).convert("RGBA")
        out, mask = to_black_alpha(rgba)
        out = remove_edge_border(out, mask)
        out = crop_to_text(out)
        w, h = out.size
        if h != target_h:
            tw = max(1, round(w * target_h / h))
            out = out.resize((tw, target_h), Image.LANCZOS)
        gen_c_file(symbol, os.path.join(OUT_DIR, cname), out)
        print("%s -> %s  cropped=%dx%d -> out=%s" % (fname, cname, w, h, out.size))


if __name__ == "__main__":
    main()
