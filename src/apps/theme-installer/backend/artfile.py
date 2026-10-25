"""Decodes CoreUI's ArtFile.bin, the file holding the installer's control artwork.

Usage:

    python artfile.py ROOT/System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/Resources/ArtFile.bin \\
        WORK/artwork/artfile-1x

Writes one PNG per image and an index.json. ArtFile200.bin (the Retina
copy) is read the same way. The layout is the one Alex Zielenski described
for artFileTool (https://github.com/alexzielenski/artFileTool); all numbers
are little-endian.

    Master header        24 bytes
    Tag descriptors      8 bytes each
    Tag names            zero-terminated ASCII strings
    Image descriptors    12 bytes each
    Image records        a 44-byte art header followed by raw pixels

Images have no names; up to eight tags ("pushbutton", "pressed") say what each is.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image

# --- Master header -----------------------------------------------------------
# image count (u16), bits per channel (u16), tag count (u32), then four
# offsets from the start of the file (u32 each): tag descriptors, tag names,
# image descriptors, image records.
MASTER_HEADER = struct.Struct("<HHIIIII")

# --- Tag descriptor ----------------------------------------------------------
# offset of the tag's name within the tag-name area (u32), and the tag's
# number (u32). Image descriptors refer to tags by that number.
TAG_DESCRIPTOR = struct.Struct("<II")

# --- Image descriptor --------------------------------------------------------
# offset of the image record within the image-record area (u32), then eight
# tag numbers of one byte each. Unused slots are 0.
IMAGE_DESCRIPTOR = struct.Struct("<I8B")

# --- Art header --------------------------------------------------------------
# How one bitmap is cut into a grid of up to 3 x 3 stretchable parts.
#   rows, columns          u16 each   how many parts down and across
#   three rectangles       12 x u16   x, y, width, height each; purpose unknown
#   unknown                u16        thought to be a control type or state
#   row heights            3 x u16    height of each row of parts, 0 if unused
#   column widths          3 x u16    width of each column of parts, 0 if unused
#   unknown                u16
ART_HEADER = struct.Struct("<HH12HH3H3HH")

# The only pixel format seen in these files.
EXPECTED_BITS_PER_CHANNEL = 8
BYTES_PER_PIXEL = 4


def fail(message):
    sys.exit(f"artfile: {message}")


def read_tags(data, tag_count, descriptors_offset, names_offset):
    """Returns a dictionary from tag number to tag name."""
    tags = {}
    for position in range(tag_count):
        name_offset, number = TAG_DESCRIPTOR.unpack_from(
            data, descriptors_offset + position * TAG_DESCRIPTOR.size
        )
        name_start = names_offset + name_offset
        name_end = data.index(b"\0", name_start)
        tags[number] = data[name_start:name_end].decode("ascii")
    return tags


def read_image(data, record_offset):
    """Reads one image record. Returns (description, picture)."""
    fields = ART_HEADER.unpack_from(data, record_offset)
    row_count, column_count = fields[0], fields[1]
    rectangle_numbers = fields[2:14]
    unknown_after_rectangles = fields[14]
    row_heights = list(fields[15:18])
    column_widths = list(fields[18:21])
    unknown_at_end = fields[21]

    width = sum(column_widths)
    height = sum(row_heights)

    pixels_start = record_offset + ART_HEADER.size
    pixels_end = pixels_start + width * height * BYTES_PER_PIXEL
    if pixels_end > len(data):
        fail(f"image record at {record_offset} runs past the end of the file")

    # Pixels are premultiplied BGRA (no colour value exceeds its alpha, though
    # artFileTool's README says otherwise); converting "RGBa" to "RGBA" undoes it.
    picture = None
    if width > 0 and height > 0:
        picture = Image.frombytes(
            "RGBa", (width, height), data[pixels_start:pixels_end], "raw", "BGRa"
        ).convert("RGBA")

    rectangles = [
        {
            "x": rectangle_numbers[start],
            "y": rectangle_numbers[start + 1],
            "width": rectangle_numbers[start + 2],
            "height": rectangle_numbers[start + 3],
        }
        for start in range(0, 12, 4)
    ]

    description = {
        "width": width,
        "height": height,
        "rows": row_count,
        "columns": column_count,
        "row_heights": row_heights[:row_count],
        "column_widths": column_widths[:column_count],
        "rectangles_meaning_unknown": rectangles,
        "unknown_after_rectangles": unknown_after_rectangles,
        "unknown_at_end": unknown_at_end,
    }
    return description, picture


def decode(art_file_path, output_dir):
    """Writes every image as a PNG plus index.json. Returns the image count."""
    data = art_file_path.read_bytes()

    (
        image_count,
        bits_per_channel,
        tag_count,
        tag_descriptors_offset,
        tag_names_offset,
        image_descriptors_offset,
        image_records_offset,
    ) = MASTER_HEADER.unpack_from(data, 0)

    if bits_per_channel != EXPECTED_BITS_PER_CHANNEL:
        fail(
            f"{art_file_path.name} says {bits_per_channel} bits per channel; "
            f"only {EXPECTED_BITS_PER_CHANNEL} is understood. Is this an ArtFile?"
        )

    tags = read_tags(data, tag_count, tag_descriptors_offset, tag_names_offset)

    output_dir.mkdir(parents=True, exist_ok=True)
    index = []

    for position in range(image_count):
        descriptor = IMAGE_DESCRIPTOR.unpack_from(
            data, image_descriptors_offset + position * IMAGE_DESCRIPTOR.size
        )
        record_offset = image_records_offset + descriptor[0]
        tag_names = [tags[number] for number in descriptor[1:] if number != 0]

        description, picture = read_image(data, record_offset)

        # Several images share a set of tags, and the system looks them up
        # by order, so the position is part of the name.
        file_name = f"{position:04d}-{'-'.join(tag_names)}.png"
        if picture is None:
            file_name = None
        else:
            picture.save(output_dir / file_name)

        index.append({"index": position, "tags": tag_names, "file": file_name, **description})

    with open(output_dir / "index.json", "w") as index_file:
        json.dump(
            {"source": art_file_path.name, "tags": sorted(tags.values()), "images": index},
            index_file,
            indent=1,
        )

    return image_count


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("art_file", type=Path, help="ArtFile.bin or ArtFile200.bin")
    parser.add_argument("output", type=Path, help="directory for the PNGs and index.json")
    arguments = parser.parse_args()

    if not arguments.art_file.is_file():
        fail(f"{arguments.art_file} does not exist")

    image_count = decode(arguments.art_file, arguments.output)
    print(f"{arguments.art_file.name}: {image_count} images written to {arguments.output}")


if __name__ == "__main__":
    main()
