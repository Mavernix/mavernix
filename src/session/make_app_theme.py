"""Writes themes for application toolkits from a desktop theme.

    python make_app_theme.py THEME_DIRECTORY OUTPUT_DIRECTORY

Translates the theme's [controls] table into a Kvantum theme for Qt and a
gtk.css for GTK 3 and 4, plus KDE and GTK settings, written to:

    OUTPUT/share/themes/DesktopTheme/Kvantum/DesktopTheme.svg, .kvconfig
    OUTPUT/share/themes/DesktopTheme/gtk-3.0/gtk.css
    OUTPUT/share/themes/DesktopTheme/gtk-4.0/gtk.css
    OUTPUT/xdg/Kvantum/kvantum.kvconfig      (names the Kvantum theme)
    OUTPUT/xdg/gtk-3.0/settings.ini          (names the icon theme, if any)
    OUTPUT/xdg/kdeglobals                    (the same, and the colours, for KDE's programs)
    OUTPUT/share/color-schemes/DesktopTheme.colors   (the colours as a KDE colour scheme)

Each starts from a stock theme (Kvantum's `kvantum_base`, GTK's Adwaita) and
replaces only what the desktop theme describes.
"""

import argparse
import base64
import configparser
import io
import re
import shutil
import sys
import tomllib
import xml.etree.ElementTree as ElementTree
from pathlib import Path

from themes import usable_icon_theme

try:
    from PIL import Image
except ImportError:
    sys.exit("make_app_theme: Python's Pillow is needed to cut and embed the theme's pictures. "
             "On Arch: sudo pacman -S --needed python-pillow")

# Fixed, so the session's environment never has to change.
APP_THEME_NAME = "DesktopTheme"

KVANTUM_STOCK_DIRECTORY = Path("/usr/share/Kvantum")

SVG_NAMESPACE = "http://www.w3.org/2000/svg"
XLINK_NAMESPACE = "http://www.w3.org/1999/xlink"

# A stretching Kvantum element is nine pictures: (name ending, column, row).
KVANTUM_FRAME_PARTS = [
    ("-topleft", 0, 0), ("-top", 1, 0), ("-topright", 2, 0),
    ("-left", 0, 1), ("", 1, 1), ("-right", 2, 1),
    ("-bottomleft", 0, 2), ("-bottom", 1, 2), ("-bottomright", 2, 2),
]

# Our own element names: the stock theme's are shared with other controls
# that keep the stock look.
KVANTUM_TOOL_BAR_ELEMENT = "desktoptoolbar"
KVANTUM_TEXT_FIELD_ELEMENT = "desktopfield"
KVANTUM_POPUP_ELEMENT = "desktoppopup"
KVANTUM_TAB_ELEMENT = "desktoptab"
KVANTUM_TOOL_BUTTON_ELEMENT = "desktoptoolbutton"

# Kvantum's button states -> the theme's [controls.button] picture. Kvantum
# has no state for the default button.
KVANTUM_BUTTON_STATES = {
    "normal": "normal",
    "focused": "normal",      # pointer over the button: no change
    "pressed": "pressed",
    "toggled": "pressed",     # a button that stays down
    "disabled": "disabled",
}

# The same for checkboxes and radio buttons.
KVANTUM_INDICATOR_STATES = {
    "normal": "normal",
    "focused": "normal",
    "pressed": "pressed",
    "disabled": "disabled",
}


def fail(message):
    sys.exit(f"make_app_theme: {message}")


def double_density(path):
    """The "@2x" version of a picture if the theme has one, else the picture."""
    candidate = path.with_name(f"{path.stem}@2x{path.suffix}")
    return candidate if candidate.is_file() else path


def picture_as_data(image):
    """A picture as the text an SVG file can carry inside itself."""
    buffer = io.BytesIO()
    image.save(buffer, format="PNG")
    return "data:image/png;base64," + base64.b64encode(buffer.getvalue()).decode("ascii")


# --- Kvantum -------------------------------------------------------------------

class KvantumPicture:
    """Kvantum's SVG file: a flat collection of pictures found by their names."""

    def __init__(self, path):
        ElementTree.register_namespace("", SVG_NAMESPACE)
        ElementTree.register_namespace("xlink", XLINK_NAMESPACE)
        self.tree = ElementTree.parse(path)
        self.root = self.tree.getroot()
        # New pictures go in a row below the stock ones.
        self.next_x = 0
        self.new_pictures_y = 5000

    def remove(self, name_pattern):
        """Removes every element whose name matches the pattern, wherever it is."""
        wanted = re.compile(name_pattern)
        for parent in list(self.root.iter()):
            for child in list(parent):
                if wanted.fullmatch(child.get("id", "")):
                    parent.remove(child)

    def add(self, name, image, logical_width, logical_height):
        """Adds a picture under a name, shown at the given size."""
        element = ElementTree.SubElement(self.root, f"{{{SVG_NAMESPACE}}}image")
        element.set("id", name)
        element.set("x", str(self.next_x))
        element.set("y", str(self.new_pictures_y))
        element.set("width", str(logical_width))
        element.set("height", str(logical_height))
        element.set("preserveAspectRatio", "none")
        element.set(f"{{{XLINK_NAMESPACE}}}href", picture_as_data(image))
        self.next_x += logical_width + 4

    def add_nine_parts(self, name, picture_path, slice_edges):
        """Adds a stretching element cut from one picture.

        `slice_edges` is top, right, bottom, left in logical pixels: the
        strips that keep their size.
        """
        image = Image.open(double_density(picture_path)).convert("RGBA")
        logical = Image.open(picture_path).size
        self.add_nine_parts_of(name, image, logical, slice_edges)

    def add_nine_parts_of(self, name, image, logical, slice_edges):
        """The same from an image in memory, shown at the size `logical`."""
        top, right, bottom, left = slice_edges
        density = image.size[0] // logical[0]

        column_edges = [0, left, logical[0] - right, logical[0]]
        row_edges = [0, top, logical[1] - bottom, logical[1]]
        for ending, column, row in KVANTUM_FRAME_PARTS:
            box = (column_edges[column], row_edges[row], column_edges[column + 1], row_edges[row + 1])
            if box[2] == box[0] or box[3] == box[1]:
                continue
            part = image.crop(tuple(edge * density for edge in box))
            self.add(name + ending, part, box[2] - box[0], box[3] - box[1])

    def add_whole(self, name, picture_path):
        """Adds a fixed-size element: one picture, shown at its own size."""
        image = Image.open(double_density(picture_path)).convert("RGBA")
        logical = Image.open(picture_path).size
        self.add(name, image, logical[0], logical[1])

    def write(self, path):
        self.tree.write(path, encoding="utf-8", xml_declaration=True)


# Highlighted items are drawn here from the theme's colours, then stretched
# with one-pixel edges kept.
ITEM_PICTURE_WIDTH = 4
ITEM_PICTURE_HEIGHT = 24
ITEM_PICTURE_EDGES = [1, 1, 1, 1]   # top, right, bottom, left


def color_tuple(text):
    """ "#rrggbb" or "#rrggbbaa" as (red, green, blue, alpha)."""
    digits = text.lstrip("#")
    if len(digits) == 6:
        digits += "ff"
    return tuple(int(digits[index:index + 2], 16) for index in (0, 2, 4, 6))


BAR_PICTURE_WIDTH = 4
BAR_PICTURE_HEIGHT = 32


def filled_picture(width, height, fill, top_line=None, bottom_line=None):
    """Draws a fill, optionally with a one-pixel line along top and bottom.

    `fill` is one colour, or two for a run from the top to the bottom.
    """
    colors = [color_tuple(color) for color in fill]
    first, last = colors[0], colors[-1]
    image = Image.new("RGBA", (width, height))
    for row in range(height):
        share = row / (height - 1)
        color = tuple(round(first[part] + (last[part] - first[part]) * share) for part in range(4))
        if row == 0 and top_line is not None:
            color = color_tuple(top_line)
        if row == height - 1 and bottom_line is not None:
            color = color_tuple(bottom_line)
        for column in range(width):
            image.putpixel((column, row), color)
    return image


def item_picture(fill, top_line=None, bottom_line=None):
    return filled_picture(ITEM_PICTURE_WIDTH, ITEM_PICTURE_HEIGHT, fill, top_line, bottom_line)


def read_kvantum_settings(path):
    """Kvantum's settings file: sections of key=value lines, case kept."""
    settings = configparser.RawConfigParser(strict=False)
    settings.optionxform = str  # keep the keys' capital letters
    settings.read(path)
    return settings


def use_stretching_picture(section, element, slice_edges):
    """Makes a section of Kvantum's settings draw a stretching element.

    `slice_edges` is top, right, bottom, left: the strips that keep their
    size, as given to KvantumPicture.add_nine_parts for the same element.
    """
    top, right, bottom, left = slice_edges
    section["frame"] = "true"
    section["frame.element"] = element
    section["interior.element"] = element
    section["frame.top"] = str(top)
    section["frame.right"] = str(right)
    section["frame.bottom"] = str(bottom)
    section["frame.left"] = str(left)
    # Our frames have fixed edges; stock themes may let them grow.
    section["frame.expansion"] = "0"


def write_kvantum_theme(theme, theme_directory, output_directory):
    """Writes the theme for Qt applications; returns whether it could."""
    controls = theme["controls"]
    base_name = controls.get("kvantum_base")
    if base_name is None:
        fail("the theme's [controls] table needs kvantum_base, the stock Kvantum theme to start from")
    if not KVANTUM_STOCK_DIRECTORY.is_dir():
        # Skip Qt alone; GTK can still be themed.
        print(f"make_app_theme: Kvantum is not installed ({KVANTUM_STOCK_DIRECTORY} is missing), "
              "so Qt applications keep their own look. To theme them, install Kvantum "
              "(on Arch: sudo pacman -S --needed kvantum).", file=sys.stderr)
        return False
    base_directory = KVANTUM_STOCK_DIRECTORY / base_name
    if not (base_directory / f"{base_name}.svg").is_file():
        fail(f"Kvantum has no stock theme {base_name} in {KVANTUM_STOCK_DIRECTORY}; "
             "check kvantum_base in the theme's [controls] table")

    picture = KvantumPicture(base_directory / f"{base_name}.svg")
    settings = read_kvantum_settings(base_directory / f"{base_name}.kvconfig")

    # Translucent, blurred windows are not inherited from the stock theme.
    general = settings["%General"]
    general["translucent_windows"] = "false"
    general["blurring"] = "false"
    general["popup_blurring"] = "false"

    colors = settings["GeneralColors"]
    if "window_color" in controls:
        settings["Window"]["interior"] = "false"
        colors["window.color"] = controls["window_color"]
        colors["inactive.window.color"] = controls["window_color"]
        colors["button.color"] = controls["window_color"]
    if "text_color" in controls:
        # Inactive colours too: left out, Kvantum defaults them to dark.
        for key in ("text.color", "window.text.color", "button.text.color",
                    "inactive.text.color", "inactive.window.text.color"):
            colors[key] = controls["text_color"]
    if "base_color" in controls:
        colors["base.color"] = controls["base_color"]
        colors["inactive.base.color"] = controls["base_color"]
        # Stock themes may make file managers' views see-through.
        for hack in ("transparent_dolphin_view", "transparent_pcmanfm_view",
                     "transparent_pcmanfm_sidepane"):
            settings["Hacks"][hack] = "false"
    if "highlight_color" in controls:
        colors["highlight.color"] = controls["highlight_color"]
        colors["inactive.highlight.color"] = controls["highlight_color"]
    if "highlight_text_color" in controls:
        colors["highlight.text.color"] = controls["highlight_text_color"]

    button = controls.get("button")
    if button is not None:
        picture.remove(r"button-(normal|focused|pressed|toggled|disabled)(-.*)?")
        for kvantum_state, our_state in KVANTUM_BUTTON_STATES.items():
            picture.add_nine_parts(f"button-{kvantum_state}", theme_directory / button[our_state], button["slice"])

        top, right, bottom, left = button["slice"]
        section = settings["PanelButtonCommand"]
        section["frame"] = "true"
        section["frame.element"] = "button"
        section["interior.element"] = "button"
        section["frame.top"] = str(top)
        section["frame.right"] = str(right)
        section["frame.bottom"] = str(bottom)
        section["frame.left"] = str(left)
        # The pictures' fixed strips already are the space around the text.
        for side in ("top", "bottom", "left", "right"):
            section[f"text.margin.{side}"] = "0"
        for state in ("normal", "focus", "press", "toggle"):
            section[f"text.{state}.color"] = controls.get("text_color", "#000000")
        section["frame.expansion"] = "0"

        # Kvantum marks the default button with bold text (the hack turns it
        # off) and a corner triangle (replaced by an empty picture).
        settings["Hacks"]["normal_default_pushbutton"] = "true"
        picture.remove("button-default-indicator")
        picture.add("button-default-indicator", Image.new("RGBA", (1, 1), (0, 0, 0, 0)), 1, 1)

        # Tool buttons inherit push buttons' pictures, which are wrong for
        # them; make them plain unless the theme brings their own (below).
        tool_button = settings["PanelButtonTool"]
        tool_button["frame"] = "false"
        tool_button["interior"] = "false"

    tool_button_pictures = controls.get("tool_button")
    if tool_button_pictures is not None:
        for kvantum_state, our_state in KVANTUM_BUTTON_STATES.items():
            file_name = tool_button_pictures.get(our_state, tool_button_pictures["normal"])
            picture.add_nine_parts(f"{KVANTUM_TOOL_BUTTON_ELEMENT}-{kvantum_state}",
                                   theme_directory / file_name, tool_button_pictures["slice"])
        # Buttons inside a tool bar use "ToolbarButton", others
        # "PanelButtonTool"; stock themes give the first its own picture.
        for section_name in ("PanelButtonTool", "ToolbarButton"):
            section = settings[section_name]
            use_stretching_picture(section, KVANTUM_TOOL_BUTTON_ELEMENT, tool_button_pictures["slice"])
            section.pop("frame.expandedElement", None)
            section.pop("interior.expandedElement", None)
            section["interior"] = "true"
            for side in ("top", "bottom", "left", "right"):
                section[f"text.margin.{side}"] = "0"
            for state in ("normal", "focus", "press", "toggle"):
                section[f"text.{state}.color"] = controls.get("text_color", "#000000")

    for kind, kvantum_name in (("checkbox", "checkbox"), ("radio", "radio")):
        indicator = controls.get(kind)
        if indicator is None:
            continue
        picture.remove(rf"{kvantum_name}-.*")
        # Kvantum: "normal" is unchecked, "checked-normal" etc. the others.
        for our_value, kvantum_value in (("off", ""), ("on", "checked-"), ("mixed", "tristate-")):
            states = indicator.get(our_value)
            if states is None:
                continue
            for kvantum_state, our_state in KVANTUM_INDICATOR_STATES.items():
                file_name = states.get(our_state, states["normal"])
                picture.add_whole(f"{kvantum_name}-{kvantum_value}{kvantum_state}", theme_directory / file_name)

    text_field = controls.get("text_field")
    if text_field is not None:
        # "focused" is the field being typed in; it looks as usual.
        states = {"normal": "normal", "focused": "normal", "disabled": "disabled"}
        for kvantum_state, our_state in states.items():
            file_name = text_field.get(our_state, text_field["normal"])
            picture.add_nine_parts(f"{KVANTUM_TEXT_FIELD_ELEMENT}-{kvantum_state}",
                                   theme_directory / file_name, text_field["slice"])
        section = settings["LineEdit"]
        use_stretching_picture(section, KVANTUM_TEXT_FIELD_ELEMENT, text_field["slice"])
        # Stock themes may switch pictures for large fields.
        section.pop("frame.expandedElement", None)
        section.pop("interior.expandedElement", None)

    tabs = controls.get("tabs")
    if tabs is not None:
        # Kvantum's states of a tab: "toggled" is the selected one.
        states = {"normal": "normal", "focused": "normal", "pressed": "pressed",
                  "toggled": "selected", "disabled": "disabled"}
        for kvantum_state, our_state in states.items():
            file_name = tabs.get(our_state, tabs["normal"])
            picture.add_nine_parts(f"{KVANTUM_TAB_ELEMENT}-{kvantum_state}",
                                   theme_directory / file_name, tabs["slice"])
        section = settings["Tab"]
        use_stretching_picture(section, KVANTUM_TAB_ELEMENT, tabs["slice"])
        for side in ("top", "bottom"):
            section[f"text.margin.{side}"] = "0"
        text_color = controls.get("text_color", "#000000")
        for state in ("normal", "focus", "press"):
            section[f"text.{state}.color"] = text_color
        section["text.toggle.color"] = tabs.get("selected_text_color", text_color)
        section["text.bold"] = "false"

        general = settings["%General"]
        general["joined_inactive_tabs"] = str(tabs.get("joined", False)).lower()
        general["center_normal_tabs"] = str(tabs.get("centered", False)).lower()
        general["embedded_tabs"] = str(tabs.get("on_pane_edge", False)).lower()
        # The selected tab is a picture like the others, not a piece of the pane.
        general["attach_active_tab"] = "false"

        if "pane" in tabs:
            picture.add_nine_parts(f"{KVANTUM_TAB_ELEMENT}pane-normal",
                                   theme_directory / tabs["pane"], tabs["pane_slice"])
            use_stretching_picture(settings["TabFrame"], f"{KVANTUM_TAB_ELEMENT}pane", tabs["pane_slice"])
            settings["TabFrame"]["interior"] = "true"

    popup_button = controls.get("popup_button")
    if popup_button is not None:
        states = {"normal": "normal", "focused": "normal", "pressed": "pressed",
                  "toggled": "pressed", "disabled": "disabled"}
        for kvantum_state, our_state in states.items():
            file_name = popup_button.get(our_state, popup_button["normal"])
            picture.add_nine_parts(f"{KVANTUM_POPUP_ELEMENT}-{kvantum_state}",
                                   theme_directory / file_name, popup_button["slice"])
        section = settings["ComboBox"]
        use_stretching_picture(section, KVANTUM_POPUP_ELEMENT, popup_button["slice"])
        section.pop("frame.expandedElement", None)
        section.pop("interior.expandedElement", None)
        # The fixed strips are the text margin unless text_inset says otherwise.
        for side in ("top", "bottom", "left", "right"):
            section[f"text.margin.{side}"] = "0"
        left_strip = popup_button["slice"][3]
        if "text_inset" in popup_button:
            section["text.margin.left"] = str(max(0, popup_button["text_inset"] - left_strip))
        for state in ("normal", "focus", "press", "toggle"):
            section[f"text.{state}.color"] = controls.get("text_color", "#000000")

        if popup_button.get("includes_arrows", False):
            # The picture has the arrows; hide Kvantum's with an empty picture
            # (naming a missing one makes Kvantum draw its stock arrow).
            nothing = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
            for state in ("normal", "focused", "pressed", "toggled", "disabled"):
                picture.add(f"{KVANTUM_POPUP_ELEMENT}nomark-{state}", nothing, 1, 1)
            section["indicator.element"] = f"{KVANTUM_POPUP_ELEMENT}nomark"
            section["indicator.size"] = "1"

    progress_bar = controls.get("progress_bar")
    if progress_bar is not None:
        picture.remove(r"progress-(normal|pattern-.*)(-.*)?")
        track_file = theme_directory / progress_bar["track"]
        picture.add_nine_parts("progress-normal", track_file, progress_bar["track_slice"])
        use_stretching_picture(settings["Progressbar"], "progress", progress_bar["track_slice"])
        for state in ("normal", "disabled", "normal-inactive"):
            picture.add_whole(f"progress-pattern-{state}", theme_directory / progress_bar["fill"])
        contents = settings["ProgressbarContents"]
        contents["frame"] = "false"
        contents["interior.element"] = "progress-pattern"
        general = settings["%General"]
        # Bar thickness from the track picture; fill inside its edge strips.
        general["progressbar_thickness"] = str(Image.open(track_file).size[1])
        general["spread_progressbar"] = "false"
        # Kvantum always writes the percentage on a bar whose program asks
        # for it; `shows_percentage` can only be honoured for GTK.

    slider = controls.get("slider")
    if slider is not None:
        picture.remove(r"slider-(normal|toggled|disabled)(-.*)?")
        picture.remove(r"slidercursor-.*")
        track_file = theme_directory / slider["track"]
        # "toggled" is the part of the groove before the knob; the theme
        # has one look for the whole groove.
        for state in ("normal", "toggled", "disabled"):
            picture.add_nine_parts(f"slider-{state}", track_file, slider["track_slice"])
        use_stretching_picture(settings["Slider"], "slider", slider["track_slice"])
        thumb_states = {"normal": "thumb", "focused": "thumb", "pressed": "thumb_pressed",
                        "disabled": "thumb_disabled"}
        for kvantum_state, our_key in thumb_states.items():
            picture.add_whole(f"slidercursor-{kvantum_state}",
                              theme_directory / slider.get(our_key, slider["thumb"]))
        thumb_size = Image.open(theme_directory / slider["thumb"]).size
        general = settings["%General"]
        general["slider_width"] = str(Image.open(track_file).size[1])
        general["slider_handle_width"] = str(thumb_size[0])
        general["slider_handle_length"] = str(thumb_size[1])

    tool_bar = controls.get("tool_bar")
    if tool_bar is not None:
        # Own name: the stock menu bar shares the tool bar's picture.
        bar = filled_picture(BAR_PICTURE_WIDTH, BAR_PICTURE_HEIGHT, tool_bar["fill"],
                             tool_bar.get("top_line"), tool_bar.get("bottom_line"))
        top = 1 if "top_line" in tool_bar else 0
        bottom = 1 if "bottom_line" in tool_bar else 0
        picture.add_nine_parts_of(f"{KVANTUM_TOOL_BAR_ELEMENT}-normal", bar,
                                  (BAR_PICTURE_WIDTH, BAR_PICTURE_HEIGHT), [top, 0, bottom, 0])
        # Kvantum uses "NAME-inactive" in inactive windows.
        if "fill_inactive" in tool_bar:
            inactive_bar = filled_picture(BAR_PICTURE_WIDTH, BAR_PICTURE_HEIGHT, tool_bar["fill_inactive"],
                                          tool_bar.get("top_line_inactive", tool_bar.get("top_line")),
                                          tool_bar.get("bottom_line_inactive", tool_bar.get("bottom_line")))
            picture.add_nine_parts_of(f"{KVANTUM_TOOL_BAR_ELEMENT}-normal-inactive", inactive_bar,
                                      (BAR_PICTURE_WIDTH, BAR_PICTURE_HEIGHT), [top, 0, bottom, 0])
        section = settings["Toolbar"]
        section["interior.element"] = KVANTUM_TOOL_BAR_ELEMENT
        section["frame.element"] = KVANTUM_TOOL_BAR_ELEMENT
        section["frame"] = "true"
        section["frame.top"] = str(top)
        section["frame.bottom"] = str(bottom)
        section["frame.left"] = "0"
        section["frame.right"] = "0"
        # A stock theme with a dark bar has light text and icons on it.
        if "text_color" in controls:
            for state in ("normal", "focus", "press", "toggle"):
                section[f"text.{state}.color"] = controls["text_color"]
        # A menu bar inside the window is not part of the tool bar's fill.
        settings["%General"]["merge_menubar_with_toolbar"] = "false"

    selected_item = controls.get("selected_item")
    if selected_item is not None:
        # "toggled" is selected, "pressed" clicked, "focused" hovered.
        picture.remove(r"itemview-(pressed|toggled|focused)(-.*)?")
        selected = item_picture(selected_item["fill"], selected_item.get("top_line"),
                                selected_item.get("bottom_line"))
        logical = (ITEM_PICTURE_WIDTH, ITEM_PICTURE_HEIGHT)
        for state in ("toggled", "pressed"):
            picture.add_nine_parts_of(f"itemview-{state}", selected, logical, ITEM_PICTURE_EDGES)
        if "hover_fill" in selected_item:
            # Without this key an item under the pointer is not marked.
            hovered = item_picture(selected_item["hover_fill"])
            picture.add_nine_parts_of("itemview-focused", hovered, logical, ITEM_PICTURE_EDGES)

        section = settings["ItemView"]
        top, right, bottom, left = ITEM_PICTURE_EDGES
        section["frame"] = "true"
        section["frame.top"] = str(top)
        section["frame.right"] = str(right)
        section["frame.bottom"] = str(bottom)
        section["frame.left"] = str(left)
        section["frame.expansion"] = "0"
        if "text_color" in selected_item:
            for state in ("toggle", "press", "toggle.inactive"):
                section[f"text.{state}.color"] = selected_item["text_color"]

    if "checkbox" in controls:
        # Checkboxes and radio buttons are drawn at this size.
        box = Image.open(theme_directory / controls["checkbox"]["off"]["normal"]).size
        settings["%General"]["check_size"] = str(box[0])

    kvantum_directory = output_directory / "share" / "themes" / APP_THEME_NAME / "Kvantum"
    kvantum_directory.mkdir(parents=True, exist_ok=True)
    picture.write(kvantum_directory / f"{APP_THEME_NAME}.svg")
    with open(kvantum_directory / f"{APP_THEME_NAME}.kvconfig", "w") as file:
        settings.write(file, space_around_delimiters=False)

    # The user's own ~/.config/Kvantum/ choice still wins over this.
    choice_directory = output_directory / "xdg" / "Kvantum"
    choice_directory.mkdir(parents=True, exist_ok=True)
    (choice_directory / "kvantum.kvconfig").write_text(f"[General]\ntheme={APP_THEME_NAME}\n")
    return True


# --- GTK -----------------------------------------------------------------------

# GTK's built-in theme, at different resource paths per major version.
GTK_BASE_IMPORT = {
    "gtk-3.0": '@import url("resource:///org/gtk/libgtk/theme/Adwaita/gtk-contained.css");',
    "gtk-4.0": '@import url("resource:///org/gtk/libgtk/theme/Default/Default-light.css");',
}
# Its dark variant, for themes with [controls] dark.
GTK_DARK_BASE_IMPORT = {
    "gtk-3.0": '@import url("resource:///org/gtk/libgtk/theme/Adwaita/gtk-contained-dark.css");',
    "gtk-4.0": '@import url("resource:///org/gtk/libgtk/theme/Default/Default-dark.css");',
}

# How each toolkit version writes "this picture, or that one on a dense screen".
def gtk_picture(version, file_name):
    stem, dot, extension = file_name.rpartition(".")
    if version == "gtk-3.0":
        return f'-gtk-scaled(url("assets/{file_name}"), url("assets/{stem}@2x.{extension}"))'
    return f'image-set(url("assets/{file_name}") 1x, url("assets/{stem}@2x.{extension}") 2x)'


def middle_name(file_name):
    """The name of the picture that holds another picture's middle part."""
    stem, dot, extension = file_name.rpartition(".")
    return f"{stem}-middle.{extension}"


def cut_middles(assets_directory, middles):
    """Cuts the middle out of stretching pictures, at both densities.

    `middles` is a list of (file name, [top, right, bottom, left]).
    """
    for file_name, (top, right, bottom, left) in middles:
        stem, dot, extension = file_name.rpartition(".")
        for density, suffix in ((1, ""), (2, "@2x")):
            source = assets_directory / f"{stem}{suffix}.{extension}"
            if not source.is_file():
                continue
            image = Image.open(source).convert("RGBA")
            box = (left * density, top * density,
                   image.width - right * density, image.height - bottom * density)
            middle_stem = middle_name(file_name).rpartition(".")[0]
            image.crop(box).save(assets_directory / f"{middle_stem}{suffix}.{extension}")


def gtk_css(theme, theme_directory, version):
    """The CSS for one GTK version; returns (text, middles for cut_middles())."""
    controls = theme["controls"]
    middles = []
    lines = [
        "/* Written by make_app_theme.py from the desktop theme. Do not edit. */",
        (GTK_DARK_BASE_IMPORT if controls.get("dark", False) else GTK_BASE_IMPORT)[version],
        "",
    ]

    if "window_color" in controls:
        lines += [
            f"window, dialog, .background {{ background-color: {controls['window_color']}; }}",
            "",
        ]

    if "highlight_color" in controls:
        selected_text = controls.get("highlight_text_color", "#ffffff")
        lines += [
            f"selection, *:selected {{ background-color: {controls['highlight_color']}; color: {selected_text}; }}",
            "",
        ]

    def stretching_rule(selector, file_name, slice_edges, more=()):
        """A rule that draws a node as a picture with fixed edge strips.

        GTK 3 ignores border-image's "fill", so the middle is cut out as a
        picture of its own and stretched as the background.
        """
        top, right, bottom, left = slice_edges
        edges = f"{top} {right} {bottom} {left}"
        widths = f"{top}px {right}px {bottom}px {left}px"
        name = Path(file_name).name
        middles.append((name, slice_edges))
        return [
            f"{selector} {{",
            f"  background: {gtk_picture(version, middle_name(name))} no-repeat;",
            "  background-size: 100% 100%;",
            "  background-origin: padding-box;",
            "  background-clip: padding-box;",
            "  box-shadow: none;",
            "  border-radius: 0;",
            "  border-style: solid;",
            f"  border-width: {widths};",
            f"  border-image: {gtk_picture(version, name)} {edges} fill / {widths} stretch;",
            *[f"  {line}" for line in more],
            "}",
            "",
        ]

    def picture_size(file_name):
        return Image.open(theme_directory / file_name).size

    text_field = controls.get("text_field")
    if text_field is not None:
        lines += stretching_rule("entry", text_field["normal"], text_field["slice"])
        if "disabled" in text_field:
            lines += stretching_rule("entry:disabled", text_field["disabled"], text_field["slice"])

    # [controls.tabs] is Kvantum-only: one border image per tab left gaps
    # and stray edges in GTK, so GTK keeps its stock tabs.

    popup_button = controls.get("popup_button")
    if popup_button is not None:
        more = ["padding: 0;", f"color: {controls.get('text_color', '#000000')};"]
        if "text_inset" in popup_button:
            left_strip = popup_button["slice"][3]
            more[0] = f"padding: 0 0 0 {max(0, popup_button['text_inset'] - left_strip)}px;"
        # No taller than the text: the arrows in the picture would stretch.
        more.append("min-height: 0;")
        box = "combobox button.combo"
        lines += stretching_rule(box, popup_button["normal"], popup_button["slice"], more)
        if "pressed" in popup_button:
            lines += stretching_rule(f"{box}:active, {box}:checked", popup_button["pressed"],
                                     popup_button["slice"], more)
        if "disabled" in popup_button:
            lines += stretching_rule(f"{box}:disabled", popup_button["disabled"],
                                     popup_button["slice"], more)
        if popup_button.get("includes_arrows", False):
            lines += [f"{box} arrow {{ -gtk-icon-source: none; min-width: 0; min-height: 0; }}", ""]

    progress_bar = controls.get("progress_bar")
    if progress_bar is not None:
        track_height = picture_size(progress_bar["track"])[1]
        fill_width, fill_height = picture_size(progress_bar["fill"])
        top, right, bottom, left = progress_bar["track_slice"]
        # Horizontal bars only: the pictures are not turned for upright ones.
        lines += stretching_rule("progressbar.horizontal trough", progress_bar["track"],
                                 progress_bar["track_slice"],
                                 [f"min-height: {track_height - top - bottom}px;", "padding: 0;"])
        if not progress_bar.get("shows_percentage", True):
            lines += ["progressbar text { opacity: 0; }", ""]
        lines += [
            "progressbar.horizontal progress {",
            f"  background: {gtk_picture(version, Path(progress_bar['fill']).name)} repeat-x;",
            f"  background-size: {fill_width}px {fill_height}px;",
            f"  min-height: {fill_height}px;",
            "  border: none;",
            "  border-radius: 0;",
            "  box-shadow: none;",
            "  margin: 0;",
            "}",
            "",
        ]

    slider = controls.get("slider")
    if slider is not None:
        track_height = picture_size(slider["track"])[1]
        top, right, bottom, left = slider["track_slice"]
        lines += stretching_rule("scale.horizontal trough", slider["track"], slider["track_slice"],
                                 [f"min-height: {track_height - top - bottom}px;", "margin: 0;"])
        # The part of the groove before the knob has no look of its own.
        lines += ["scale.horizontal highlight { background: none; border: none; }", ""]

        def thumb_rule(selector, key):
            width, height = picture_size(slider["thumb"])
            name = Path(slider.get(key, slider["thumb"])).name
            return [
                f"{selector} {{",
                f"  background: {gtk_picture(version, name)} center / {width}px {height}px no-repeat;",
                f"  min-width: {width}px;",
                f"  min-height: {height}px;",
                # Centre the knob on the groove.
                f"  margin: -{(height - track_height) // 2}px;",
                "  border: none;",
                "  box-shadow: none;",
                "}",
                "",
            ]

        lines += thumb_rule("scale slider", "thumb")
        lines += thumb_rule("scale slider:active", "thumb_pressed")
        lines += thumb_rule("scale slider:disabled", "thumb_disabled")

    tool_bar = controls.get("tool_bar")
    if tool_bar is not None:
        fill = tool_bar["fill"]
        lines += ["toolbar, .toolbar {"]
        lines += [f"  background-image: linear-gradient(to bottom, {fill[0]}, {fill[-1]});"]
        if "top_line" in tool_bar:
            lines += [f"  border-top: 1px solid {tool_bar['top_line']};"]
        if "bottom_line" in tool_bar:
            lines += [f"  border-bottom: 1px solid {tool_bar['bottom_line']};"]
        lines += ["}", ""]
        # "backdrop" is GTK's inactive window state.
        if "fill_inactive" in tool_bar:
            inactive_fill = tool_bar["fill_inactive"]
            lines += ["toolbar:backdrop, .toolbar:backdrop {"]
            lines += [f"  background-image: linear-gradient(to bottom, {inactive_fill[0]}, {inactive_fill[-1]});"]
            if "bottom_line_inactive" in tool_bar:
                lines += [f"  border-bottom: 1px solid {tool_bar['bottom_line_inactive']};"]
            lines += ["}", ""]

    selected_item = controls.get("selected_item")
    if selected_item is not None:
        items = "row:selected, treeview.view:selected, iconview:selected, flowboxchild:selected"
        fill = selected_item["fill"]
        lines += [f"{items} {{"]
        lines += [f"  background-image: linear-gradient(to bottom, {fill[0]}, {fill[-1]});"]
        edge_lines = []
        if "top_line" in selected_item:
            edge_lines.append(f"inset 0 1px {selected_item['top_line']}")
        if "bottom_line" in selected_item:
            edge_lines.append(f"inset 0 -1px {selected_item['bottom_line']}")
        if edge_lines:
            lines += [f"  box-shadow: {', '.join(edge_lines)};"]
        if "text_color" in selected_item:
            lines += [f"  color: {selected_item['text_color']};"]
        lines += ["}", ""]

    button = controls.get("button")
    if button is not None:
        def button_rule(selector, state):
            more = ["padding: 0;", f"color: {controls.get('text_color', '#000000')};"]
            return stretching_rule(selector, button[state], button["slice"], more)

        # Only plain text buttons; those inside other controls keep stock.
        lines += button_rule("button.text-button", "normal")
        lines += button_rule("button.text-button:active, button.text-button:checked", "pressed")
        lines += button_rule("button.text-button:disabled", "disabled")
        lines += button_rule("button.text-button.suggested-action, button.text-button.default", "default")
        lines += button_rule("button.text-button.suggested-action:active, button.text-button.default:active",
            "default_pressed")
        lines.append("")

    # GTK 3 calls the boxes "check" and "radio"; GTK 4 kept the names.
    for kind, node in (("checkbox", "check"), ("radio", "radio")):
        indicator = controls.get(kind)
        if indicator is None:
            continue
        width, height = indicator["size"]

        def indicator_rule(selector, states, state):
            name = Path(states.get(state, states["normal"])).name
            return [
                f"{selector} {{",
                "  background: none; border: none; box-shadow: none;",
                f"  min-width: {width}px; min-height: {height}px;",
                f"  -gtk-icon-source: {gtk_picture(version, name)};",
                "}",
            ]

        for value, pseudo_class in (("off", ""), ("on", ":checked"), ("mixed", ":indeterminate")):
            states = indicator.get(value)
            if states is None:
                continue
            lines += indicator_rule(f"{node}{pseudo_class}", states, "normal")
            lines += indicator_rule(f"{node}{pseudo_class}:active", states, "pressed")
            lines += indicator_rule(f"{node}{pseudo_class}:disabled", states, "disabled")
        lines.append("")

    return "\n".join(lines) + "\n", middles


def kde_color(text):
    """ "#rrggbb" as KDE's settings write a colour: "red,green,blue"."""
    red, green, blue, alpha = color_tuple(text)
    return f"{red},{green},{blue}"


def mixed(first, second):
    """The colour halfway between two, as "#rrggbb"."""
    one, two = color_tuple(first), color_tuple(second)
    return "#" + "".join(f"{(one[part] + two[part]) // 2:02x}" for part in range(3))


def write_kde_settings(theme, output_directory):
    """Writes kdeglobals with the icon theme and colours.

    KDE programs read these from kdeglobals, not from the Qt style; without
    them they draw text and symbolic icons for a light desktop.
    """
    controls = theme["controls"]
    lines = []
    icon_theme = theme.get("icons", {}).get("theme")
    if icon_theme is not None:
        lines += ["[Icons]", f"Theme={icon_theme}", ""]

    text = controls.get("text_color")
    # KDE colour set, background, foreground.
    sets = [
        ("Window", controls.get("window_color"), text),
        ("View", controls.get("base_color"), text),
        ("Button", controls.get("window_color"), text),
        ("Header", controls.get("window_color"), text),
        ("Tooltip", controls.get("window_color"), text),
        ("Selection", controls.get("highlight_color"), controls.get("highlight_text_color")),
    ]
    for name, background, foreground in sets:
        if background is None or foreground is None:
            continue
        lines += [
            f"[Colors:{name}]",
            f"BackgroundNormal={kde_color(background)}",
            f"BackgroundAlternate={kde_color(background)}",
            f"ForegroundNormal={kde_color(foreground)}",
            # Secondary text: halfway between text and background.
            f"ForegroundInactive={kde_color(mixed(foreground, background))}",
            "",
        ]
    if not lines:
        return

    # KDE programs also build their palette from a named colour scheme,
    # light without Plasma, so write the colours as a scheme too.
    color_lines = [line for line in lines if not line.startswith(("[Icons]", "Theme="))]
    if any(line.startswith("[Colors:") for line in color_lines):
        schemes_directory = output_directory / "share" / "color-schemes"
        schemes_directory.mkdir(parents=True, exist_ok=True)
        (schemes_directory / f"{APP_THEME_NAME}.colors").write_text(
            "\n".join(["[General]", f"ColorScheme={APP_THEME_NAME}", f"Name={APP_THEME_NAME}", ""] + color_lines))
        lines += ["[UiSettings]", f"ColorScheme={APP_THEME_NAME}", ""]

    settings_directory = output_directory / "xdg"
    settings_directory.mkdir(parents=True, exist_ok=True)
    (settings_directory / "kdeglobals").write_text("\n".join(lines))


def write_gtk_settings(theme, output_directory):
    """Writes GTK's settings.ini (read from every XDG config dir)."""
    lines = ["[Settings]"]
    icon_theme = theme.get("icons", {}).get("theme")
    if icon_theme is not None:
        lines.append(f"gtk-icon-theme-name={icon_theme}")
    if theme["controls"].get("dark", False):
        # Editors and terminals pick their own colours from this.
        lines.append("gtk-application-prefer-dark-theme=true")
    if len(lines) == 1:
        return
    for version in GTK_BASE_IMPORT:
        settings_directory = output_directory / "xdg" / version
        settings_directory.mkdir(parents=True, exist_ok=True)
        (settings_directory / "settings.ini").write_text("\n".join(lines) + "\n")


def write_gtk_themes(theme, theme_directory, output_directory):
    for version in GTK_BASE_IMPORT:
        gtk_directory = output_directory / "share" / "themes" / APP_THEME_NAME / version
        assets_directory = gtk_directory / "assets"
        assets_directory.mkdir(parents=True, exist_ok=True)

        controls_pictures = theme_directory / "images" / "controls"
        if controls_pictures.is_dir():
            for picture in controls_pictures.glob("*.png"):
                shutil.copyfile(picture, assets_directory / picture.name)

        text, middles = gtk_css(theme, theme_directory, version)
        (gtk_directory / "gtk.css").write_text(text)
        cut_middles(assets_directory, middles)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("theme", type=Path, help="desktop theme directory, holding theme.toml")
    parser.add_argument("output", type=Path, help="directory to write the toolkit themes into")
    arguments = parser.parse_args()

    theme_file = arguments.theme / "theme.toml"
    if not theme_file.is_file():
        fail(f"{theme_file} does not exist")
    with open(theme_file, "rb") as file:
        theme = usable_icon_theme(tomllib.load(file), arguments.theme)

    if "controls" not in theme:
        print(f"{theme_file} has no [controls] table; applications keep their own look")
        return

    # Start clean, so nothing from an earlier theme lingers.
    if arguments.output.exists():
        shutil.rmtree(arguments.output)

    wrote_qt = write_kvantum_theme(theme, arguments.theme, arguments.output)
    write_gtk_themes(theme, arguments.theme, arguments.output)
    write_gtk_settings(theme, arguments.output)
    write_kde_settings(theme, arguments.output)
    toolkits = "Qt and GTK" if wrote_qt else "GTK only"
    print(f"wrote toolkit themes named {APP_THEME_NAME} into {arguments.output} ({toolkits})")


if __name__ == "__main__":
    main()
