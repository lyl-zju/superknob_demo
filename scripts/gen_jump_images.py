"""Generate the small Chinese text images used by the Jump game UI."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PROJECT_DIR = Path(__file__).resolve().parent.parent
OUT_DIR = PROJECT_DIR / "src" / "ui_image_src"
FONT_CANDIDATES = (
    Path("C:/Windows/Fonts/msyh.ttc"),
    Path("C:/Windows/Fonts/simhei.ttf"),
    Path("C:/Windows/Fonts/simsun.ttc"),
)

IMAGES = (
    ("跳一跳", "jump_title_img", "jump_title_img.c", 22, (0, 0, 0)),
    ("再来一局", "jump_retry_img", "jump_retry_img.c", 20, (255, 255, 255)),
)


def find_font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_CANDIDATES:
        if path.exists():
            return ImageFont.truetype(str(path), size=size)
    raise FileNotFoundError("No supported Chinese font found in C:/Windows/Fonts")


def render_text(text: str, target_height: int, color: tuple[int, int, int]) -> Image.Image:
    font = find_font(target_height * 2)
    bbox = font.getbbox(text)
    image = Image.new("RGBA", (bbox[2] - bbox[0] + 4, bbox[3] - bbox[1] + 4), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.text((2 - bbox[0], 2 - bbox[1]), text, font=font, fill=(*color, 255))
    content = image.getbbox()
    image = image.crop(content)
    width = max(1, round(image.width * target_height / image.height))
    return image.resize((width, target_height), Image.Resampling.LANCZOS)


def rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def write_c(symbol: str, filename: str, image: Image.Image) -> None:
    data: list[int] = []
    for r, g, b, a in image.getdata():
        color = rgb565(r, g, b)
        data.extend((color & 0xFF, color >> 8, a))

    lines = [
        '#include "lvgl.h"\n\n',
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n\n",
        f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {symbol}_map[] = {{\n",
    ]
    for offset in range(0, len(data), 16):
        chunk = ", ".join(f"0x{value:02x}" for value in data[offset : offset + 16])
        lines.append(f"  {chunk},\n")
    lines.extend(
        (
            "};\n\n",
            f"const lv_img_dsc_t {symbol} = {{\n",
            "  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n",
            "  .header.always_zero = 0,\n",
            "  .header.reserved = 0,\n",
            f"  .header.w = {image.width},\n",
            f"  .header.h = {image.height},\n",
            f"  .data_size = {image.width * image.height} * LV_IMG_PX_SIZE_ALPHA_BYTE,\n",
            f"  .data = {symbol}_map,\n",
            "};\n",
        )
    )
    (OUT_DIR / filename).write_text("".join(lines), encoding="utf-8")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for text, symbol, filename, height, color in IMAGES:
        image = render_text(text, height, color)
        write_c(symbol, filename, image)
        print(f"{text} -> {filename} ({image.width}x{image.height})")


if __name__ == "__main__":
    main()
