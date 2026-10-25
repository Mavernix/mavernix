"""Builds the Sea Lion theme's mouse pointers from the installer's cursors.

    python make_cursors.py WORK OUTPUT/sea-lion

WORK holds the unpacked installer under root/; run it after make_icons.py,
whose icon theme the pointers join (THEME/share/icons/SeaLion/cursors/), so
one name serves both. Two sources:

- the arrow, the text beam and the spinning wait cursor are pictures inside
  CoreGraphics, found by their symbols (__kCGX...CursorBitmap0..3, each a
  width and height followed by premultiplied BGRA pixels; _kCGX...Cursor
  describes each: size and hot spot in points, frames, and its pictures);
- the others are drawings in HIServices' cursors/NAME/cursor.pdf, with the
  hot spot, the shadow and the frames of an animation in info.plist.

Each pointer is written as an Xcursor file in several sizes, and linked
under the names programs ask for: the CSS names (default, text, pointer,
ew-resize, ...) and the older X ones (left_ptr, xterm, hand2, ...).
"""

import argparse
import math
import plistlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageFilter

from macho import MachO

CURSOR_THEME_NAME = "SeaLion"

CORE_GRAPHICS = "System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics"
HISERVICES_CURSORS = ("System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/"
                      "HIServices.framework/Versions/A/Resources/cursors")

# The sizes written into each file. Programs ask for their size times the
# screen's scale (24 at 1, 36 at 1.5, 48 at 2) and get the nearest one. A
# point of the Mac's cursors is a pixel at size 24.
NOMINAL_SIZES = [24, 30, 36, 48, 60, 72, 96]
POINTS_AT_NOMINAL = 24

# CoreGraphics' cursors: our file name -> the symbol's middle part.
CORE_GRAPHICS_CURSORS = {"arrow": "Arrow", "ibeam": "IBeam", "wait": "Wait"}

# The shadow WindowServer draws under the arrow and the text beam; the
# HIServices drawings name theirs in info.plist. (The wait cursor's pictures
# have theirs drawn in.)
SYSTEM_SHADOW = {"shadowcolor": [0, 0, 0, 0.65], "blur": 1.8, "shadowoffsetx": 0, "shadowoffsety": -1}
SHADOWED_CORE_GRAPHICS_CURSORS = {"arrow", "ibeam"}

# Our file name (a CoreGraphics cursor or an HIServices folder) -> the names
# programs ask for. The first is the file; the others link to it.
NAMES = {
    "arrow": ["default", "left_ptr", "arrow", "top_left_arrow", "left_arrow", "right_ptr"],
    "ibeam": ["text", "xterm", "ibeam"],
    "wait": ["wait", "watch"],
    "busybutclickable": ["progress", "left_ptr_watch", "half-busy",
                         "08e8e1c95fe2fc01f976f1e063a24ccd", "3ecb610c1bf2410f44200f48c40d3599"],
    "ibeamvertical": ["vertical-text"],
    "pointinghand": ["pointer", "hand", "hand1", "hand2", "pointing_hand",
                     "e29285e634086352946a0e7090d73106", "9d800788f1b08800ae810202380a0822"],
    "openhand": ["grab", "openhand", "5aca4d189052212118709018842178c0"],
    "closedhand": ["grabbing", "closedhand", "dnd-none", "208530c400c041818281048008011002"],
    "cross": ["crosshair", "cross", "tcross", "cross_reverse", "diamond_cross"],
    "cell": ["cell", "plus"],
    "copy": ["copy", "dnd-copy", "1081e37283d90000800003c07f3ef6bf", "6407b0e94181790501fd1e167b474872"],
    "makealias": ["alias", "link", "dnd-link", "3085a0e285430894940527032f8b26df",
                  "640fb0e74195791501fd1ed57b41487f"],
    "contextualmenu": ["context-menu", "dnd-ask"],
    "help": ["help", "question_arrow", "whats_this", "left_ptr_help",
             "5c6cd98b3f3ebcb1f9c7f1c204630408", "d9ce0ab605698f320427677b458ad60b"],
    "notallowed": ["not-allowed", "no-drop", "crossed_circle", "forbidden", "dnd-no-drop", "circle",
                   "pirate", "X_cursor", "03b6e0fcb3499374a867c041f52298f0"],
    "move": ["move", "all-scroll", "fleur", "size_all", "dnd-move", "all-resize",
             "4498f0e0c1937ffe01fd06f973665830", "9081237383d90e509aa00f00170e968f"],
    "resizeeastwest": ["ew-resize", "sb_h_double_arrow", "h_double_arrow", "size_hor",
                       "028006030e0e7ebffc7f7070c0600140", "14fef782d02440884392942c11205230"],
    "resizenorthsouth": ["ns-resize", "sb_v_double_arrow", "v_double_arrow", "size_ver",
                         "00008160000006810000408080010102", "2870a09082c103050810ffdffffe0204"],
    "resizeleftright": ["col-resize", "split_h", "043a9f68147c53184671403ffa811cc5"],
    "resizeupdown": ["row-resize", "split_v", "c07385c7190e701020ff7ffffd08103c"],
    "resizeeast": ["e-resize", "right_side"],
    "resizewest": ["w-resize", "left_side"],
    "resizenorth": ["n-resize", "top_side"],
    "resizesouth": ["s-resize", "bottom_side"],
    "resizenortheast": ["ne-resize", "top_right_corner"],
    "resizenorthwest": ["nw-resize", "top_left_corner"],
    "resizesoutheast": ["se-resize", "bottom_right_corner"],
    "resizesouthwest": ["sw-resize", "bottom_left_corner"],
    "resizenortheastsouthwest": ["nesw-resize", "fd_double_arrow", "size_bdiag", "fcf1c3c7cd4491d801f1e1c78f100000"],
    "resizenorthwestsoutheast": ["nwse-resize", "bd_double_arrow", "size_fdiag", "c7088f0f3e6c8088236ef8e1e3e70000"],
    "zoomin": ["zoom-in", "zoom_in"],
    "zoomout": ["zoom-out", "zoom_out"],
}

# Xcursor's file format (Xcursor(3)): a header, a table of contents, and an
# image chunk per picture, its pixels premultiplied ARGB in native order.
XCURSOR_MAGIC = b"Xcur"
XCURSOR_IMAGE_TYPE = 0xFFFD0002
XCURSOR_FILE_HEADER_BYTES = 16
XCURSOR_TOC_ENTRY_BYTES = 12
XCURSOR_IMAGE_HEADER_BYTES = 36


def fail(message):
    sys.exit(f"make_cursors: {message}")


class Frame:
    """One picture of a cursor at one size: premultiplied, with its hot spot."""

    def __init__(self, picture, hot_x, hot_y, delay_ms=0):
        self.picture = picture  # mode "RGBa"
        self.hot_x = min(max(round(hot_x), 0), picture.width - 1)
        self.hot_y = min(max(round(hot_y), 0), picture.height - 1)
        self.delay_ms = delay_ms


def with_shadow(picture, hot_x, hot_y, shadow, factor):
    """The picture (straight "RGBA") over its shadow, on a canvas grown to
    hold the blur, and the hot spot moved with it."""
    color = shadow["shadowcolor"]
    blur = shadow.get("blur", 0) * factor
    # The Mac's y axis points up: an offset of -1 is one point down.
    offset_x = round(shadow.get("shadowoffsetx", 0) * factor)
    offset_y = round(-shadow.get("shadowoffsety", 0) * factor)
    margin = math.ceil(blur * 2) + max(abs(offset_x), abs(offset_y))
    canvas = Image.new("RGBA", (picture.width + 2 * margin, picture.height + 2 * margin))
    shade = Image.new("RGBA", picture.size, tuple(round(c * 255) for c in color[:3]) + (0,))
    shade.putalpha(picture.getchannel("A").point(lambda a: round(a * color[3])))
    canvas.alpha_composite(shade, (margin + offset_x, margin + offset_y))
    if blur > 0:
        # Blurring straight alpha would smear the shade's colour into the
        # clear pixels, which are black anyway.
        canvas = canvas.filter(ImageFilter.GaussianBlur(blur / 2))
    canvas.alpha_composite(picture, (margin, margin))
    return canvas, hot_x + margin, hot_y + margin


def core_graphics_cursor(binary, symbol_name):
    """{"width", "height", "hot_x", "hot_y", "frames", "interval_s", "bitmaps"}
    for _kCGX<NAME>Cursor, its bitmaps as (scale, [straight RGBA frames])."""
    symbols = {symbol.name: symbol.address for symbol in binary.symbols}
    address = symbols.get(f"_kCGX{symbol_name}Cursor")
    if address is None:
        fail(f"CoreGraphics has no _kCGX{symbol_name}Cursor; is this OS X 10.9's?")
    # Four doubles (size and hot spot in points), the frame count, the time
    # per frame (a float, for animations), the number of pictures, and
    # pointers to them.
    width, height, hot_x, hot_y = struct.unpack("<4d", binary.bytes_at(address, 32))
    frames, interval_s, count = struct.unpack("<IfQ", binary.bytes_at(address + 32, 16))
    pointers = struct.unpack(f"<{count}Q", binary.bytes_at(address + 48, 8 * count))
    bitmaps = []
    for pointer in pointers:
        bitmap_width, bitmap_height = struct.unpack("<II", binary.bytes_at(pointer, 8))
        frame_bytes = bitmap_width * bitmap_height * 4
        pixels = binary.bytes_at(pointer + 8, frame_bytes * frames)
        if len(pixels) != frame_bytes * frames:
            fail(f"_kCGX{symbol_name}Cursor's pictures are cut short")
        pictures = [Image.frombytes("RGBa", (bitmap_width, bitmap_height),
                                    pixels[number * frame_bytes:(number + 1) * frame_bytes], "raw", "BGRa")
                    .convert("RGBA") for number in range(frames)]
        bitmaps.append((bitmap_width / width, pictures))
    return {"width": width, "height": height, "hot_x": hot_x, "hot_y": hot_y,
            "frames": frames, "interval_s": interval_s if frames > 1 else 0,
            "bitmaps": sorted(bitmaps, key=lambda bitmap: bitmap[0])}


def frames_from_core_graphics(cursor, factor, shadow):
    """The cursor's frames at `factor` pixels per point, each made from the
    smallest of its pictures that is at least that sharp."""
    scale, pictures = next((bitmap for bitmap in cursor["bitmaps"] if bitmap[0] >= factor),
                           cursor["bitmaps"][-1])
    size = (max(1, round(cursor["width"] * factor)), max(1, round(cursor["height"] * factor)))
    frames = []
    for picture in pictures:
        if picture.size != size:
            # Scaled premultiplied, so the clear pixels' colour does not bleed in.
            picture = picture.convert("RGBa").resize(size, Image.LANCZOS).convert("RGBA")
        hot_x, hot_y = cursor["hot_x"] * factor, cursor["hot_y"] * factor
        if shadow:
            picture, hot_x, hot_y = with_shadow(picture, hot_x, hot_y, shadow, factor)
        frames.append(Frame(picture.convert("RGBa"), hot_x, hot_y, round(cursor["interval_s"] * 1000)))
    return frames


def page_size(pdf):
    """The first page's size in points."""
    result = subprocess.run(["pdfinfo", str(pdf)], capture_output=True, text=True)
    for line in result.stdout.splitlines():
        if line.startswith("Page size:"):
            words = line.split()
            return float(words[2]), float(words[4])
    fail(f"pdfinfo cannot read {pdf}: {result.stderr.strip()}")


def render_pdf(pdf, width, height, scratch):
    """The page drawn at width x height pixels, with a clear background."""
    base = scratch / pdf.parent.name
    result = subprocess.run(["pdftocairo", "-png", "-singlefile", "-transp",
                             "-scale-to-x", str(width), "-scale-to-y", str(height), str(pdf), str(base)],
                            capture_output=True, text=True)
    if result.returncode != 0:
        fail(f"pdftocairo cannot draw {pdf}: {result.stderr.strip()}")
    return Image.open(f"{base}.png").convert("RGBA")


def frames_from_drawing(folder, factor, scratch):
    """An HIServices cursor's frames at `factor` pixels per point. The frames
    of an animation lie under each other on the page, the first on top."""
    info = plistlib.loads((folder / "info.plist").read_bytes())
    page_width, page_height = page_size(folder / "cursor.pdf")
    count = int(info.get("frames", 1))
    frame_width = max(1, round(page_width * factor))
    frame_height = max(1, round(page_height / count * factor))
    sheet = render_pdf(folder / "cursor.pdf", frame_width, frame_height * count, scratch)
    # "hotx" is for cursor_1only_.png, a picture of its own at one pixel per
    # point; the drawing's hot spot is the "-scaled" one.
    hot_x = info.get("hotx-scaled", info.get("hotx", 0)) * factor
    hot_y = info.get("hoty-scaled", info.get("hoty", 0)) * factor
    shadow = info if "shadowcolor" in info else None
    frames = []
    for number in range(count):
        picture = sheet.crop((0, number * frame_height, frame_width, (number + 1) * frame_height))
        frame_hot_x, frame_hot_y = hot_x, hot_y
        if shadow:
            picture, frame_hot_x, frame_hot_y = with_shadow(picture, hot_x, hot_y, shadow, factor)
        frames.append(Frame(picture.convert("RGBa"), frame_hot_x, frame_hot_y,
                            round(info.get("delay", 0) * 1000) if count > 1 else 0))
    return frames


def write_xcursor(path, sized_frames):
    """Writes {nominal size: [Frame]} as an Xcursor file."""
    images = [(size, frame) for size, frames in sorted(sized_frames.items()) for frame in frames]
    chunks = []
    position = XCURSOR_FILE_HEADER_BYTES + XCURSOR_TOC_ENTRY_BYTES * len(images)
    table = b""
    for size, frame in images:
        picture = frame.picture
        chunk = struct.pack("<9I", XCURSOR_IMAGE_HEADER_BYTES, XCURSOR_IMAGE_TYPE, size, 1,
                            picture.width, picture.height, frame.hot_x, frame.hot_y, frame.delay_ms)
        # Native order on a little-endian machine: B, G, R, A.
        chunk += picture.tobytes("raw", "BGRa")
        table += struct.pack("<3I", XCURSOR_IMAGE_TYPE, size, position)
        position += len(chunk)
        chunks.append(chunk)
    header = XCURSOR_MAGIC + struct.pack("<3I", XCURSOR_FILE_HEADER_BYTES, 0x10000, len(images))
    path.write_bytes(header + table + b"".join(chunks))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("work", type=Path)
    parser.add_argument("theme", type=Path)
    arguments = parser.parse_args()

    root = arguments.work / "root"
    icon_theme = arguments.theme / "share" / "icons" / CURSOR_THEME_NAME
    if not (icon_theme / "index.theme").is_file():
        fail(f"{icon_theme}/index.theme is missing; run make_icons.py first")
    binary_path = root / CORE_GRAPHICS
    if not binary_path.is_file():
        fail(f"{binary_path} is missing; has the installer image been unpacked?")
    binary = MachO(binary_path)
    drawings = root / HISERVICES_CURSORS

    target = icon_theme / "cursors"
    target.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as scratch:
        for name, names in NAMES.items():
            sized = {}
            if name in CORE_GRAPHICS_CURSORS:
                cursor = core_graphics_cursor(binary, CORE_GRAPHICS_CURSORS[name])
                shadow = SYSTEM_SHADOW if name in SHADOWED_CORE_GRAPHICS_CURSORS else None
                for size in NOMINAL_SIZES:
                    sized[size] = frames_from_core_graphics(cursor, size / POINTS_AT_NOMINAL, shadow)
            else:
                folder = drawings / name
                if not (folder / "cursor.pdf").is_file():
                    fail(f"{folder}/cursor.pdf is missing; has the installer image been unpacked?")
                for size in NOMINAL_SIZES:
                    sized[size] = frames_from_drawing(folder, size / POINTS_AT_NOMINAL, Path(scratch))
            write_xcursor(target / names[0], sized)
            for alias in names[1:]:
                link = target / alias
                if link.is_symlink() or link.exists():
                    link.unlink()
                link.symlink_to(names[0])


if __name__ == "__main__":
    main()
