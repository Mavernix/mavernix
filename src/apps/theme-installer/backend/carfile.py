"""Decodes the asset catalogue (.car file) of the ProKit framework.

Usage:

    python carfile.py ROOT/System/Library/PrivateFrameworks/ProKit.framework/Versions/A/Resources/ProThemeBits.car \\
        WORK/artwork/prokit

Writes one PNG per picture in a folder per "look", plus index.json,
colors.json and gradients.json. ProThemeBits.car holds the dark artwork of
the professional programs; the dark theme is made from it. Numbers are big-endian in
the container and little-endian inside the records:

    BOM store            "BOMStore", a table of numbered blocks, and a table
                         of named blocks (CARHEADER, RENDITIONS, COLORS, ...)
    B-tree               each named tree holds (key block, value block)
                         pairs in a chain of leaf pages
    Rendition record     "ISKP" header with size, pixel format and the name
                         of the Photoshop file it came from, a list of tagged
                         values (slices, metrics), then the payload
    Payload              "MLEP": pixels, raw or run-length coded by row;
                         "DARG": a gradient given as numbers
"""

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image

# --- BOM container -------------------------------------------------------------
# magic (8 bytes), version, block count, offset and length of the block
# table, offset and length of the named-block table. Big-endian.
BOM_HEADER = struct.Struct(">8sIIIIII")
BOM_MAGIC = b"BOMStore"

# A tree's header block: "tree", version, number of its first page, page
# size, number of entries, one flag byte.
TREE_HEADER = struct.Struct(">4sIIIIB")

# A tree page: is-leaf flag, entry count, next page, previous page. Each
# entry after it is (value block, key block). A page that is not a leaf
# leads to its first child through the value block of its first entry.
TREE_PAGE = struct.Struct(">HHII")
TREE_ENTRY = struct.Struct(">II")

# --- Rendition key ---------------------------------------------------------------
# Thirteen 16-bit numbers; only the first three and the last are understood.
KEY = struct.Struct("<13H")
KEY_LOOK, KEY_ELEMENT, KEY_PART, KEY_SCALE = 0, 1, 2, 12

# --- Rendition header ------------------------------------------------------------
#   magic "ISKP", version, flags, width, height, scale (always 0 here),
#   pixel format ("BGRA": "ARGB" kept as a little-endian number), colour
#   space, modification time, layout (u16), unused (u16), name of the source file (128 bytes, zero-padded),
#   length of the tagged values, unknown, unused, length of the payload.
RENDITION_HEADER = struct.Struct("<4sIIIII4sIIHH128sIIII")
RENDITION_MAGIC = b"ISKP"

# Tagged values between header and payload: tag, length, then the value.
TAGGED_VALUE = struct.Struct("<II")
TAG_SLICES = 1001   # count, then x, y, width, height of each slice
TAG_METRICS = 1003  # count, then the stretchable insets and the full size

# --- Payloads --------------------------------------------------------------------
# Pixels start with magic, version, a kind number and the length of the rest.
# Gradients have no kind number: magic, version, length of the rest.
PIXELS_HEADER = struct.Struct("<4sIII")
GRADIENT_HEADER = struct.Struct("<4sII")
PIXELS_MAGIC = b"MLEP"
GRADIENT_MAGIC = b"DARG"
PIXELS_RAW = 0
PIXELS_ROW_CODED = 1

# A row-coded payload continues with: bytes per pixel, width, height, and
# for each row the offset of that row's data from the start of this part.
ROW_CODED_HEADER = struct.Struct("<III")

# Inside a row, a run is a 32-bit count followed by pixels. With the top bit
# set, one pixel follows, repeated that many times; otherwise that many pixels.
REPEAT_FLAG = 0x80000000

BYTES_PER_PIXEL = 4

# Output folder names, after the prefixes of each look's source file names.
LOOK_LABELS = {1: "look-1-PAA", 2: "look-2-DPA", 3: "look-3-GPA", 4: "look-4", 5: "look-5", 7: "look-7"}


def fail(message):
    sys.exit(f"carfile: {message}")


class BomStore:
    """Read access to the numbered and named blocks of a BOM file."""

    def __init__(self, data):
        self.data = data
        magic, _version, _count, table_offset, _table_length, names_offset, _names_length = (
            BOM_HEADER.unpack_from(data, 0)
        )
        if magic != BOM_MAGIC:
            fail(f"the file does not start with {BOM_MAGIC!r}; is it a .car file?")

        (block_count,) = struct.unpack_from(">I", data, table_offset)
        self.blocks = [
            struct.unpack_from(">II", data, table_offset + 4 + 8 * number)
            for number in range(block_count)
        ]

        self.named = {}
        (name_count,) = struct.unpack_from(">I", data, names_offset)
        position = names_offset + 4
        for _ in range(name_count):
            block_number, name_length = struct.unpack_from(">IB", data, position)
            name = data[position + 5:position + 5 + name_length].decode("ascii")
            self.named[name] = block_number
            position += 5 + name_length

    def block(self, number):
        offset, length = self.blocks[number]
        return self.data[offset:offset + length]

    def tree(self, name):
        """All (key, value) pairs of the named tree, in the file's order."""
        if name not in self.named:
            fail(f"the file has no tree named {name}")
        _magic, _version, first_page, _page_size, _count, _flag = TREE_HEADER.unpack_from(
            self.block(self.named[name])
        )

        # Go down the first children to the leftmost leaf page.
        page = self.block(first_page)
        while not TREE_PAGE.unpack_from(page)[0]:
            first_child, _ = TREE_ENTRY.unpack_from(page, TREE_PAGE.size)
            page = self.block(first_child)

        pairs = []
        while True:
            _is_leaf, entry_count, next_page, _previous = TREE_PAGE.unpack_from(page)
            for number in range(entry_count):
                value_block, key_block = TREE_ENTRY.unpack_from(
                    page, TREE_PAGE.size + number * TREE_ENTRY.size
                )
                pairs.append((self.block(key_block), self.block(value_block)))
            if next_page == 0:
                return pairs
            page = self.block(next_page)


def zero_terminated(raw):
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def read_info_names(store, tree_name):
    """Element or part names: number -> (identifier, readable name).

    Each value is two lengths followed by the two strings.
    """
    names = {}
    for key, value in store.tree(tree_name):
        (number,) = struct.unpack_from("<I", key)
        identifier_length, label_length = struct.unpack_from("<II", value)
        identifier = value[8:8 + identifier_length].decode("ascii", "replace")
        label = value[8 + identifier_length:8 + identifier_length + label_length].decode("ascii", "replace")
        names[number] = (identifier, label)
    return names


def read_colours(store):
    """Named colours of each look: {look: {name: "#rrggbbaa"}}.

    Key: the look (u32) and the name. Value: two numbers, then four bytes
    taken to be blue, green, red, alpha (the colour order is inferred).
    """
    colours = {}
    for key, value in store.tree("COLORS"):
        (look,) = struct.unpack_from("<I", key)
        name = zero_terminated(key[4:])
        blue, green, red, alpha = value[8:12]
        colours.setdefault(look, {})[name] = f"#{red:02x}{green:02x}{blue:02x}{alpha:02x}"
    return colours


def read_tagged_values(record, start, length):
    """The slices and metrics between header and payload, as lists of numbers."""
    values = {}
    position = start
    while position < start + length:
        tag, value_length = TAGGED_VALUE.unpack_from(record, position)
        numbers = list(struct.unpack_from(f"<{value_length // 4}I", record, position + TAGGED_VALUE.size))
        if tag == TAG_SLICES:
            slice_count = numbers[0]
            values["slices"] = [numbers[1 + 4 * n:5 + 4 * n] for n in range(slice_count)]
        elif tag == TAG_METRICS:
            values["metrics"] = numbers
        else:
            values[f"tag_{tag}"] = numbers
        position += TAGGED_VALUE.size + value_length
    return values


def decode_row_coded(payload, width, height):
    """Undoes ProKit's per-row run-length coding. Returns ARGB bytes."""
    bytes_per_pixel, coded_width, coded_height = ROW_CODED_HEADER.unpack_from(payload)
    if (bytes_per_pixel, coded_width, coded_height) != (BYTES_PER_PIXEL, width, height):
        fail(
            f"row-coded picture says {coded_width}x{coded_height} at {bytes_per_pixel} bytes "
            f"a pixel, its header {width}x{height} at {BYTES_PER_PIXEL}"
        )
    row_offsets = struct.unpack_from(f"<{height}I", payload, ROW_CODED_HEADER.size)

    pixels = bytearray()
    for row, offset in enumerate(row_offsets):
        filled = 0
        position = offset
        while filled < width:
            (count,) = struct.unpack_from("<I", payload, position)
            position += 4
            if count & REPEAT_FLAG:
                count &= ~REPEAT_FLAG
                pixels += payload[position:position + BYTES_PER_PIXEL] * count
                position += BYTES_PER_PIXEL
            else:
                pixels += payload[position:position + count * BYTES_PER_PIXEL]
                position += count * BYTES_PER_PIXEL
            filled += count
        if filled != width:
            fail(f"row {row} of a picture holds {filled} pixels, not {width}")
    return bytes(pixels)


def to_picture(argb, width, height):
    # Pixels are premultiplied ARGB (the format word "BGRA" is "ARGB" read
    # little-endian). Pillow cannot unpack alpha-first premultiplied pixels
    # in one step, so the channels are put in order first.
    in_order = Image.frombytes("RGBA", (width, height), argb, "raw", "ARGB")
    return Image.frombytes("RGBa", (width, height), in_order.tobytes()).convert("RGBA")


def read_rendition(record):
    """One rendition. Returns (description, picture or None, gradient or None)."""
    (
        magic, _version, _flags, width, height, _scale, pixel_format, colour_space,
        _modified, layout, _unused, raw_name, tagged_length, _unknown, _zero, payload_length,
    ) = RENDITION_HEADER.unpack_from(record)
    if magic != RENDITION_MAGIC:
        fail(f"a rendition starts with {magic!r}, not {RENDITION_MAGIC!r}")

    description = {
        "source_name": zero_terminated(raw_name),
        "width": width,
        "height": height,
        "pixel_format": pixel_format.decode("ascii"),
        "colour_space": colour_space,
        "layout": layout,
        **read_tagged_values(record, RENDITION_HEADER.size, tagged_length),
    }

    payload_start = RENDITION_HEADER.size + tagged_length
    payload_magic = record[payload_start:payload_start + 4]

    if payload_magic == GRADIENT_MAGIC:
        _magic, _version, rest_length = GRADIENT_HEADER.unpack_from(record, payload_start)
        rest_start = payload_start + GRADIENT_HEADER.size
        rest = record[rest_start:rest_start + rest_length]
        # Not fully understood: a four-letter type ("Lnr ", linear), two
        # numbers, then 64-bit floats holding the colour stops. Kept as numbers.
        gradient = {
            "type": rest[:4][::-1].decode("ascii", "replace"),
            "numbers": list(struct.unpack_from("<II", rest, 4)),
            "floats": [round(x, 6) for x in struct.unpack_from(f"<{(len(rest) - 12) // 8}d", rest, 12)],
        }
        description["kind"] = "gradient"
        return description, None, gradient

    if payload_magic != PIXELS_MAGIC:
        fail(f"unknown payload {payload_magic!r} in {description['source_name']}")
    _magic, _version, kind, rest_length = PIXELS_HEADER.unpack_from(record, payload_start)
    rest_start = payload_start + PIXELS_HEADER.size
    rest = record[rest_start:rest_start + rest_length]
    if width == 0 or height == 0:
        description["kind"] = "empty"
        return description, None, None

    if kind == PIXELS_RAW:
        argb = rest[:width * height * BYTES_PER_PIXEL]
    elif kind == PIXELS_ROW_CODED:
        argb = decode_row_coded(rest, width, height)
    else:
        fail(f"unknown pixel coding {kind} in {description['source_name']}")
    description["kind"] = "pixels"
    return description, to_picture(argb, width, height), None


def decode(car_path, output_dir):
    store = BomStore(car_path.read_bytes())
    elements = read_info_names(store, "ELEMENT_INFO")
    parts = read_info_names(store, "PART_INFO")

    output_dir.mkdir(parents=True, exist_ok=True)
    index = []
    gradients = []
    count_in_source = {}

    for key, record in store.tree("RENDITIONS"):
        numbers = list(KEY.unpack_from(key))
        description, picture, gradient = read_rendition(record)
        look = numbers[KEY_LOOK]
        scale = numbers[KEY_SCALE]
        element = elements.get(numbers[KEY_ELEMENT], ("", ""))[1]
        part = parts.get(numbers[KEY_PART], ("", ""))[1]

        entry = {"key": numbers, "look": look, "scale": scale, "element": element, "part": part, **description}

        if picture is not None:
            # Many renditions come from one source file (its states, sizes
            # and slices), so a running number per source keeps names apart.
            stem = Path(description["source_name"]).stem
            number = count_in_source.get((look, stem), 0)
            count_in_source[(look, stem)] = number + 1
            suffix = "@2x" if scale == 2 else ""
            folder = output_dir / LOOK_LABELS.get(look, f"look-{look}")
            folder.mkdir(exist_ok=True)
            file_name = f"{stem}-{number:03d}{suffix}.png"
            picture.save(folder / file_name)
            entry["file"] = f"{folder.name}/{file_name}"
        if gradient is not None:
            gradients.append({"key": numbers, "look": look, "source_name": description["source_name"], **gradient})
        index.append(entry)

    (output_dir / "index.json").write_text(json.dumps(
        {
            "source": car_path.name,
            "elements": {number: names for number, names in elements.items()},
            "parts": {number: names for number, names in parts.items()},
            "renditions": index,
        },
        indent=1,
    ))
    (output_dir / "colors.json").write_text(json.dumps(read_colours(store), indent=1, sort_keys=True))
    (output_dir / "gradients.json").write_text(json.dumps(gradients, indent=1))
    return len(index)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("car_file", type=Path, help="path to a .car file such as ProThemeBits.car")
    parser.add_argument("output_dir", type=Path, help="folder for the PNGs and the JSON files")
    arguments = parser.parse_args()
    if not arguments.car_file.is_file():
        fail(f"{arguments.car_file} is not a file")
    count = decode(arguments.car_file, arguments.output_dir)
    print(f"carfile: {count} renditions from {arguments.car_file.name} into {arguments.output_dir}")


if __name__ == "__main__":
    main()
