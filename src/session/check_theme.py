"""Checks a theme against the theme format, as docs/themes/ describes it.

    python check_theme.py THEME_DIR [KEYS.json]
    python check_theme.py --collect DOCS_DIR KEYS.json

The documents are the format: their TOML examples name every key a theme
may have. `--collect` gathers those keys once, when the desktop is built;
the check then reports every key of a theme the format does not have, which
is almost always a misspelling (`backgrund`) or a key of an older format.
A theme with `based_on` is checked with its base laid under it.

An example that begins with a table's header describes that table; one
without a header only shows what a value may look like, and is passed over.
A value written inline (`highlight = { image = "…" }`) is a fill, a font or
a shape, whose own keys depend on its kind: its name is checked, not its
inside. A table whose header line says `# any names` takes keys of any
name ([menu.key_symbols]); one that says `# any key of [TABLE]` takes that
table's keys besides its own ([dock.menu]).
"""

import json
import re
import sys
import tomllib
from pathlib import Path

HEADER = re.compile(r"^\s*\[([^\[\]]+)\]\s*(#.*)?$", re.M)
TOML_BLOCK = re.compile(r"```toml\n(.*?)```", re.S)
ANY_NAMES = "any names"
KEYS_OF = re.compile(r"any key of \[([^\]]+)\]")


def fail(message):
    sys.exit(f"check_theme: {message}")


def block_keys(block):
    """What one example describes: every key as a dotted path, the tables
    among them (as opposed to values written inline), and the tables that
    take any names."""
    headers = {match.group(1).strip(): match.group(2) or "" for match in HEADER.finditer(block)}
    keys, tables, open_tables, borrowed = set(), set(), set(), {}

    def walk(table, path):
        for name, value in table.items():
            dotted = f"{path}.{name}" if path else name
            keys.add(dotted)
            if not isinstance(value, dict):
                continue
            # A table has a header of its own, or one of a table inside it.
            is_table = dotted in headers or any(header.startswith(dotted + ".") for header in headers)
            if not is_table:
                continue
            tables.add(dotted)
            lender = KEYS_OF.search(headers.get(dotted, ""))
            if lender:
                borrowed[dotted] = lender.group(1)
            if ANY_NAMES in headers.get(dotted, ""):
                open_tables.add(dotted)
            else:
                walk(value, dotted)

    walk(tomllib.loads(block), "")
    return keys, tables, open_tables, borrowed


def collect(docs_dir):
    keys, tables, open_tables, borrowed = set(), set(), set(), {}
    for document in sorted(Path(docs_dir).glob("*.md")):
        for block in TOML_BLOCK.findall(document.read_text()):
            first_line = next((line for line in block.splitlines()
                               if line.strip() and not line.lstrip().startswith("#")), "")
            if not first_line.lstrip().startswith("["):
                continue
            try:
                found, found_tables, found_open, found_borrowed = block_keys(block)
            except tomllib.TOMLDecodeError as error:
                fail(f"an example in {document} is not valid TOML: {error}")
            keys |= found
            tables |= found_tables
            open_tables |= found_open
            borrowed |= found_borrowed
    return {"keys": sorted(keys), "tables": sorted(tables), "any_names": sorted(open_tables),
            "keys_of": borrowed}


def theme_keys(table, path=""):
    for name, value in table.items():
        dotted = f"{path}.{name}" if path else name
        yield dotted
        if isinstance(value, dict):
            yield from theme_keys(value, dotted)


def nearest_known(dotted, known):
    """The longest path among `known` that `dotted` lies inside, or ""."""
    while "." in dotted:
        dotted = dotted.rpartition(".")[0]
        if dotted in known:
            return dotted
    return ""


def unknown_keys(theme, format_keys):
    """The theme's keys the format does not have. Inside a value written
    inline, or in a table of any names, nothing is reported."""
    known = set(format_keys["keys"])
    tables = set(format_keys["tables"]) - set(format_keys["any_names"])
    unknown = []
    borrowed = format_keys["keys_of"]
    for dotted in theme_keys(theme):
        if dotted in known or any(dotted.startswith(reported + ".") for reported in unknown):
            continue
        lender = next((table for table in borrowed if dotted.startswith(table + ".")), None)
        if lender is not None and borrowed[lender] + dotted[len(lender):] in known:
            continue
        around = nearest_known(dotted, known)
        if around == "" or around in tables:
            unknown.append(dotted)
    return unknown


def read_theme_with_base(theme_dir, format_keys):
    """The theme in `theme_dir`, with the theme it is based on laid under it."""
    from themes import find_theme, joined

    theme = tomllib.loads((theme_dir / "theme.toml").read_text())
    base = theme.pop("based_on", None)
    if base is None:
        return theme
    base_theme = tomllib.loads((find_theme(base) / "theme.toml").read_text())
    return joined(base_theme, theme, set(format_keys["tables"]))


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--collect":
        Path(sys.argv[3]).write_text(json.dumps(collect(sys.argv[2]), indent=1) + "\n")
        return
    if len(sys.argv) not in (2, 3):
        fail("usage: check_theme.py THEME_DIR [KEYS.json] | --collect DOCS_DIR KEYS.json")

    # Imported here: --collect runs while building, before paths.py exists.
    from paths import DATA_DIR

    keys_file = Path(sys.argv[2]) if len(sys.argv) == 3 else DATA_DIR / "theme-keys.json"
    format_keys = json.loads(keys_file.read_text())
    theme_dir = Path(sys.argv[1])
    if not (theme_dir / "theme.toml").is_file():
        fail(f"{theme_dir} has no theme.toml; give the theme's folder")
    theme = read_theme_with_base(theme_dir, format_keys)
    unknown = unknown_keys(theme, format_keys)
    for dotted in unknown:
        print(f"{theme_dir.name}: {dotted} is not a key of the theme format (docs/themes/)")
    sys.exit(1 if unknown else 0)


if __name__ == "__main__":
    main()
