import io
import requests
from PIL import Image, ImageEnhance
import cache

W, H = 64, 32

ONE_DAY = 86400


def _process(img: Image.Image) -> list:
    """Resize image to 64x32, boost contrast/saturation for LED, return pixel list."""
    # Crop to 2:1 aspect ratio (center crop) before resizing
    iw, ih = img.size
    target_ratio = W / H  # 2.0
    current_ratio = iw / ih

    if current_ratio > target_ratio:
        # wider than needed — crop sides
        new_w = int(ih * target_ratio)
        offset = (iw - new_w) // 2
        img = img.crop((offset, 0, offset + new_w, ih))
    else:
        # taller than needed — crop top/bottom, bias toward top (sky/subject)
        new_h = int(iw / target_ratio)
        img = img.crop((0, 0, iw, new_h))

    img = img.resize((W, H), Image.LANCZOS)
    img = img.convert("RGB")

    # Boost for LED visibility — LEDs need more punch than screen images
    img = ImageEnhance.Contrast(img).enhance(1.3)
    img = ImageEnhance.Color(img).enhance(1.4)
    img = ImageEnhance.Brightness(img).enhance(1.1)

    pixels = list(img.getdata())  # list of (r, g, b) tuples, row by row
    return [list(p) for p in pixels]


def apod_frame(url: str) -> dict:
    cached = cache.get("apod_frame", ONE_DAY)
    if cached:
        return cached

    r = requests.get(url, timeout=15)
    r.raise_for_status()
    img = Image.open(io.BytesIO(r.content))
    pixels = _process(img)

    result = {"width": W, "height": H, "pixels": pixels}
    cache.set("apod_frame", result)
    return result
