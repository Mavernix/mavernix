"""Writes data read with tomllib back as TOML text.

Python reads TOML but cannot write it. The session writes only what it has
read itself (a theme laid over its base), so strings, numbers, booleans,
lists and tables are all it needs to handle.
"""

import json
import re

BARE_KEY = re.compile(r"^[A-Za-z0-9_-]+$")


def key_text(key):
    return key if BARE_KEY.match(key) else json.dumps(key, ensure_ascii=False)


def value_text(value):
    """One value, written on one line; a table inside a list is written inline."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, str):
        # JSON's escapes are all valid in a TOML basic string.
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, list):
        return "[" + ", ".join(value_text(item) for item in value) + "]"
    if isinstance(value, dict):
        pairs = ", ".join(f"{key_text(key)} = {value_text(item)}" for key, item in value.items())
        return "{ " + pairs + " }" if pairs else "{}"
    raise TypeError(f"toml_text: cannot write a {type(value).__name__}")


def table_lines(table, path):
    """A table's own keys first, then each table inside it under its own header:
    TOML puts every key after a header into that header's table."""
    lines = []
    for key, value in table.items():
        if not isinstance(value, dict):
            lines.append(f"{key_text(key)} = {value_text(value)}")
    for key, value in table.items():
        if isinstance(value, dict):
            inner_path = path + [key_text(key)]
            lines.append("")
            lines.append("[" + ".".join(inner_path) + "]")
            lines += table_lines(value, inner_path)
    return lines


def toml_text(data):
    return "\n".join(table_lines(data, [])).lstrip("\n") + "\n"
