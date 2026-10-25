"""Decodes CoreUI's SArtFile.bin, the installer's second artwork file.

Usage:

    python sartfile.py ROOT/System/Library/PrivateFrameworks/CoreUI.framework/Versions/A/Resources/SArtFile.bin \\
        WORK/artwork/sartfile

Writes the pictures and an index.json. SArtFile.bin holds numbered,
unnamed pictures (window frame pieces, textures, glyphs). The layout is the
10.8 one Alex Zielenski described for sartFileTool
(https://github.com/alexzielenski/sartFileTool); all numbers are little-endian.

    Header               version (u16, always 2), entry count (u16),
                         offset where picture data begins (u32)
    Entry offsets        one u32 per entry
    Unknown area         4 bytes for each entry that has two sizes; not understood
    Entries              type (u16), picture count (u16), then one
                         12-byte picture header per picture
    Picture data

An entry holds the same picture in one or more forms:

    type 1   two bitmaps: normal size, then double size (Retina)
    type 2   one bitmap: normal size only
    type 3   a PDF (vector), then the same drawing as two bitmaps
"""

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image

HEADER = struct.Struct("<HHI")
ENTRY_HEADER = struct.Struct("<HH")

# width (u16), height (u16), data length in bytes (u32), data offset from the
# start of the picture data area (u32).
PICTURE_HEADER = struct.Struct("<HHII")

EXPECTED_VERSION = 2
BYTES_PER_PIXEL = 4

TYPE_TWO_SIZES = 1
TYPE_ONE_SIZE = 2
TYPE_PDF = 3
TYPE_NAMES = {TYPE_TWO_SIZES: "bitmap, two sizes", TYPE_ONE_SIZE: "bitmap, one size", TYPE_PDF: "pdf"}


def fail(message):
    sys.exit(f"sartfile: {message}")


def bitmap_from(pixel_bytes, width, height):
    """Turns raw pixels into a picture: premultiplied BGRA, as in ArtFile.bin."""
    return Image.frombytes("RGBa", (width, height), pixel_bytes, "raw", "BGRa").convert("RGBA")


def suffix_for(entry_type, picture_position, bitmap_count):
    """Names a picture within its entry: '1x', '2x', or 'pdf'."""
    if entry_type == TYPE_PDF and picture_position == 0:
        return "pdf"
    # With more than one bitmap, the last is the double-size one.
    bitmap_position = picture_position - (1 if entry_type == TYPE_PDF else 0)
    return "2x" if bitmap_count > 1 and bitmap_position == bitmap_count - 1 else "1x"


def decode(sart_file_path, output_dir):
    """Writes every picture plus index.json. Returns the number of entries."""
    data = sart_file_path.read_bytes()

    version, entry_count, picture_data_offset = HEADER.unpack_from(data, 0)
    if version != EXPECTED_VERSION:
        fail(f"{sart_file_path.name} has version {version}; only {EXPECTED_VERSION} is understood")

    entry_offsets = struct.unpack_from(f"<{entry_count}I", data, HEADER.size)

    output_dir.mkdir(parents=True, exist_ok=True)
    index = []

    for entry_number, entry_offset in enumerate(entry_offsets):
        entry_type, picture_count = ENTRY_HEADER.unpack_from(data, entry_offset)
        if entry_type not in TYPE_NAMES:
            fail(f"entry {entry_number} has unknown type {entry_type}")

        bitmap_count = picture_count - (1 if entry_type == TYPE_PDF else 0)
        pictures = []

        for picture_position in range(picture_count):
            width, height, length, offset = PICTURE_HEADER.unpack_from(
                data, entry_offset + ENTRY_HEADER.size + picture_position * PICTURE_HEADER.size
            )
            start = picture_data_offset + offset
            picture_bytes = data[start:start + length]

            suffix = suffix_for(entry_type, picture_position, bitmap_count)
            if suffix == "pdf":
                file_name = f"{entry_number:03d}.pdf"
                (output_dir / file_name).write_bytes(picture_bytes)
            else:
                if length != width * height * BYTES_PER_PIXEL:
                    fail(
                        f"entry {entry_number}: {length} bytes do not make a "
                        f"{width}x{height} bitmap"
                    )
                file_name = f"{entry_number:03d}-{suffix}.png"
                bitmap_from(picture_bytes, width, height).save(output_dir / file_name)

            # For a PDF, width and height are its drawing size in points.
            pictures.append({"file": file_name, "width": width, "height": height})

        index.append({"index": entry_number, "type": TYPE_NAMES[entry_type], "pictures": pictures})

    with open(output_dir / "index.json", "w") as index_file:
        json.dump({"source": sart_file_path.name, "entries": index}, index_file, indent=1)

    return entry_count


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sart_file", type=Path, help="SArtFile.bin")
    parser.add_argument("output", type=Path, help="directory for the pictures and index.json")
    arguments = parser.parse_args()

    if not arguments.sart_file.is_file():
        fail(f"{arguments.sart_file} does not exist")

    entry_count = decode(arguments.sart_file, arguments.output)
    print(f"{arguments.sart_file.name}: {entry_count} entries written to {arguments.output}")


if __name__ == "__main__":
    main()
