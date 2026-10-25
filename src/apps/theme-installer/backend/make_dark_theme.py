"""Builds the Sea Lion Dark theme: Sea Lion as it might have looked in the dark.

Run it after the Sea Lion theme exists:

    python make_dark_theme.py WORK OUTPUT/sea-lion OUTPUT/sea-lion-dark

It needs ProKit's decoded artwork (carfile.py):

    WORK/artwork/prokit/      index.json, colors.json, look-5/

The original has no dark appearance, but its ProKit framework has the dark
look of the professional programs ("look 5"). The Sea Lion theme's theme.toml is
changed key by key: PROKIT values come from that look, RULE colours of our own
programs are turned dark by dark_counterpart(), and CHOSEN values have no
source from 2013. Desktop pictures, typeface and icons are linked, not copied.
"""

import argparse
import colorsys
import itertools
import json
import re
import shutil
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat

DARK_LOOK = 5

# Positions in ProKit's rendition keys.
KEY_VALUE = 6      # 0 off, 1 on, 2 mixed
KEY_TINT = 7       # check boxes only; 0 is the ordinary one
KEY_STATE = 10     # see STATE_* below
KEY_EMPHASIS = 11  # push buttons only; 3 is the blue default button

# States, inferred from the pictures (0 and 3 are a window behind; 5 repeats 1).
STATE_ACTIVE = 1
STATE_PRESSED = 2
STATE_DISABLED = 4

# --- Window title bar ------------------------------------------------------------
# PROKIT: TitleBar_WindowBackgroundGradient (gradients.json), states 0 and 4.
TITLE_BAR_GRADIENT = ["#525252", "#383838"]
TITLE_BAR_GRADIENT_INACTIVE = ["#3b3b3b", "#2e2e2e"]

# --- Dock shelf --------------------------------------------------------------------
# CHOSEN: ProKit has no Dock. The glass shelf keeps its opacities; its light
# greys become dark greys near ProKit's window greys.
DOCK_SURFACE = ["#4a4a4aba", "#333333a6", "#2a2a2a8f"]
DOCK_FACE = ["#1a1a1acc", "#2e2e2edb"]
DOCK_LINE = "#ffffff4d"
DOCK_OUTLINE = "#0000004d"

DESKTOP_PICTURE = "wallpapers/Galaxy.jpg"

# --- Tables our own programs draw ---------------------------------------------------
# Turned dark by dark_counterpart(). Overlays that are already dark are left
# out: the rule would turn their white text dark.
RULE_TABLES = [
    "dialog", "about", "settings.overview", "search", "search.clear_button", "search.box", "theme_installer",
    "lock.box", "lock.field",
]

# Light colours under keys with these words are text on something coloured
# and stay light; other light colours are backgrounds and turn dark.
TEXT_WORDS = ("text", "glyph", "title", "label", "name", "heading", "number", "date",
              "mark", "tick", "arrow", "cross")
# Words that say a key is not text after all (header_text_shadow_color).
BACKGROUND_WORDS = ("column", "background", "fill", "row", "band", "shadow")

# The rule's two ends: white becomes ProKit's controlBackgroundColor, black a
# light grey between its controlTextColor and selectedTextColor.
DARKEST = 0x28
LIGHTEST = 0xc8

# Turning each stop over would light a gradient from below, so only its
# average is turned over; each stop keeps its offset from it, scaled by this.
GRADIENT_CONTRAST = 0.6

# A pale tint (lightness above PALE) keeps this share of its saturation when
# turned dark, or it would become a loud colour.
PALE_TINT_SATURATION = 0.35
PALE = 0.8

def fail(message):
    sys.exit(f"make_dark_theme: {message}")


# --- ProKit's artwork ---------------------------------------------------------------
class ProKit:
    """The decoded catalogue: pictures by source file and key, colours by name."""

    def __init__(self, prokit_dir):
        self.dir = prokit_dir
        index_path = prokit_dir / "index.json"
        if not index_path.is_file():
            fail(f"{index_path} is missing; run carfile.py on ProThemeBits.car first")
        renditions = json.loads(index_path.read_text())["renditions"]
        self.renditions = [r for r in renditions if r.get("file")]
        self.colours = json.loads((prokit_dir / "colors.json").read_text())[str(DARK_LOOK)]

    def colour(self, name):
        """A named colour as "#rrggbb", or "#rrggbbaa" when not opaque."""
        value = self.colours.get(name)
        if value is None:
            fail(f"ProKit's look {DARK_LOOK} has no colour named {name}")
        return value[:7] if value.endswith("ff") else value

    def picture(self, source, scale=1, look=DARK_LOOK, **key_values):
        """The one picture of `source` at `scale` whose key has the given values."""
        positions = {"value": KEY_VALUE, "tint": KEY_TINT, "state": KEY_STATE, "emphasis": KEY_EMPHASIS}
        matches = [
            r for r in self.renditions
            if r["source_name"] == source and r["scale"] == scale and r["look"] == look
            and all(r["key"][positions[name]] == wanted for name, wanted in key_values.items())
        ]
        if len(matches) != 1:
            fail(f"expected one picture of {source} in look {look} at scale {scale} with {key_values}, "
                 f"found {len(matches)}")
        return Image.open(self.dir / matches[0]["file"]).convert("RGBA")


def widen_middle(picture, width, left_cap, right_cap):
    """Makes a picture `width` wide by stretching the columns between its caps.

    ProKit's double-density push buttons are 64 wide, not twice the single 72;
    the theme format wants exactly twice.
    """
    middle = picture.crop((left_cap, 0, picture.width - right_cap, picture.height))
    middle = middle.resize((width - left_cap - right_cap, picture.height), Image.NEAREST)
    result = Image.new("RGBA", (width, picture.height))
    result.paste(picture.crop((0, 0, left_cap, picture.height)), (0, 0))
    result.paste(middle, (left_cap, 0))
    result.paste(picture.crop((picture.width - right_cap, 0, picture.width, picture.height)),
                 (width - right_cap, 0))
    return result


def enlarged(picture):
    """Twice the size, for pictures ProKit has at single density only."""
    return picture.resize((picture.width * 2, picture.height * 2), Image.LANCZOS)


def save_pair(single, double, path):
    single.save(path.with_name(f"{path.stem}.png"))
    double.save(path.with_name(f"{path.stem}@2x.png"))


# --- Pictures ------------------------------------------------------------------------
# Push buttons: PushButton_RG_HUD, as the plain PushButton_RG is see-through
# and nearly vanishes on dark grey. Stretch caps in single-density pixels.
BUTTON_CAP = 14
BUTTON_STATES = {
    "": {"state": STATE_ACTIVE, "emphasis": 0},
    "-pressed": {"state": STATE_PRESSED, "emphasis": 0},
    "-disabled": {"state": STATE_DISABLED, "emphasis": 0},
    "-default": {"state": STATE_ACTIVE, "emphasis": 3},
    # ProKit's default button does not change when pressed; this is the nearest.
    "-default-pressed": {"state": STATE_PRESSED, "emphasis": 0},
}
# The toolkits' button, without the empty rows; its corners take 5 rows.
BUTTON_BOX_SLICE = [5, BUTTON_CAP, 5, BUTTON_CAP]
# The whole picture, as the dialogs draw it at its own height.
BUTTON_SLICE = [10, BUTTON_CAP, 10, BUTTON_CAP]

DIALOG_BUTTON_HEIGHT = 25
# The row the label stands on, counted from 1: the 15th of the visible body,
# which starts on the picture's 3rd row.
DIALOG_BUTTON_TEXT_ROW = 3 + 14

# The About window's small button (PushButton_SM_HUD), drawn at its own
# height, stretching sideways only; its label on the 13th row of the body.
SMALL_BUTTON_SLICE = [0, BUTTON_CAP, 0, BUTTON_CAP]
SMALL_BUTTON_HEIGHT = 22
SMALL_BUTTON_TEXT_ROW = 3 + 12
# The About window places its buttons by their pictures' top rows, and its
# closing lines under the second button's picture. ProKit's small body
# starts one row nearer its picture's top than the light one's, and its
# picture is 5 rows shorter, so the light theme's places (169, 7 and 10)
# take one row more before each button and 5 more after the second, to keep
# the buttons' bodies and the closing lines on the same rows.
ABOUT_UPDATE_BUTTON_TOP = 170
ABOUT_MORE_BUTTON_GAP = 8
ABOUT_FOOTER_GAP = 14


def make_button_pictures(prokit, controls_dir):
    for size_name, source in (("button", "PushButton_RG_HUD.psd"), ("button-small", "PushButton_SM_HUD.psd")):
        for suffix, key in BUTTON_STATES.items():
            # The About window's small buttons are never default buttons.
            if size_name == "button-small" and "default" in suffix:
                continue
            single = prokit.picture(source, 1, value=0, **key)
            double = prokit.picture(source, 2, value=0, **key)
            double = widen_middle(double, single.width * 2, BUTTON_CAP * 2, BUTTON_CAP * 2)
            save_pair(single, double, controls_dir / f"{size_name}{suffix}.png")

    plain = Image.open(controls_dir / "button.png")
    first_row, last_row = plain.getchannel("A").getbbox()[1::2]
    for suffix in BUTTON_STATES:
        for density_suffix, density in (("", 1), ("@2x", 2)):
            picture = Image.open(controls_dir / f"button{suffix}{density_suffix}.png")
            box = picture.crop((0, first_row * density, picture.width, last_row * density))
            box.save(controls_dir / f"button{suffix}-box{density_suffix}.png")


INDICATOR_SIZE = [18, 20]
INDICATOR_PICTURES = {
    "checkbox-off": ("Checkbox_RG.psd", 0, STATE_ACTIVE),
    "checkbox-off-pressed": ("Checkbox_RG.psd", 0, STATE_PRESSED),
    "checkbox-off-disabled": ("Checkbox_RG.psd", 0, STATE_DISABLED),
    "checkbox-on": ("Checkbox_RG.psd", 1, STATE_ACTIVE),
    "checkbox-on-pressed": ("Checkbox_RG.psd", 1, STATE_PRESSED),
    "checkbox-on-disabled": ("Checkbox_RG.psd", 1, STATE_DISABLED),
    "checkbox-mixed": ("Checkbox_RG.psd", 2, STATE_ACTIVE),
    "radio-off": ("Radio_RG.psd", 0, STATE_ACTIVE),
    "radio-off-pressed": ("Radio_RG.psd", 0, STATE_PRESSED),
    "radio-off-disabled": ("Radio_RG.psd", 0, STATE_DISABLED),
    "radio-on": ("Radio_RG.psd", 1, STATE_ACTIVE),
    "radio-on-pressed": ("Radio_RG.psd", 1, STATE_PRESSED),
    "radio-on-disabled": ("Radio_RG.psd", 1, STATE_DISABLED),
}


def make_indicator_pictures(prokit, controls_dir):
    for name, (source, value, state) in INDICATOR_PICTURES.items():
        keys = {"value": value, "state": state}
        if source.startswith("Checkbox"):
            keys["tint"] = 0
        single = prokit.picture(source, 1, **keys)
        save_pair(single, redrawn_double(single, name), controls_dir / f"{name}.png")


# --- Redrawing the check boxes and radio buttons at double density ---------------
# ProKit has them at single density only, and enlarged they are soft. The
# outline and sign are drawn anew from these shapes (in pixel edges).
BOX = (2, 3, 16, 17)
BOX_RADIUS = 2
CIRCLE_CENTRE, CIRCLE_RADIUS = (9, 10), 7
TICK = [(5.5, 9.5), (9.0, 13.0), (12.8, 4.5)]
TICK_WIDTH = 1.6
DASH = (5, 9, 13, 11)
DOT_CENTRE, DOT_RADIUS = (9, 10.3), 2.2
# A sign's pixels differ from the body's colour on their row by more than
# this (red, green and blue differences summed).
SIGN_DIFFERENCE = 40
# Drawn this many times larger again, then shrunk, for smooth edges.
SMOOTHING = 8


def sign_of(name):
    if name.startswith("checkbox-on"):
        return "tick"
    if name.startswith("checkbox-mixed"):
        return "dash"
    if name.startswith("radio-on"):
        return "dot"
    return None


def smooth_drawing(size, draw_shapes):
    """A mask drawn at SMOOTHING times `size` by draw_shapes(draw, scale), then shrunk."""
    scale = 2 * SMOOTHING
    big = Image.new("L", (size[0] * scale, size[1] * scale), 0)
    draw_shapes(ImageDraw.Draw(big), scale)
    return big.resize((size[0] * 2, size[1] * 2), Image.LANCZOS)


def sign_pixels(picture, reference_column, first_row, last_row):
    """Positions of a sign's pixels: those unlike their row's body colour."""
    found = []
    for y in range(first_row, last_row + 1):
        reference = picture.getpixel((reference_column, y))
        for x in range(reference_column + 1, picture.width - reference_column - 1):
            colour = picture.getpixel((x, y))
            if sum(abs(a - b) for a, b in zip(colour[:3], reference[:3])) > SIGN_DIFFERENCE:
                found.append((x, y))
    return found


def redrawn_double(picture, name):
    is_box = name.startswith("checkbox")
    sign = sign_of(name)
    body = picture.copy()

    # Take the sign out and keep its colour: the middle of its brightest
    # pixels for the tick and the dash, its darkest for the dot.
    sign_colour = None
    if sign:
        reference_column = 3 if is_box else 4
        # Row 4 and the last rows are the body's own light lines, not the sign.
        pixels = sign_pixels(picture, reference_column, 5 if sign != "tick" else 4, 14)
        if sign == "tick":
            pixels = [p for p in pixels if not (p[1] == 4 and p[0] < 11)]
        colours = sorted((picture.getpixel(p) for p in pixels), key=lambda c: sum(c[:3]))
        if sign == "dot":
            pixels = [p for p in pixels if abs(p[0] + 0.5 - DOT_CENTRE[0]) < 3 and abs(p[1] + 0.5 - DOT_CENTRE[1]) < 3]
            colours = sorted((picture.getpixel(p) for p in pixels), key=lambda c: sum(c[:3]))
            sign_colour = colours[0]
        else:
            brightest = colours[-5:]
            sign_colour = brightest[len(brightest) // 2]
        for x, y in pixels:
            body.putpixel((x, y), picture.getpixel((reference_column, y)))

    size = picture.size
    doubled_size = (size[0] * 2, size[1] * 2)
    inside = body.resize(doubled_size, Image.NEAREST if is_box else Image.LANCZOS)
    outside = picture.resize(doubled_size, Image.LANCZOS)

    def shape(draw, scale):
        if is_box:
            left, top, right, bottom = (edge * scale for edge in BOX)
            draw.rounded_rectangle((left, top, right - 1, bottom - 1), radius=BOX_RADIUS * scale, fill=255)
        else:
            x, y = CIRCLE_CENTRE
            radius = CIRCLE_RADIUS
            draw.ellipse(((x - radius) * scale, (y - radius) * scale,
                          (x + radius) * scale - 1, (y + radius) * scale - 1), fill=255)

    # Times the picture's own opacity: a disabled control is see-through.
    mask = ImageChops.multiply(smooth_drawing(size, shape), inside.getchannel("A"))
    inside.putalpha(mask)
    result = Image.alpha_composite(outside, inside)

    if sign:
        def sign_shape(draw, scale):
            if sign == "tick":
                points = [(x * scale, y * scale) for x, y in TICK]
                draw.line(points, fill=255, width=round(TICK_WIDTH * scale), joint="curve")
            elif sign == "dash":
                left, top, right, bottom = (edge * scale for edge in DASH)
                draw.rounded_rectangle((left, top, right - 1, bottom - 1), radius=scale // 2, fill=255)
            else:
                x, y = DOT_CENTRE
                draw.ellipse(((x - DOT_RADIUS) * scale, (y - DOT_RADIUS) * scale,
                              (x + DOT_RADIUS) * scale, (y + DOT_RADIUS) * scale), fill=255)

        layer = Image.new("RGBA", doubled_size, sign_colour[:3] + (255,))
        layer.putalpha(smooth_drawing(size, sign_shape))
        result = Image.alpha_composite(result, layer)
    return result


# --- The other controls of ordinary programs --------------------------------------
# Single density only; enlarging shows little on these smooth shapes.
DARK_LOOK_7 = 7   # the other dark look; it has the HUD pop-up button look 5 lacks
OTHER_CONTROL_PICTURES = {
    "text-field": ("TextField_HUD.psd", DARK_LOOK, {"state": STATE_ACTIVE}),
    "text-field-disabled": ("TextField_HUD.psd", DARK_LOOK, {"state": STATE_DISABLED}),
    "popup": ("PopUpMenu_RG_HUD.psd", DARK_LOOK_7, {"state": STATE_ACTIVE}),
    "popup-pressed": ("PopUpMenu_RG_HUD.psd", DARK_LOOK_7, {"state": STATE_PRESSED}),
    "popup-disabled": ("PopUpMenu_RG_HUD.psd", DARK_LOOK_7, {"state": STATE_DISABLED}),
    # Tabs: a single segment, grey (tint 0), and blue (tint 1) when chosen.
    "tab": ("SegmentedSingle_RG_HUD.psd", DARK_LOOK, {"value": 0, "tint": 0, "state": STATE_ACTIVE}),
    "tab-pressed": ("SegmentedSingle_RG_HUD.psd", DARK_LOOK, {"value": 0, "tint": 0, "state": STATE_PRESSED}),
    "tab-disabled": ("SegmentedSingle_RG_HUD.psd", DARK_LOOK, {"value": 0, "tint": 0, "state": STATE_DISABLED}),
    "tab-selected": ("SegmentedSingle_RG_HUD.psd", DARK_LOOK, {"value": 0, "tint": 1, "state": STATE_ACTIVE}),
    "tab-selected-pressed": ("SegmentedSingle_RG_HUD.psd", DARK_LOOK, {"value": 0, "tint": 1, "state": STATE_PRESSED}),
    "slider-track": ("Slider_RG_HorizontalTrack_HUD.psd", DARK_LOOK, {"state": STATE_ACTIVE}),
    "slider-thumb": ("Slider_RG_NonDirectionalSlider_HUD.psd", DARK_LOOK, {"state": STATE_ACTIVE}),
    "slider-thumb-pressed": ("Slider_RG_NonDirectionalSlider_HUD.psd", DARK_LOOK, {"state": STATE_PRESSED}),
    "slider-thumb-disabled": ("Slider_RG_NonDirectionalSlider_HUD.psd", DARK_LOOK, {"state": STATE_DISABLED}),
    "progress-track": ("ProgressBarDeterminate_RG_Background.psd", DARK_LOOK, {"state": STATE_ACTIVE}),
}
# Slices from the catalogue, as [top, right, bottom, left].
TEXT_FIELD_SLICE = [4, 4, 5, 4]
POPUP_SLICE = [5, 20, 6, 9]
# The box's border is on column 2, so the text starts 10 after it.
POPUP_TEXT_INSET = 12
TAB_SLICE = [5, 15, 6, 15]
SLIDER_TRACK_SLICE = [2, 5, 2, 5]
PROGRESS_TRACK_SLICE = [1, 3, 2, 3]
# The fill is a capped bar; its uniform middle columns are the repeated tile.
PROGRESS_FILL_COLUMNS = (2, 18)


def make_other_control_pictures(prokit, controls_dir):
    for name, (source, look, keys) in OTHER_CONTROL_PICTURES.items():
        single = prokit.picture(source, 1, look=look, **keys)
        save_pair(single, enlarged(single), controls_dir / f"{name}.png")
    fill = prokit.picture("ProgressBarDeterminate_RG_Fill.psd", 1, value=0, state=STATE_ACTIVE)
    left, right = PROGRESS_FILL_COLUMNS
    tile = fill.crop((left, 0, right, fill.height))
    save_pair(tile, enlarged(tile), controls_dir / "progress-fill.png")


# ProKit's menubarTextColor is too dim for a whole desktop's menu bar; this is
# its brightest neutral text colour.
PANEL_TEXT_COLOUR = "commandKeyboardTextColor"


# MenuBar is 21 rows of gradient and a black last row, which the panel draws
# as its separator; one column of the gradient is the background.
PANEL_ROWS = 21


def make_panel_pictures(prokit, images_dir, light_images_dir):
    bar = prokit.picture("MenuBar.psd", 1, state=0)
    column = bar.crop((bar.width // 2, 0, bar.width // 2 + 1, PANEL_ROWS))
    save_pair(column, enlarged(column), images_dir / "panel-background.png")

    highlight = prokit.picture("MenuBar.psd", 1, state=STATE_PRESSED)
    column = highlight.crop((highlight.width // 2, 0, highlight.width // 2 + 1, highlight.height))
    save_pair(column, enlarged(column), images_dir / "panel-title-highlight.png")

    # The logo, sharp at both densities, painted in the panel's text
    # colours; ProKit's own exists at single density only.
    for name, colour in (("panel-logo", prokit.colour(PANEL_TEXT_COLOUR)),
                         ("panel-logo-highlighted", prokit.colour("menubarSelectedTextColor"))):
        for suffix in ("", "@2x"):
            source = Image.open(light_images_dir / f"panel-logo{suffix}.png").convert("RGBA")
            painted = Image.new("RGBA", source.size, colour)
            painted.putalpha(source.getchannel("A"))
            painted.save(images_dir / f"{name}{suffix}.png")


# The pale buttons of a window behind glare on a dark title bar; they are
# darkened to the average grey of ProKit's TitleBar_RG_WindowControl.
INACTIVE_BUTTON_GREY = 54


def make_inactive_button_pictures(images_dir):
    # A disabled button is as pale as an inactive one, and glares the same.
    for button in ("close", "minimize", "maximize"):
        for state, suffix in itertools.product(("inactive", "disabled"), ("", "@2x")):
            path = images_dir / f"{button}-{state}{suffix}.png"
            picture = Image.open(path).convert("RGBA")
            average = sum(ImageStat.Stat(picture.convert("RGB"), picture.getchannel("A")).mean) / 3
            factor = INACTIVE_BUTTON_GREY / average
            red, green, blue, alpha = picture.split()
            red, green, blue = (channel.point(lambda value: round(value * factor)) for channel in (red, green, blue))
            Image.merge("RGBA", (red, green, blue, alpha)).save(path)


# --- Colours: the rule for our own programs ---------------------------------------
def parse_colour(text):
    digits = text.lstrip("#")
    red, green, blue = (int(digits[i:i + 2], 16) for i in (0, 2, 4))
    alpha = digits[6:8] if len(digits) == 8 else ""
    return red, green, blue, alpha


def turned_over(lightness):
    """Lightness from 0 to 1, turned over: 1 becomes DARKEST, 0 LIGHTEST."""
    return (LIGHTEST + (DARKEST - LIGHTEST) * lightness) / 255


def keeps_its_colour(red, green, blue, alpha, key):
    """Whether the rule leaves a colour as it is (see dark_counterpart)."""
    _, lightness, saturation = colorsys.rgb_to_hls(red / 255, green / 255, blue / 255)
    is_text = any(word in key for word in TEXT_WORDS) and not any(word in key for word in BACKGROUND_WORDS)
    # See-through black is a shadow or line, unless the key names text.
    is_shadow = alpha and max(red, green, blue) < 0x20 and not is_text
    is_strong_colour = saturation > 0.25 and lightness < PALE
    is_text_on_colour = lightness > 0.9 and is_text
    return is_shadow or is_strong_colour or is_text_on_colour


def with_lightness(red, green, blue, alpha, new_lightness):
    """The colour at a new lightness; a pale tint also loses saturation."""
    hue, lightness, saturation = colorsys.rgb_to_hls(red / 255, green / 255, blue / 255)
    if lightness > PALE:
        saturation *= PALE_TINT_SATURATION
    new_lightness = min(1.0, max(0.0, new_lightness))
    new_red, new_green, new_blue = colorsys.hls_to_rgb(hue, new_lightness, saturation)
    return "#{:02x}{:02x}{:02x}{}".format(
        round(new_red * 255), round(new_green * 255), round(new_blue * 255), alpha
    )


def lightness_of(red, green, blue):
    return colorsys.rgb_to_hls(red / 255, green / 255, blue / 255)[1]


def dark_counterpart(text, key):
    """The dark theme's colour for one of the light theme's colours in our programs.

    Lightness is turned over between DARKEST and LIGHTEST; hue and saturation
    stay. Shadows, strong colours and text on colour are kept as they are.
    """
    red, green, blue, alpha = parse_colour(text)
    if keeps_its_colour(red, green, blue, alpha, key):
        return text
    return with_lightness(red, green, blue, alpha, turned_over(lightness_of(red, green, blue)))


def dark_gradient(texts, key):
    """A gradient's colours darkened as a whole (see GRADIENT_CONTRAST)."""
    colours = [parse_colour(text) for text in texts]
    lightnesses = [lightness_of(red, green, blue) for red, green, blue, _ in colours]
    average = sum(lightnesses) / len(lightnesses)
    result = []
    for text, colour, lightness in zip(texts, colours, lightnesses):
        if keeps_its_colour(*colour, key):
            result.append(text)
        else:
            result.append(with_lightness(*colour, turned_over(average) + (lightness - average) * GRADIENT_CONTRAST))
    return result


COLOUR = re.compile(r'"#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?"')
# A gradient's list, or a lone colour: the rule treats them differently.
GRADIENT_OR_COLOUR = re.compile(r'gradient = \[[^\]]*\]|"#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?"')
KEY_LINE = re.compile(r"^\s*\"?([A-Za-z0-9_-]+)\"?\s*=")


def darkened(found, key):
    """The rule applied to what GRADIENT_OR_COLOUR found in a line."""
    if found.startswith("gradient"):
        texts = [match[1:-1] for match in COLOUR.findall(found)]
        return "gradient = [" + ", ".join(f'"{text}"' for text in dark_gradient(texts, key)) + "]"
    return f'"{dark_counterpart(found[1:-1], key)}"'


class ThemeText:
    """theme.toml as a list of tables, each a header line and its lines.

    Only single-line values are changed; make_theme.py writes these on one line.
    """

    def __init__(self, text):
        self.preamble = []
        self.tables = []   # [name, [lines]]
        for line in text.splitlines():
            header = re.match(r"^\[([^\]]+)\]\s*$", line)
            if header:
                self.tables.append([header.group(1), [line]])
            elif self.tables:
                self.tables[-1][1].append(line)
            else:
                self.preamble.append(line)

    def lines_of(self, table):
        for name, lines in self.tables:
            if name == table:
                return lines
        fail(f"the Sea Lion theme has no table [{table}]")

    def key_index(self, table, key):
        lines = self.lines_of(table)
        for number, line in enumerate(lines):
            match = KEY_LINE.match(line)
            if match and match.group(1) == key and not line.lstrip().startswith("#"):
                return lines, number
        fail(f"the Sea Lion theme has no key {key} in [{table}]")

    def has_key(self, table, key):
        lines = self.lines_of(table)
        return any(KEY_LINE.match(line) and KEY_LINE.match(line).group(1) == key
                   and not line.lstrip().startswith("#") for line in lines)

    # `note` says why a value changes; it stays in the code, not the theme.

    def set(self, table, key, value, note):
        """Gives `key` a new value."""
        lines, number = self.key_index(table, key)
        lines[number] = f"{key} = {value}"

    def substitute(self, table, key, pattern, replacement, note):
        """Replaces the part of a key's line that `pattern` matches."""
        lines, number = self.key_index(table, key)
        changed = re.sub(pattern, replacement, lines[number])
        if changed == lines[number]:
            fail(f"[{table}] {key} does not match {pattern}")
        lines[number] = changed

    def apply_rule(self, table):
        """Turns every colour value of a table dark by dark_counterpart."""
        lines = self.lines_of(table)
        for number, line in enumerate(lines):
            if line.lstrip().startswith("#") or "=" not in line:
                continue
            key_match = KEY_LINE.match(line)
            key = key_match.group(1) if key_match else ""
            lines[number] = GRADIENT_OR_COLOUR.sub(lambda match: darkened(match.group(0), key), line)

    def table_names(self):
        return [name for name, _ in self.tables]

    def text(self):
        parts = list(self.preamble)
        for _, lines in self.tables:
            parts.extend(lines)
        return "\n".join(parts) + "\n"


# --- The changes, part by part ------------------------------------------------------
def change_theme_header(theme):
    theme.set("theme", "name", '"Sea Lion Dark"', "the name of this theme.")
    theme.set("theme", "description",
              '"Sea Lion in the dark look of the professional programs."',
              "see make_dark_theme.py.")


def change_window(theme, prokit):
    table = "window.titlebar"
    gradient = "[" + ", ".join(f'"{c}"' for c in TITLE_BAR_GRADIENT) + "], positions = [0, 1]"
    theme.substitute(table, "background", r"gradient = \[[^\]]*\], positions = \[[^\]]*\]",
                     f"gradient = {gradient}",
                     "PROKIT, TitleBar_WindowBackgroundGradient, state 0 (a window in front).")
    gradient = "[" + ", ".join(f'"{c}"' for c in TITLE_BAR_GRADIENT_INACTIVE) + "]"
    theme.substitute(table, "background_inactive", r"gradient = \[[^\]]*\]", f"gradient = {gradient}",
                     "PROKIT, the same gradient's state 4 (a window behind).")
    theme.substitute(table, "background_joined", r"gradient = \[[^\]]*\], positions = \[[^\]]*\]",
                     "gradient = [" + ", ".join(f'"{c}"' for c in TITLE_BAR_GRADIENT) + "], positions = [0, 1]",
                     "PROKIT, the plain title bar's gradient: the tool bars run on from its last grey.")
    theme.substitute(table, "background_joined_inactive", r"gradient = \[[^\]]*\], positions = \[[^\]]*\]",
                     "gradient = [" + ", ".join(f'"{c}"' for c in TITLE_BAR_GRADIENT_INACTIVE) + "], positions = [0, 1]",
                     "PROKIT, the plain inactive title bar's gradient, as above.")
    theme.set(table, "separator", f'["{prokit.colour("windowTitlebarSeparatorActiveColor")}"]',
              "PROKIT, windowTitlebarSeparatorActiveColor.")
    theme.set(table, "separator_inactive", f'["{prokit.colour("windowTitlebarSeparatorInactiveColor")}"]',
              "PROKIT, windowTitlebarSeparatorInactiveColor.")
    theme.set(table, "highlight", '["#ffffff14"]',
              "CHOSEN: a faint light top line, about as strong as ProKit's menuSeparatorColor.")
    theme.set(table, "highlight_inactive", '["#ffffff0a"]', "CHOSEN: half of the line above.")
    theme.set(table, "title_color", f'"{prokit.colour("windowFrameTextColor")}"',
              "PROKIT, windowFrameTextColor.")
    theme.set(table, "title_color_inactive", f'"{prokit.colour("windowFrameInactiveTextColor")}"',
              "PROKIT, windowFrameInactiveTextColor.")


def change_panel(theme, prokit):
    theme.set("panel", "separator", f'["{prokit.colour("menubarEdgeColor")}"]',
              "PROKIT, menubarEdgeColor: the black last row of ProKit's MenuBar picture.")
    theme.set("panel", "highlight", '["#ffffff00"]',
              "PROKIT: the bar's picture has its own light first row, so none is added.")
    theme.set("panel", "text_color", f'"{prokit.colour(PANEL_TEXT_COLOUR)}"',
              f"PROKIT, {PANEL_TEXT_COLOUR} (see PANEL_TEXT_COLOUR in the generator).")
    theme.set("panel", "title_highlight_text_color", f'"{prokit.colour("menubarSelectedTextColor")}"',
              "PROKIT, menubarSelectedTextColor.")


def change_menu(theme, prokit):
    theme.set("menu", "background", f'"{prokit.colour("menuBackgroundColor")}"', "PROKIT, menuBackgroundColor.")
    theme.set("menu", "text_color", f'"{prokit.colour("menuText")}"', "PROKIT, menuText.")
    theme.set("menu", "disabled_text_color", f'"{prokit.colour("disabledControlTextColor")}"',
              "PROKIT, disabledControlTextColor.")
    theme.set("menu", "separator_color", f'"{prokit.colour("menuSeparatorColor")}"', "PROKIT, menuSeparatorColor.")
    top, start, end = (prokit.colour(name) for name in
                       ("selectedMenuItemTopColor", "selectedMenuItemGradientStartColor",
                        "selectedMenuItemGradientEndColor"))
    # The top colour is one row of the item's 19, about 0.05.
    theme.set("menu", "highlight", f'{{ gradient = ["{top}", "{start}", "{end}"], positions = [0, 0.05, 1] }}',
              "PROKIT, selectedMenuItemTopColor, then GradientStartColor to GradientEndColor.")
    theme.set("menu", "highlight_text_color", f'"{prokit.colour("selectedMenuItemTextColor")}"',
              "PROKIT, selectedMenuItemTextColor.")
    theme.set("menu", "rim", f'"{prokit.colour("menuBackgroundOutlineColor")}"',
              "PROKIT, menuBackgroundOutlineColor.")
    for key in ("submenu_arrow_shape", "check_mark_shape"):
        theme.substitute("menu", key, '"#404040"', f'"{prokit.colour("menuText")}"',
                         "PROKIT, menuText, like the labels.")


def change_dock(theme):
    note = "CHOSEN (ProKit has no Dock): the light theme's shelf in dark greys, see the generator."
    theme.set("dock.shelf", "surface", "[" + ", ".join(f'"{c}"' for c in DOCK_SURFACE) + "]", note)
    theme.set("dock.shelf", "face", "[" + ", ".join(f'"{c}"' for c in DOCK_FACE) + "]", note)
    theme.set("dock.shelf", "line", f'"{DOCK_LINE}"', note)
    theme.set("dock.shelf", "outline", f'"{DOCK_OUTLINE}"', note)


def change_controls(theme, prokit):
    theme.set("controls", "kvantum_base", '"KvMojave"',
              "the dark form of the stock theme the Sea Lion theme starts from.")
    lines, number = theme.key_index("controls", "kvantum_base")
    lines.insert(number + 1, "dark = true")
    theme.set("controls", "window_color", f'"{prokit.colour("windowBackgroundColor")}"',
              "PROKIT, windowBackgroundColor.")
    theme.set("controls", "text_color", f'"{prokit.colour("controlTextColor")}"', "PROKIT, controlTextColor.")
    theme.set("controls", "base_color", f'"{prokit.colour("controlBackgroundColor")}"',
              "PROKIT, controlBackgroundColor.")
    theme.set("controls", "highlight_color", f'"{prokit.colour("alternateSelectedControlColor")}"',
              "PROKIT, alternateSelectedControlColor.")
    theme.set("controls", "highlight_text_color", '"#ffffff"',
              "PROKIT: selectedTextColor and its kin are near white; white as in the light theme.")

    theme.set("controls.tool_bar", "fill", f'["{TITLE_BAR_GRADIENT[1]}", "{prokit.colour("windowBackgroundColor")}"]',
              "PROKIT: on from the title bar's last grey to windowBackgroundColor, as the light theme "
              "runs the tool bar on from its title bar.")
    theme.set("controls.tool_bar", "bottom_line", f'"{prokit.colour("paneSeparatorColor")}"',
              "PROKIT, paneSeparatorColor.")
    theme.set("controls.tool_bar", "fill_inactive",
              f'["{TITLE_BAR_GRADIENT_INACTIVE[1]}", "{prokit.colour("windowBackgroundColor")}"]',
              "PROKIT: on from the inactive title bar's last grey, as the fill above.")
    theme.set("controls.tool_bar", "bottom_line_inactive", f'"{prokit.colour("paneSeparatorColor")}"',
              "PROKIT, paneSeparatorColor.")

    theme.set("controls.selected_item", "fill",
              f'["{prokit.colour("navPanelSidebarHighlightColorActiveKeyStart")}", '
              f'"{prokit.colour("navPanelSidebarHighlightColorActiveKeyEnd")}"]',
              "PROKIT, the side bar's selection, navPanelSidebarHighlightColorActiveKey Start to End.")
    theme.set("controls.selected_item", "top_line", f'"{prokit.colour("navPanelSidebarHighlightColorActiveKeyBacking")}"',
              "PROKIT, navPanelSidebarHighlightColorActiveKeyBacking.")
    theme.set("controls.selected_item", "bottom_line", f'"{prokit.colour("selectedMenuItemGradientEndColor")}"',
              "PROKIT, selectedMenuItemGradientEndColor (no side bar colour for this line).")

    theme.set("controls.button", "slice", str(BUTTON_BOX_SLICE),
              "PROKIT, PushButton_RG_HUD; its caps are 14 wide, its corners 5 high.")
    theme.set("controls.checkbox", "size", str(INDICATOR_SIZE), "PROKIT, Checkbox_RG is 18 by 20.")
    theme.set("controls.radio", "size", str(INDICATOR_SIZE), "PROKIT, Radio_RG is 18 by 20.")
    note = "PROKIT, the pictures under these names are ProKit's (see OTHER_CONTROL_PICTURES in the generator)."
    theme.set("controls.text_field", "slice", str(TEXT_FIELD_SLICE), note)
    theme.set("controls.popup_button", "slice", str(POPUP_SLICE), note)
    theme.set("controls.popup_button", "text_inset", str(POPUP_TEXT_INSET), "as in the light theme, 10 after the box's border.")
    theme.set("controls.tabs", "slice", str(TAB_SLICE), note + " The pane under them stays the light theme's see-through darkening.")
    theme.set("controls.slider", "track_slice", str(SLIDER_TRACK_SLICE), note)
    theme.set("controls.progress_bar", "track_slice", str(PROGRESS_TRACK_SLICE), note)
    # ProKit has no tool bar button ("tbsegments" in the light theme); its lone
    # segmented button, which the tabs use, is the nearest: a rounded dark
    # bezel, so the back and forward buttons keep one as in the light theme.
    for key, picture in (("normal", "tab"), ("pressed", "tab-pressed"), ("disabled", "tab-disabled")):
        theme.set("controls.tool_button", key, f'"images/controls/{picture}.png"', note)
    theme.set("controls.tool_button", "slice", str(TAB_SLICE), note)


def change_dialog_and_about(theme):
    for key in ("button", "button_pressed", "button_default", "button_default_pressed"):
        theme.substitute("dialog", key, r"slice = \[[^\]]*\]", f"slice = {BUTTON_SLICE}",
                         "PROKIT, the picture is ProKit's PushButton_RG_HUD; its caps are 14 wide.")
    for key in ("button", "button_pressed", "button_disabled"):
        theme.substitute("about", key, r"slice = \[[^\]]*\]", f"slice = {SMALL_BUTTON_SLICE}",
                         "PROKIT, the picture is ProKit's PushButton_SM_HUD; its caps are 14 wide.")
    theme.set("dialog", "button_height", str(DIALOG_BUTTON_HEIGHT),
              "PROKIT, the height of PushButton_RG_HUD, drawn at its own size.")
    theme.set("dialog", "button_text_baseline_row", str(DIALOG_BUTTON_TEXT_ROW),
              "PROKIT, the 15th row of the button's visible body, which starts on the picture's 3rd.")
    theme.set("about", "button_height", str(SMALL_BUTTON_HEIGHT),
              "PROKIT, the height of PushButton_SM_HUD, drawn at its own size.")
    theme.set("about", "button_text_baseline_row", str(SMALL_BUTTON_TEXT_ROW),
              "PROKIT, the 13th row of the button's visible body, which starts on the picture's 3rd.")
    note = "PROKIT, so that the shorter picture's body stays on the light theme's rows (see ABOUT_UPDATE_BUTTON_TOP)."
    theme.set("about", "update_button_top", str(ABOUT_UPDATE_BUTTON_TOP), note)
    theme.set("about", "more_button_gap", str(ABOUT_MORE_BUTTON_GAP), note)
    theme.set("about", "footer_gap", str(ABOUT_FOOTER_GAP), note)


def change_desktop(theme):
    theme.substitute("desktop", "background", r"wallpapers/Wave\.jpg", DESKTOP_PICTURE,
                     "CHOSEN, one of the installer's own desktop pictures, a dark one.")


def change_programs(theme):
    for table in theme.table_names():
        top = table.split(".")[0]
        if table in RULE_TABLES or top in RULE_TABLES:
            theme.apply_rule(table)


# Corner radius in pixels, read from the installer's banner1line.png.
BANNER_RADIUS = 4


def change_banner(theme, prokit):
    fill = (f'{{ color = "{prokit.colour("windowBackgroundHUDColor")}", radius = {BANNER_RADIUS}, '
            f'border = {{ color = "{prokit.colour("windowFrameHUDColor")}", width = 1 }} }}')
    table = "notifications.banner"
    theme.set(table, "backgrounds", f"[{fill}, {fill}, {fill}]",
              "PROKIT, windowBackgroundHUDColor inside windowFrameHUDColor, for all three heights.")
    theme.set(table, "background_margins", "[0, 0, 0, 0]", "a plain fill has no room for a shadow.")
    theme.set(table, "title_color", f'"{prokit.colour("primaryControlTextHUDColor")}"',
              "PROKIT, primaryControlTextHUDColor.")
    theme.set(table, "text_color", f'"{prokit.colour("controlTextHUDColor")}"', "PROKIT, controlTextHUDColor.")


# --- Icons ----------------------------------------------------------------------------
# The same icons by links to the Sea Lion icon theme's folders, with one-colour
# icons taken from Breeze's dark set rather than its light one.
ICON_THEME = "SeaLionDark"
LIGHT_ICON_THEME = "SeaLion"
ICON_PARENTS = "breeze-dark,Adwaita,hicolor"


def make_icon_theme(light_dir, output_dir):
    light_icons = light_dir / "share" / "icons" / LIGHT_ICON_THEME
    share = output_dir / "share"
    icon_dir = share / "icons" / ICON_THEME
    icon_dir.mkdir(parents=True)
    # Relative links, so the pair of themes can move together.
    (share / "sounds").symlink_to(Path("..") / ".." / light_dir.name / "share" / "sounds")
    for folder in sorted(light_icons.iterdir()):
        if folder.is_dir():
            (icon_dir / folder.name).symlink_to(
                Path("../../../..") / light_dir.name / "share" / "icons" / LIGHT_ICON_THEME / folder.name)

    index = (light_icons / "index.theme").read_text()
    index = re.sub(r"^Name=.*$", f"Name={ICON_THEME}", index, count=1, flags=re.M)
    index = re.sub(r"^Comment=.*$", "Comment=Sea Lion's icons, with Breeze's dark set for the "
                   "one-colour ones; made by make_dark_theme.py", index, count=1, flags=re.M)
    index = re.sub(r"^Inherits=.*$", f"Inherits={ICON_PARENTS}", index, count=1, flags=re.M)
    (icon_dir / "index.theme").write_text(index)


def change_lock(theme, prokit):
    theme.set("lock", "text_color", f'"{prokit.colour("windowFrameTextColor")}"',
              "PROKIT, windowFrameTextColor: the name on the now dark box.")


def change_icons(theme):
    theme.set("icons", "theme", f'"{ICON_THEME}"',
              "Sea Lion's icons, with Breeze's dark set for the one-colour icons (see the generator).")
    # The pointers come with the linked folders; only this theme's share/ is
    # searched, so they are named by this theme's name too.
    theme.set("cursors", "theme", f'"{ICON_THEME}"', "Sea Lion's pointers, through the linked cursors folder.")


LINKED = ["fonts", "wallpapers"]


def build(work_dir, light_dir, output_dir):
    prokit = ProKit(work_dir / "artwork" / "prokit")
    if not (light_dir / "theme.toml").is_file():
        fail(f"{light_dir} has no theme.toml; build the Sea Lion theme first")

    # A .git in the output folder is kept; everything else is written anew.
    output_dir.mkdir(parents=True, exist_ok=True)
    for child in output_dir.iterdir():
        if child.name == ".git":
            continue
        if child.is_dir() and not child.is_symlink():
            shutil.rmtree(child)
        else:
            child.unlink()
    shutil.copytree(light_dir / "images", output_dir / "images")
    for name in LINKED:
        # Relative, so the pair of themes can move together.
        (output_dir / name).symlink_to(Path("..") / light_dir.name / name)

    make_icon_theme(light_dir, output_dir)
    images_dir = output_dir / "images"
    make_button_pictures(prokit, images_dir / "controls")
    make_indicator_pictures(prokit, images_dir / "controls")
    make_other_control_pictures(prokit, images_dir / "controls")
    make_panel_pictures(prokit, images_dir, light_dir / "images")
    make_inactive_button_pictures(images_dir)

    theme = ThemeText((light_dir / "theme.toml").read_text())
    theme.preamble = [
        "# The Sea Lion Dark theme, made by the Theme Installer from the Sea Lion theme",
        "# and the dark look of 2013's professional programs.",
        "",
    ]
    change_theme_header(theme)
    change_window(theme, prokit)
    change_panel(theme, prokit)
    change_menu(theme, prokit)
    change_dock(theme)
    change_controls(theme, prokit)
    change_dialog_and_about(theme)
    change_desktop(theme)
    change_programs(theme)
    change_banner(theme, prokit)
    change_icons(theme)
    change_lock(theme, prokit)
    (output_dir / "theme.toml").write_text(theme.text())


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("work_dir", type=Path, help="the work folder, with artwork/prokit decoded")
    parser.add_argument("light_dir", type=Path, help="the generated Sea Lion theme")
    parser.add_argument("output_dir", type=Path, help="where the dark theme is written")
    arguments = parser.parse_args()
    build(arguments.work_dir, arguments.light_dir, arguments.output_dir)
    print(f"make_dark_theme: wrote {arguments.output_dir}")


if __name__ == "__main__":
    main()
