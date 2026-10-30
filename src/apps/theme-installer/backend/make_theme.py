"""Builds the Sea Lion theme from the installer's own files.

    python make_theme.py WORK OUTPUT/sea-lion

WORK holds the unpacked installer under root/, and the decoded artwork and
the colour table, which must exist already:

    WORK/artwork/artfile-1x, artfile-2x   (artfile.py)
    WORK/artwork/sartfile                 (sartfile.py)
    WORK/coreui-colors.json               (coreui_colors.py)
"""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

# --- Window buttons -------------------------------------------------------------
# Picture numbers in ArtFile.bin. The file's state names for the theme's states:
# normal "active", hover "rollover", pressed "pressed", inactive "inactive",
# disabled "disabled" (a button that does nothing, as the zoom button of
# System Preferences, which cannot change its size).
BUTTON_PICTURES = {
    "close":    {"normal": 2454, "hover": 2504, "pressed": 2501, "inactive": 2496, "disabled": 2493},
    "minimize": {"normal": 2467, "hover": 2586, "pressed": 2583, "inactive": 2573, "disabled": 2567},
    # The file calls this button "zoom"; the theme format calls it "maximize".
    "maximize": {"normal": 2470, "hover": 2594, "pressed": 2591, "inactive": 2576, "disabled": 2570},
}

# --- Title bar ------------------------------------------------------------------
TITLEBAR_HEIGHT = 22
FIRST_BUTTON_INSET = 7       # from the window's left edge to the first button cell
BUTTON_CELL_WIDTH = 14
BUTTON_CELL_GAP = 6

# Colour numbers in CoreUI's colour table, each with its place along the
# gradient: 1.0 is the top of the bar, 0.0 the bottom.
ACTIVE_GRADIENT = [(234, 1.0), (235, 0.5), (236, 0.25), (237, 0.0)]
INACTIVE_GRADIENT = [(238, 1.0), (239, 0.0)]

CORNER_RADIUS = 4

# A grey noise texture (shared art entry 112, 128x128) tiled over the
# gradient in "overlay" blend mode at this opacity.
NOISE_SHARED_ART_ENTRY = 112
NOISE_OPACITY = 0.3

# The dark line under the bar. The light line CoreUI also has (241, 243)
# shows only on windows with a toolbar, and our frame is a plain window.
SEPARATOR_ACTIVE = [240]
SEPARATOR_INACTIVE = [242]

# The bar's lighter first row: white at 30 % over the gradient's first colour.
TOP_LINE = "#ffffff4d"

# An inactive window's title is black at this opacity.
TITLE_COLOR = "#000000"
TITLE_INACTIVE_OPACITY = 0.63

# The system's interface face in LucidaGrande.ttc.
TITLE_FONT_FAMILY = ".Lucida Grande UI"

# The App Store, as OS X 10.9.5's draws it (measured from its window): a grey
# toolbar that carries on the title bar (sea-lion.toml joins the two), a
# white page, the blue count of updates, grey GET pills that turn into the
# green Install App, and grey Information labels. The toolbar starts at the
# colour the joined title bar ends with (background_joined, #d1d1d1 and
# #eeeeee) and ends at 10.9's.
#
# APP_STORE_TABLE needs no picture from the installer, so updates.toml carries
# it, with APP_STORE_STAND_INS, to themes imported before the App Store
# existed. An import gets APP_STORE_PICTURES instead: AppKit's own toolbar
# controls (CONTROL_PICTURES) and glyphs (make_toolbar_glyphs).
APP_STORE_TABLE = """[app_store]
width = 1000
height = 650
background = "#ffffff"
text_color = "#000000"
heading_color = "#53565e"
secondary_text_color = "#787878"
failure_text_color = "#c03030"
separator_color = "#d7d7d7"
side_margin = 20
toolbar_height = 33
toolbar = { gradient = ["#d0d0d0", "#afafaf"] }
toolbar_inactive = { gradient = ["#ededed", "#dfdfdf"] }
toolbar_border_color = "#6b6b6b"
toolbar_border_color_inactive = "#a7a7a7"
toolbar_margin = 8
toolbar_controls_top = 2
back_forward_size = [53, 23]
arrow_color = "#2d2d2d"
search_field_size = [148, 23]
search_text_color = "#000000"
tab_height = 22
tab_padding = 10
tab_text_color = "#424242"
tab_selected_text_color = "#000000"
tab_selected = { gradient = ["#00000020", "#00000010"], radius = 4 }
badge = { color = "#00000000" }
badge_text_color = "#034ac9"
heading_height = 30
tile_width = 245
tile_height = 108
tile_icon_size = 64
page_icon_size = 128
information_width = 190
screenshot_height = 340
row_height = 64
row_icon_size = 40
button_height = 22
small_button_height = 15
button_padding = 10
button = { gradient = ["#b5b7bb", "#8d8e90"], radius = 3 }
button_pressed = { gradient = ["#8d8e90", "#75767a"], radius = 3 }
button_text_color = "#ffffff"
button_confirm = { gradient = ["#6dd640", "#3eab08"], radius = 3 }
button_confirm_pressed = { gradient = ["#4fb823", "#2f8a05"], radius = 3 }
button_confirm_text_color = "#ffffff"
progress_track = { gradient = ["#cfd0d3", "#e9eaec"], radius = 3 }
progress_bar = { gradient = ["#6fa3ec", "#2e65b6"], radius = 3 }
"""
APP_STORE_PICTURES = """back_forward = { image = "images/controls/toolbar-segments.png", slice = [0, 5, 0, 5] }
back_forward_pressed = { image = "images/controls/toolbar-segments-pressed.png", slice = [0, 5, 0, 5] }
back_forward_inactive = { image = "images/controls/toolbar-segments-inactive.png", slice = [0, 5, 0, 5] }
back_forward_separator = { image = "images/controls/toolbar-segments-separator.png" }
back_forward_separator_width = 1
back_forward_separator_inset = [1, 2]
back_glyph = "images/app_store/back.png"
back_glyph_disabled = "images/app_store/back-disabled.png"
forward_glyph = "images/app_store/forward.png"
forward_glyph_disabled = "images/app_store/forward-disabled.png"
search_field = { image = "images/controls/toolbar-search-field.png", slice = [0, 11, 0, 11] }
search_icon = "images/app_store/search.png"
"""
# What a theme from before the App Store gets in place of APP_STORE_PICTURES,
# until it's imported again: the same shapes, drawn without pictures.
APP_STORE_STAND_INS = """back_forward = { gradient = ["#fdfdfd", "#dadada"], radius = 4, border = { color = "#8c8c8c", width = 1 } }
back_forward_pressed = { gradient = ["#c8c8c8", "#b4b4b4"], radius = 4, border = { color = "#7c7c7c", width = 1 } }
back_forward_inactive = { gradient = ["#fbfbfb", "#efefef"], radius = 4, border = { color = "#b8b8b8", width = 1 } }
back_forward_separator = { color = "#9a9a9a" }
back_forward_separator_width = 1
back_forward_separator_inset = [1, 1]
search_field = { color = "#ffffff", radius = 11, border = { color = "#9a9a9a", width = 1 } }
"""

# How menus go away, measured on 10.9: a chosen item blinks once, then the
# menu fades; a menu closed by a press elsewhere or by Escape just fades.
# updates.toml carries it to themes imported before menus faded.
MENU_CLOSE_TABLE = """[animation.menu_close]
kind = "fade"
duration_ms = 130
blink_ms = 60
"""
TITLE_FONT_SIZE = 13


def fail(message):
    sys.exit(f"make_theme: {message}")


def load_json(path):
    if not path.is_file():
        fail(f"{path} is missing. Run the decoding tools first (see this file's header).")
    with open(path) as file:
        return json.load(file)


def copy_button_pictures(artwork_dir, images_dir):
    """Copies the original button pictures into the theme, unchanged.

    Returns the picture size at scale 1 as (width, height).
    """
    index_1x = {image["index"]: image for image in load_json(artwork_dir / "artfile-1x/index.json")["images"]}
    index_2x = {image["index"]: image for image in load_json(artwork_dir / "artfile-2x/index.json")["images"]}

    sizes = set()
    for button, states in BUTTON_PICTURES.items():
        for state, number in states.items():
            for index, source_dir, suffix in (
                (index_1x, "artfile-1x", ""),
                (index_2x, "artfile-2x", "@2x"),
            ):
                source = artwork_dir / source_dir / index[number]["file"]
                shutil.copyfile(source, images_dir / f"{button}-{state}{suffix}.png")
            sizes.add((index_1x[number]["width"], index_1x[number]["height"]))

    if len(sizes) != 1:
        fail(f"the button pictures are not all the same size: {sorted(sizes)}")
    return sizes.pop()


def copy_noise_picture(artwork_dir, images_dir):
    """Copies the title bar's noise texture into the theme, unchanged."""
    for suffix, theme_suffix in (("1x", ""), ("2x", "@2x")):
        source = artwork_dir / "sartfile" / f"{NOISE_SHARED_ART_ENTRY:03d}-{suffix}.png"
        if not source.is_file():
            fail(f"{source} is missing. Decode SArtFile.bin first.")
        shutil.copyfile(source, images_dir / f"titlebar-noise{theme_suffix}.png")


# --- Panel (the menu bar) ------------------------------------------------------
# Shared art entries: the translucent background strip and the logo in black.
PANEL_BACKGROUND_ENTRY = 48
PANEL_LOGO_ENTRY = 11
# The strip is 21 rows and covers all but one row of the bar.
PANEL_HEIGHT = 22
PANEL_FONT_SIZE = 14
# Shared art: the strip behind an open menu's title, and the logo in white.
PANEL_TITLE_HIGHLIGHT_ENTRY = 49
PANEL_LOGO_HIGHLIGHTED_ENTRY = 12


# --- Menus ---------------------------------------------------------------------
# Shared art: the strip behind the selected item.
MENU_HIGHLIGHT_ENTRY = 107
# The menu font is nominally 13, but item text on screen has the height of 14.
MENU_FONT_SIZE = 14
MENU_PADDING = 4
MENU_ITEM_HEIGHT = 19
MENU_TEXT_BASELINE_ROW = 14        # the baseline is on this row of the item
MENU_TEXT_INSET = 21
MENU_SEPARATOR_HEIGHT = 12
MENU_SEPARATOR_LINE_ROW = 7
MENU_CORNER_RADIUS = 5
MENU_SHORTCUT_END_INSET = 11


# --- Dock ----------------------------------------------------------------------
# The shelf is a slab drawn by the shell, not a picture.
DOCK_SHELF_SLANT = 0.6                 # the ends move out by this per row
DOCK_SHELF_FACE_HEIGHT = 3
DOCK_SHELF_LINE_HEIGHT = 1
DOCK_SHELF_BACK_CORNER_RADIUS = 3
# The front corner's size is 4 + 8 * (height - 32) / 96: 4 at a shelf 32
# high, 12 at one 128 high.
DOCK_SHELF_FRONT_CORNER_HEIGHTS = [32, 128]
DOCK_SHELF_FRONT_CORNER_SIZES = [4, 12]
DOCK_SHELF_CLEAR_ABOVE = 1
DOCK_SHELF_CLEAR_BESIDE = 1
# (grey, opacity) pairs, from the front of the shelf to the back.
DOCK_SHELF_SURFACE = [(1.0, 0.73), (0.81, 0.65), (0.61, 0.56)]
DOCK_SHELF_FACE = [(0.55, 0.80), (0.79, 0.86)]
DOCK_SHELF_LINE = (1.0, 0.65)
DOCK_SHELF_OUTLINE = (0.0, 0.12)
DOCK_SHELF_OUTLINE_WIDTH = 1
# The greys above are "generic grey" values (gamma 1.8); on a display of
# gamma 2.2 they show as grey ^ (1.8 / 2.2).
GENERIC_GREY_GAMMA = 1.8
DISPLAY_GAMMA = 2.2
# Below, T is the tile size. The icon size is 52 on displays up to 800 high.
DOCK_ICON_SIZE = 64
DOCK_COMPACT_ICON_SIZE = 52
DOCK_COMPACT_UP_TO_HEIGHT = 800
DOCK_TILE_MARGIN = 1                    # a tile is the icon plus 2
DOCK_HEADROOM = 6                       # space kept free above the tiles
DOCK_BASE = [0.2, 3]                    # space under the tiles: 0.2 T + 3
DOCK_END_PADDING = [0.475, 0]           # shelf beyond the row: 0.475 T
DOCK_ROW_SHIFT = 2                      # the row starts 2 right of centred
DOCK_END_TRIM = 4                       # and the shelf's right end is 4 short
DOCK_SHELF_HEIGHT_ABOVE_BASE = [0.5, 3] # shelf: base + T / 2 + 3
DOCK_FIT_SPARE_TILES = 2                # shrink if T (n + 2) > width + 12
DOCK_FIT_OVERHANG = 12
DOCK_SEPARATOR_WIDTH = 0.5              # in tiles
DOCK_LARGEST_ICON_SIZE = 128            # the `largesize` setting's default
DOCK_MAGNIFICATION_REACH = 3            # in tiles, each side of the pointer
# Ramp of the magnification: floor(1 + 42 ln(1 + 0.5 change)) milliseconds.
DOCK_MAGNIFICATION_RAMP_MS = [1, 42]
DOCK_PRESSED_DARKEN = 0.4               # how much darker a pressed tile is

DOCK_RESOURCES = "System/Library/CoreServices/Dock.app/Contents/Resources"
DOCK_PICTURES = {
    "indicator_medium": "dock-indicator",
    "indicator_small": "dock-indicator-small",
    "separator": "dock-separator",
    "trashempty": "dock-trash-empty",
    "trashfull": "dock-trash-full",
    # A folder's grid box in pieces ("bt" is the bottom edge with the
    # pointer), the entry that opens the folder, and the open folder's tile.
    "stackbackground-tl": "dock-folder-box-tl",
    "stackbackground-tm": "dock-folder-box-tm",
    "stackbackground-tr": "dock-folder-box-tr",
    "stackbackground-l": "dock-folder-box-l",
    "stackbackground-c": "dock-folder-box-c",
    "stackbackground-r": "dock-folder-box-r",
    "stackbackground-bl": "dock-folder-box-bl",
    "stackbackground-bm": "dock-folder-box-bm",
    "stackbackground-br": "dock-folder-box-br",
    "stackbackground-bt": "dock-folder-box-bt",
    "openinfinder": "dock-folder-open",
    # The puff of smoke where a tile is dropped: five frames, stacked.
    "poof": "dock-poof",
    "pileArrow": "dock-folder-tile-open",
    # Back to the folder before, in a folder opened inside a grid.
    "back-button": "dock-folder-back",
    "back-button-pressed": "dock-folder-back-pressed",
}
# The thin scroll bar a folder's grid shows while it scrolls, as three
# pieces (only at single density), stacked into one picture that stretches
# along its middle row.
DOCK_SCROLL_THUMB_PIECES = ["ecsbl_thin_thumb_top", "ecsbl_thin_thumb_center", "ecsbl_thin_thumb_bottom"]
DOCK_INDICATOR_SMALL_BELOW_TILE_SIZE = 28
DOCK_INDICATOR_CENTER_HEIGHT = 6
DOCK_SEPARATOR_BOTTOM = 8
DOCK_SEPARATOR_TOP_GAP = 5
DOCK_REFLECTION_OPACITY = 0.315
# The reflection starts this far above the icon picture's lower edge.
DOCK_REFLECTION_OVERLAP = 5
DOCK_REFLECTION_CLIP_BOTTOM = 3
# As a share of the icon's size, at the reflection's foot (the only part that shows).
DOCK_REFLECTION_BLUR = 6 / 128
DOCK_REFLECTION_CONTRAST = 1.45
DOCK_REFLECTION_BRIGHTNESS = 0.2
DOCK_FLOOR_SHADOW = (0.0, 0.25)
DOCK_ICON_SHADOW = (0.0, 0.28)
DOCK_ICON_SHADOW_BLUR = 7
DOCK_ICON_SHADOW_RAISE = 1
# Launch bounce: a parabola, 350 ms up and 350 ms down, 6 + 0.2 (S - 8) high.
DOCK_BOUNCE_HALF_PERIOD_MS = 350
DOCK_BOUNCE_HEIGHT = [0.2, 4.4]
DOCK_BOUNCE_LONGEST_MS = 120000
DOCK_TILE_SLIDE_MS = 270
DOCK_TILE_APPEAR_MS = 350

# --- Controls inside windows (buttons, checkboxes...) ------------------------
# Theme picture name -> the tags of the original control picture.
CONTROL_PICTURES = {
    "button": "pushbutton-clear-regular",
    "button-pressed": "pushbutton-clear-pressed-regular",
    "button-disabled": "pushbutton-clear-disabled-regular",
    "button-default": "pushbutton-blue-regular",
    "button-default-pressed": "pushbutton-blue-pressed-regular",
    # The App Store's toolbar: the ◀ ▶ capsule, its pressed half, the line
    # between the halves, and the round search field made for toolbars.
    # "active" is the one a window in front draws; the plain tag is darker.
    "toolbar-segments": "tbsegments-active-regular",
    "toolbar-segments-pressed": "tbsegments-pressed-regular",
    "toolbar-segments-inactive": "tbsegments-inactive-regular",
    "toolbar-segments-separator": "tbsegments-active-regular-separator",
    "toolbar-search-field": "textfieldround-active-fortoolbars-regular",
    # The small size, which the About window's buttons have.
    "button-small": "pushbutton-clear-small",
    "button-small-pressed": "pushbutton-clear-pressed-small",
    "button-small-disabled": "pushbutton-clear-disabled-small",
    "checkbox-off": "checkbox-active-clear-off-regular",
    "checkbox-off-pressed": "checkbox-clear-off-pressed-regular",
    "checkbox-off-disabled": "checkbox-clear-disabled-off-regular",
    "checkbox-on": "checkbox-active-blue-onchecked-regular",
    "checkbox-on-pressed": "checkbox-blue-onchecked-pressed-regular",
    "checkbox-on-disabled": "checkbox-clear-disabled-onchecked-regular",
    "checkbox-mixed": "checkbox-active-blue-mixed-regular",
    "radio-off": "radiobutton-active-clear-off-regular",
    "radio-off-pressed": "radiobutton-clear-off-pressed-regular",
    "radio-off-disabled": "radiobutton-clear-disabled-off-regular",
    "radio-on": "radiobutton-active-blue-on-regular",
    "radio-on-pressed": "radiobutton-blue-on-pressed-regular",
    "radio-on-disabled": "radiobutton-clear-disabled-on-regular",
    "text-field": "textfield-active-regular",
    "text-field-disabled": "textfield-disabled-regular",
    "progress-track": "progressindicator-horizontal-regular-track",
    "progress-fill": "progressindicator-activefill-blue-determinate-horizontal-regular",
    "slider-track": "sliders-active-horizontal-regular-track",
    "slider-thumb": "sliders-active-clear-none-regular-thumb",
    "slider-thumb-pressed": "sliders-clear-none-pressed-regular-thumb",
    "slider-thumb-disabled": "sliders-clear-disabled-none-regular-thumb",
    "tab": "tab-clear-horizontal-regular",
    "tab-pressed": "tab-clear-horizontal-pressed-regular",
    "tab-selected": "tab-clear-horizontal-on-regular",
    "tab-selected-pressed": "tab-clear-horizontal-on-pressed-regular",
    "tab-disabled": "tab-clear-disabled-horizontal-regular",
    "tab-pane": "tabpane-alpha",
    "popup": "popup-clear-regular-rightsidebutton",
    "popup-pressed": "popup-clear-pressed-regular-rightsidebutton",
    "popup-disabled": "popup-clear-disabled-regular-rightsidebutton",
    # A tool bar's textured rounded buttons ("tbsegments": a lone button is
    # one segment of a segmented control).
    "tool-button": "tbsegments-active-regular",
    "tool-button-pressed": "tbsegments-pressed-regular",
    "tool-button-disabled": "tbsegments-disabled-regular",
}
# The push button's picture (48 by 32) has empty rows, 4 above the body and
# 6 below. The shell's dialogs draw it whole, at its own height.
CONTROL_BUTTON_SLICE = [12, 20, 8, 20]   # top, right, bottom, left
# Toolkits take a picture's edge for the control's edge and centre the label
# between its strips, so for them the empty rows are cut off and the strips
# are the rounded corners alone. Otherwise the button stands too high.
CONTROL_BUTTON_BOX_SLICE = [5, 20, 5, 20]
CONTROL_BUTTON_BOX_PICTURES = ["button", "button-pressed", "button-disabled", "button-default",
                               "button-default-pressed"]
# The small push button: drawn at its own height, it stretches sideways
# only, between end caps of 17 (the art file's columns).
ABOUT_BUTTON_SLICE = [0, 17, 0, 17]
CONTROL_INDICATOR_SIZE = [18, 18]        # checkbox and radio pictures
# The slices below (top, right, bottom, left) keep borders, rounded corners
# and shadow room at their size; the pop-up's right slice keeps its arrows.
CONTROL_TEXT_FIELD_SLICE = [3, 2, 1, 2]
CONTROL_PROGRESS_TRACK_SLICE = [1, 1, 2, 1]
CONTROL_SLIDER_TRACK_SLICE = [2, 3, 2, 3]
CONTROL_TAB_SLICE = [5, 6, 6, 6]
CONTROL_TAB_PANE_SLICE = [4, 4, 4, 4]
CONTROL_POPUP_SLICE = [5, 16, 7, 6]
# With a 15-row label, strips of 4 give the original's 23 rows.
CONTROL_TOOL_BUTTON_SLICE = [4, 6, 4, 6]
CONTROL_POPUP_TEXT_INSET = 12
CONTROL_WINDOW_COLOR = "#ededed"


# --- Status pictures -----------------------------------------------------------
# PDF drawings in pure black whose opacity makes the shape.
MENU_EXTRAS = "System/Library/CoreServices/Menu Extras"
WIFI_PICTURES = "System/Library/PrivateFrameworks/CoreWLANKit.framework/Versions/A/Resources"

# freedesktop.org icon name (without "-symbolic") -> the original picture.
STATUS_PICTURES = {
    "audio-volume-muted": f"{MENU_EXTRAS}/Volume.menu/Contents/Resources/Volume1.pdf",
    "audio-volume-low": f"{MENU_EXTRAS}/Volume.menu/Contents/Resources/Volume2.pdf",
    "audio-volume-medium": f"{MENU_EXTRAS}/Volume.menu/Contents/Resources/Volume3.pdf",
    "audio-volume-high": f"{MENU_EXTRAS}/Volume.menu/Contents/Resources/Volume4.pdf",
    "network-wireless-signal-none": f"{WIFI_PICTURES}/AirPort0.pdf",
    "network-wireless-signal-weak": f"{WIFI_PICTURES}/AirPort1.pdf",
    "network-wireless-signal-ok": f"{WIFI_PICTURES}/AirPort2.pdf",
    "network-wireless-signal-good": f"{WIFI_PICTURES}/AirPort3.pdf",
    "network-wireless-signal-excellent": f"{WIFI_PICTURES}/AirPort4.pdf",
    "network-wireless-offline": f"{WIFI_PICTURES}/AirPortOff.pdf",
    "edit-find": "System/Library/CoreServices/Search.bundle/Contents/Resources/MDSearchMenuIcon.pdf",
    "bluetooth-active": f"{MENU_EXTRAS}/Bluetooth.menu/Contents/Resources/Bluetooth_Idle.pdf",
    "bluetooth-disabled": f"{MENU_EXTRAS}/Bluetooth.menu/Contents/Resources/Bluetooth_Off.pdf",
    "battery-good-charging": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryCharging.pdf",
    "battery-full-charged": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryCharged.pdf",
    "battery-missing": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryNone.pdf",
    "media-eject": f"{MENU_EXTRAS}/Eject.menu/Contents/Resources/Eject.pdf",
}

# An empty outline, and a bar of two ends one point wide and a repeating middle.
BATTERY_PIECES = {
    "outline": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryEmpty.pdf",
    "bar_left": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryLevelCapB-L.pdf",
    "bar_middle": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryLevelCapB-M.pdf",
    "bar_right": f"{MENU_EXTRAS}/Battery.menu/Contents/Resources/BatteryLevelCapB-R.pdf",
}
# Where the bar lies in the outline, in points: one point inside its clear
# space, worked out from the pieces' sizes.
BATTERY_BAR_LEFT = 4
BATTERY_BAR_TOP = 3
BATTERY_BAR_WIDTH = 12


# --- Dialogs -------------------------------------------------------------------
# The shut down, restart and log out dialogs' pictures. Each file holds the
# picture at 256 and at 128 pixels; the dialogs show it at 64.
DIALOG_PICTURES_DIR = "System/Library/CoreServices/loginwindow.app/Contents/Resources"
DIALOG_PICTURES = {
    "system-shutdown": "ShutDown.tiff",
    "system-reboot": "Restart.tiff",
    "system-log-out": "LogOut.tiff",
    "system-suspend": "Sleep.tiff",
}
DIALOG_ICON_SIZE = 64

# The About window's picture, the logo over the system's name, in the same
# folder: one file holding it at 428 by 248 and at 214 by 124.
ABOUT_PICTURE = "MacOSX.tiff"
ABOUT_PICTURE_SIZE = (214, 124)


# --- Notifications -------------------------------------------------------------
# A banner's background for one, two and three lines of text, each file at
# both densities.
NOTIFICATION_PICTURES_DIR = "System/Library/CoreServices/NotificationCenter.app/Contents/Resources"
BANNER_BACKGROUNDS = ["banner1line.tiff", "banner2lines.tiff", "banner3lines.tiff"]
# The panel inside each picture, and the shadow round it.
BANNER_BACKGROUND_WIDTH = 65
BANNER_MARGINS = [11, 15, 19, 15]        # top, right, bottom, left
BANNER_HEIGHTS = [36, 48, 64]
BANNER_SLICE = [0, 22, 0, 22]


# --- Application grid ------------------------------------------------------------
APP_GRID_PICTURES = {
    "search-background": "ectl_search_background.png",
    "search-icon": "ectl_search_loupe.png",
}
# The search field stretches sideways between round ends 13 wide.
APP_GRID_SEARCH_SLICE = [0, 13, 0, 13]

WINDOW_OVERVIEW_PICTURES = {
    "selection": "expose-window-selection-big.png",
    "icon-shadow": "wviconshadow.png",
    "desktop-close": "closebox.png",
    "desktop-close-pressed": "closebox_pressed.png",
    "desktop-add": "expose-plus.png",
}
WINDOW_OVERVIEW_SELECTION_SLICE = [13, 13, 13, 13]


# --- Desktop picture -----------------------------------------------------------
# System/Library/CoreServices/DefaultDesktop.jpg links to this file.
DESKTOP_PICTURES = "Library/Desktop Pictures"
DEFAULT_DESKTOP_PICTURE = "Wave.jpg"
THEME_DESKTOP_PICTURES = "wallpapers"


# --- Typeface ------------------------------------------------------------------
# The theme carries the fonts its [fonts] table names; a user's font folder
# links to them. Helvetica is the notification list's face.
SYSTEM_FONT_FILE = "System/Library/Fonts/LucidaGrande.ttc"
THEME_FONTS = "fonts"
HELVETICA_FILE = "System/Library/Fonts/Helvetica.dfont"


def copy_typeface(root_dir, fonts_dir):
    """Copies the system's font files into the theme, unchanged."""
    fonts_dir.mkdir(parents=True, exist_ok=True)
    for name in (SYSTEM_FONT_FILE, HELVETICA_FILE):
        font_file = root_dir / name
        if not font_file.is_file():
            fail(f"{font_file} is missing")

        shutil.copyfile(font_file, fonts_dir / font_file.name)


def copy_desktop_pictures(source_dir, pictures_dir):
    """Copies the system's desktop pictures into the theme, unchanged."""
    if not (source_dir / DEFAULT_DESKTOP_PICTURE).is_file():
        fail(f"{source_dir / DEFAULT_DESKTOP_PICTURE} is missing")

    pictures_dir.mkdir(parents=True, exist_ok=True)
    for picture in sorted(source_dir.glob("*.jpg")):
        shutil.copyfile(picture, pictures_dir / picture.name)


def copy_shared_art(artwork_dir, images_dir, entry, name):
    """Copies one shared art entry, at both sizes, into the theme as NAME."""
    for suffix, theme_suffix in (("1x", ""), ("2x", "@2x")):
        source = artwork_dir / "sartfile" / f"{entry:03d}-{suffix}.png"
        if not source.is_file():
            fail(f"{source} is missing. Decode SArtFile.bin first.")
        shutil.copyfile(source, images_dir / f"{name}{theme_suffix}.png")


def hex_with_opacity(color):
    """A colour of CoreUI's table as #rrggbbaa."""
    return color["hex"] + f"{round(color['alpha'] * 255):02x}"


def color_list(numbers, colors):
    return "[" + ", ".join(f'"{hex_with_opacity(colors[number])}"' for number in numbers) + "]"


def gradient_for_theme(stops, colors):
    """Turns CoreUI's stops into the theme's form.

    CoreUI counts positions from the bottom of the bar (0.0) to the top
    (1.0). The theme format counts from the top (0.0) to the bottom (1.0),
    so each position is flipped, and the stops are listed top first.
    """
    flipped = sorted((1.0 - position, colors[number]["hex"]) for number, position in stops)
    gradient_colors = ", ".join(f'"{color}"' for _, color in flipped)
    positions = ", ".join(f"{position:g}" for position, _ in flipped)
    overlay = (
        f'overlay = {{ image = "images/titlebar-noise.png", '
        f'opacity = {NOISE_OPACITY}, blend = "overlay" }}'
    )
    return f"{{ gradient = [{gradient_colors}], positions = [{positions}], {overlay} }}"


def copy_dock_pictures(resources_dir, images_dir):
    """Copies the Dock's own pictures, at both densities, unchanged."""
    for original_name, theme_name in DOCK_PICTURES.items():
        for suffix in ("", "@2x"):
            source = resources_dir / f"{original_name}{suffix}.png"
            if not source.is_file():
                fail(f"{source} is missing")
            shutil.copyfile(source, images_dir / f"{theme_name}{suffix}.png")


def join_scroll_thumb(resources_dir, images_dir):
    """Stacks the scroll bar's pieces into dock-folder-scroll-thumb.png."""
    pieces = []
    for name in DOCK_SCROLL_THUMB_PIECES:
        source = resources_dir / f"{name}.png"
        if not source.is_file():
            fail(f"{source} is missing")
        pieces.append(Image.open(source).convert("RGBA"))
    joined = Image.new("RGBA", (max(piece.width for piece in pieces), sum(piece.height for piece in pieces)))
    top = 0
    for piece in pieces:
        joined.paste(piece, (0, top))
        top += piece.height
    joined.save(images_dir / "dock-folder-scroll-thumb.png")


def copy_control_pictures(artwork_dir, images_dir):
    """Copies the control pictures named in CONTROL_PICTURES, at both densities.

    The decoded pictures are named "NUMBER-tags.png" and found by their tags.
    """
    controls_dir = images_dir / "controls"
    controls_dir.mkdir(parents=True, exist_ok=True)
    for theme_name, tags in CONTROL_PICTURES.items():
        for folder, suffix in (("artfile-1x", ""), ("artfile-2x", "@2x")):
            matches = list((artwork_dir / folder).glob(f"*-{tags}.png"))
            # "pushbutton-clear-regular" must not also match "...-inset-...-regular".
            matches = [path for path in matches if path.name.split("-", 1)[1] == f"{tags}.png"]
            if len(matches) != 1:
                fail(f"expected one picture tagged {tags} in {artwork_dir / folder}, found {len(matches)}")
            shutil.copyfile(matches[0], controls_dir / f"{theme_name}{suffix}.png")


# --- The App Store's toolbar glyphs ----------------------------------------------
# Shared art entries: AppKit's ◀ and ▶ (the "go left" and "go right" templates)
# and the search field's magnifier. They are black shapes; AppKit paints them
# as it draws them. Measured in the reference VM's App Store toolbar: a
# usable arrow runs from TOOLBAR_GLYPH_TOP at its top to TOOLBAR_GLYPH_BOTTOM
# at its bottom, with a light copy one point lower as an embossed edge; one
# with nowhere to go is lighter; the magnifier is one grey.
TOOLBAR_GLYPH_ENTRIES = {"back": 127, "forward": 128, "search": 295}
TOOLBAR_GLYPH_TOP = (0x2d, 0x2d, 0x2d)
TOOLBAR_GLYPH_BOTTOM = (0x80, 0x80, 0x80)
TOOLBAR_GLYPH_DISABLED_TOP = (0x70, 0x70, 0x70)
TOOLBAR_GLYPH_DISABLED_BOTTOM = (0xa8, 0xa8, 0xa8)
TOOLBAR_GLYPH_EMBOSS = (255, 255, 255, 110)
SEARCH_ICON_COLOR = (0x74, 0x74, 0x74)


def paint_glyph(shape, top, bottom, emboss=None, density=1):
    """The template `shape` filled from colour `top` to `bottom`, with an
    `emboss` colour showing one point under it, if given."""
    shape = shape.convert("RGBA")
    width, height = shape.size
    extra = density if emboss else 0
    painted = Image.new("RGBA", (width, height + extra), (0, 0, 0, 0))
    if emboss:
        light = Image.new("RGBA", shape.size, emboss[:3] + (0,))
        light.putalpha(shape.getchannel("A").point(lambda alpha: alpha * emboss[3] // 255))
        painted.alpha_composite(light, (0, density))
    gradient = Image.new("RGBA", shape.size)
    for y in range(height):
        share = y / max(1, height - 1)
        colour = tuple(round(a + (b - a) * share) for a, b in zip(top, bottom))
        gradient.paste(colour + (255,), (0, y, width, y + 1))
    gradient.putalpha(shape.getchannel("A"))
    painted.alpha_composite(gradient, (0, 0))
    return painted


def make_toolbar_glyphs(artwork_dir, images_dir):
    """Writes images/app_store/: back, forward and their -disabled forms, and search."""
    target = images_dir / "app_store"
    target.mkdir(parents=True, exist_ok=True)
    for suffix, theme_suffix, density in (("1x", "", 1), ("2x", "@2x", 2)):
        shapes = {}
        for name, entry in TOOLBAR_GLYPH_ENTRIES.items():
            source = artwork_dir / "sartfile" / f"{entry:03d}-{suffix}.png"
            if not source.is_file():
                fail(f"{source} is missing. Decode SArtFile.bin first.")
            shapes[name] = Image.open(source)
        for name in ("back", "forward"):
            paint_glyph(shapes[name], TOOLBAR_GLYPH_TOP, TOOLBAR_GLYPH_BOTTOM, TOOLBAR_GLYPH_EMBOSS,
                        density).save(target / f"{name}{theme_suffix}.png")
            paint_glyph(shapes[name], TOOLBAR_GLYPH_DISABLED_TOP, TOOLBAR_GLYPH_DISABLED_BOTTOM,
                        TOOLBAR_GLYPH_EMBOSS, density).save(target / f"{name}-disabled{theme_suffix}.png")
        paint_glyph(shapes["search"], SEARCH_ICON_COLOR, SEARCH_ICON_COLOR).save(target / f"search{theme_suffix}.png")


def trim_toolbar_separator(images_dir):
    """Keeps only the dark line of the ◀ ▶ separator. AppKit draws the picture
    between two segments, with their border rows and light edges; ours goes
    over one capsule that has its own, so those would show twice."""
    for suffix, density in (("", 1), ("@2x", 2)):
        file = images_dir / "controls" / f"toolbar-segments-separator{suffix}.png"
        picture = Image.open(file)
        middle = picture.width // 2 - density // 2
        picture.crop((middle, density, middle + density, picture.height - 2 * density)).save(file)


def cut_button_boxes(images_dir):
    """Writes CONTROL_BUTTON_BOX_PICTURES: the push buttons without their empty rows."""
    controls_dir = images_dir / "controls"
    for suffix in ("", "@2x"):
        plain = Image.open(controls_dir / f"button{suffix}.png").convert("RGBA")
        first_row, last_row = plain.getchannel("A").getbbox()[1::2]
        for name in CONTROL_BUTTON_BOX_PICTURES:
            picture = Image.open(controls_dir / f"{name}{suffix}.png").convert("RGBA")
            picture.crop((0, first_row, picture.width, last_row)).save(controls_dir / f"{name}-box{suffix}.png")


def convert_status_picture(pdf_file, target, density):
    """Draws one of the original status pictures, stored as PDF, as a PNG.

    One PDF point is one pixel at density 1, so 72 dpi times the density.
    """
    if not pdf_file.is_file():
        fail(f"{pdf_file} is missing")
    result = subprocess.run(
        ["pdftocairo", "-png", "-transp", "-singlefile", "-r", str(72 * density),
         str(pdf_file), str(target.with_suffix(""))],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        fail(f"pdftocairo could not convert {pdf_file}: {result.stderr.strip()}")


# The on-screen volume display's loudspeaker, white, as PDF.
VOLUME_DISPLAY_DIR = "System/Library/LoginPlugins/BezelServices.loginPlugin/Contents/Resources/BezelUI/HiDPI"
VOLUME_DISPLAY_PICTURES = {"volume": "Volume.pdf", "muted": "Mute.pdf"}


def make_volume_display_pictures(root, images_dir):
    """Writes the volume display's two pictures, at both densities."""
    for name, source in VOLUME_DISPLAY_PICTURES.items():
        for density, suffix in ((1, ""), (2, "@2x")):
            convert_status_picture(root / VOLUME_DISPLAY_DIR / source,
                                   images_dir / f"volume-display-{name}{suffix}.png", density)


def make_status_pictures(root, images_dir):
    """Writes the panel's status pictures; returns {icon name: theme path}."""
    status_dir = images_dir / "status"
    status_dir.mkdir(parents=True, exist_ok=True)
    icons = {}
    for icon_name, source in STATUS_PICTURES.items():
        file_name = Path(source).stem.lower().replace("_", "-") + ".png"
        for density, suffix in ((1, ""), (2, "@2x")):
            target = status_dir / file_name.replace(".png", f"{suffix}.png")
            if not target.is_file():
                convert_status_picture(root / source, target, density)
        icons[icon_name] = f"images/status/{file_name}"

    # The battery at each level is put together from pieces.
    pieces = {}
    for piece, source in BATTERY_PIECES.items():
        for density, suffix in ((1, ""), (2, "@2x")):
            target = status_dir / f"battery-piece-{piece}{suffix}.png"
            convert_status_picture(root / source, target, density)
            pieces[(piece, density)] = Image.open(target).convert("RGBA")
            target.unlink()
    left, top, width = BATTERY_BAR_LEFT, BATTERY_BAR_TOP, BATTERY_BAR_WIDTH
    for level in range(0, 101, 10):
        bar_width = round(width * level / 100)
        for density, suffix in ((1, ""), (2, "@2x")):
            picture = pieces[("outline", density)].copy()
            cap_width = pieces[("bar_left", density)].width
            bar_pixels = bar_width * density
            for column in range(0, bar_pixels, cap_width):
                if column == 0:
                    piece = "bar_left"
                elif column + cap_width >= bar_pixels:
                    piece = "bar_right"
                else:
                    piece = "bar_middle"
                picture.alpha_composite(pieces[(piece, density)], (left * density + column, top * density))
            picture.save(status_dir / f"battery-level-{level}{suffix}.png")
        icons[f"battery-level-{level}"] = f"images/status/battery-level-{level}.png"
    return icons


def make_dialog_pictures(root, images_dir):
    """Writes the dialogs' pictures; returns {icon name: theme path}.

    The 128-pixel picture serves dense screens; ordinary ones get it halved.
    """
    dialog_dir = images_dir / "dialog"
    dialog_dir.mkdir(parents=True, exist_ok=True)
    icons = {}
    for icon_name, file_name in DIALOG_PICTURES.items():
        source = root / DIALOG_PICTURES_DIR / file_name
        if not source.is_file():
            fail(f"{source} is missing")
        pictures = Image.open(source)
        dense = None
        for frame in range(getattr(pictures, "n_frames", 1)):
            pictures.seek(frame)
            if pictures.size == (2 * DIALOG_ICON_SIZE, 2 * DIALOG_ICON_SIZE):
                dense = pictures.convert("RGBA")
        if dense is None:
            fail(f"{source} holds no picture of {2 * DIALOG_ICON_SIZE} pixels")
        dense.save(dialog_dir / f"{icon_name}@2x.png")
        dense.resize((DIALOG_ICON_SIZE, DIALOG_ICON_SIZE), Image.LANCZOS).save(dialog_dir / f"{icon_name}.png")
        icons[icon_name] = f"images/dialog/{icon_name}.png"
    return icons


def copy_about_picture(root, images_dir):
    """Writes the About window's picture at both densities, unchanged."""
    source = root / DIALOG_PICTURES_DIR / ABOUT_PICTURE
    if not source.is_file():
        fail(f"{source} is missing")
    pictures = Image.open(source)
    width, height = ABOUT_PICTURE_SIZE
    wanted = {(width, height): "about.png", (2 * width, 2 * height): "about@2x.png"}
    for frame in range(getattr(pictures, "n_frames", 1)):
        pictures.seek(frame)
        if pictures.size in wanted:
            pictures.convert("RGBA").save(images_dir / wanted.pop(pictures.size))
    if wanted:
        fail(f"{source} does not hold the picture at {sorted(wanted)}")


def tiff_frame(source, width):
    """The picture of the given width out of a TIFF file that holds several."""
    if not source.is_file():
        fail(f"{source} is missing")
    pictures = Image.open(source)
    for frame in range(getattr(pictures, "n_frames", 1)):
        pictures.seek(frame)
        if pictures.width == width:
            return pictures.convert("RGBA")
    fail(f"{source} holds no picture {width} wide")


# The notification list's pictures: the original file and the width of its
# density-1 picture (each file also holds one twice as large).
NOTIFICATION_LIST_PICTURES = {
    "list-section": ("headerTile.tiff", 10),
    "list-close": ("close.tiff", 17),
    "list-close-pressed": ("closePressed.tiff", 17),
    "list-unread": ("unread_list.tiff", 12),
    "list-status": ("menuitemNormal.tiff", 27),
}


def copy_notification_list_pictures(root, images_dir):
    """Copies the notification list's pictures, at both densities."""
    target_dir = images_dir / "notifications"
    target_dir.mkdir(parents=True, exist_ok=True)
    for name, (file_name, width) in NOTIFICATION_LIST_PICTURES.items():
        source = root / NOTIFICATION_PICTURES_DIR / file_name
        tiff_frame(source, width).save(target_dir / f"{name}.png")
        tiff_frame(source, 2 * width).save(target_dir / f"{name}@2x.png")


def copy_notification_pictures(root, images_dir):
    """Copies the banners' backgrounds, at both densities; returns their theme paths."""
    target_dir = images_dir / "notifications"
    target_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    for file_name in BANNER_BACKGROUNDS:
        source = root / NOTIFICATION_PICTURES_DIR / file_name
        stem = Path(file_name).stem
        tiff_frame(source, BANNER_BACKGROUND_WIDTH).save(target_dir / f"{stem}.png")
        tiff_frame(source, 2 * BANNER_BACKGROUND_WIDTH).save(target_dir / f"{stem}@2x.png")
        paths.append(f"images/notifications/{stem}.png")
    return paths


def copy_app_grid_pictures(resources_dir, images_dir):
    """Copies the application grid's pictures, at both densities."""
    target_dir = images_dir / "app_grid"
    target_dir.mkdir(parents=True, exist_ok=True)
    for theme_name, file_name in APP_GRID_PICTURES.items():
        for suffix in ("", "@2x"):
            source = resources_dir / file_name.replace(".png", f"{suffix}.png")
            if not source.is_file():
                fail(f"{source} is missing")
            shutil.copyfile(source, target_dir / f"{theme_name}{suffix}.png")


def copy_window_overview_pictures(resources_dir, images_dir):
    """Copies the window overview's pictures, at both densities."""
    target_dir = images_dir / "window_overview"
    target_dir.mkdir(parents=True, exist_ok=True)
    for theme_name, file_name in WINDOW_OVERVIEW_PICTURES.items():
        for suffix in ("", "@2x"):
            source = resources_dir / file_name.replace(".png", f"{suffix}.png")
            if not source.is_file():
                fail(f"{source} is missing")
            shutil.copyfile(source, target_dir / f"{theme_name}{suffix}.png")


def status_icon_lines(icons):
    """The [panel.status.icons] table's lines."""
    return "\n".join(f'"{name}" = "{path}"' for name, path in sorted(icons.items()))


def displayed_grey(grey, opacity):
    """A generic grey (0 to 1, see GENERIC_GREY_GAMMA) as the colour a screen shows."""
    level = round(255 * grey ** (GENERIC_GREY_GAMMA / DISPLAY_GAMMA))
    return f"#{level:02x}{level:02x}{level:02x}{round(255 * opacity):02x}"


def displayed_greys(pairs):
    """Several of them, written as a TOML list."""
    return "[" + ", ".join(f'"{displayed_grey(grey, opacity)}"' for grey, opacity in pairs) + "]"


def button_tables(picture_width):
    """The state lines for each of the three buttons."""
    lines = []
    for button, states in BUTTON_PICTURES.items():
        lines.append(f"[window.buttons.{button}]")
        for state in states:
            lines.append(f'{state:8} = {{ image = "images/{button}-{state}.png" }}')
        lines.append("")
    return "\n".join(lines)


def build_theme_text(colors, picture_size, status_icons, dialog_icons, banner_backgrounds):
    picture_width, picture_height = picture_size

    # The button pictures are wider than their cells and centred on them, so
    # each overhangs its cell by half the difference on both sides.
    overhang = (picture_width - BUTTON_CELL_WIDTH) // 2
    edge_inset = FIRST_BUTTON_INSET - overhang
    spacing = BUTTON_CELL_GAP - 2 * overhang

    banner_background_list = ", ".join(
        '{ image = "' + path + '", slice = ' + str(BANNER_SLICE) + ' }' for path in banner_backgrounds)

    active_gradient = gradient_for_theme(ACTIVE_GRADIENT, colors)
    inactive_gradient = gradient_for_theme(INACTIVE_GRADIENT, colors)

    inactive_title_color = TITLE_COLOR + f"{round(TITLE_INACTIVE_OPACITY * 255):02x}"

    return f"""# The Sea Lion theme, made by the Theme Installer.
# docs/themes/ describes every key.
[theme]
name = "Sea Lion"
description = "The look of OS X 10.9 Mavericks."
format = 0

[fonts]
interface    = {{ family = "{TITLE_FONT_FAMILY}", size = {TITLE_FONT_SIZE} }}
window_title = {{ family = "{TITLE_FONT_FAMILY}", size = {TITLE_FONT_SIZE} }}
panel        = {{ family = "{TITLE_FONT_FAMILY}", size = {PANEL_FONT_SIZE} }}
dock_label   = {{ family = "{TITLE_FONT_FAMILY}", size = 14, bold = true }}
notification_list = {{ family = "Helvetica", size = 13 }}
notification_list_heading = {{ family = "Helvetica", size = 14, bold = true }}
notification_list_time = {{ family = "Helvetica", size = 11, bold = true }}
lock_name    = {{ family = "{TITLE_FONT_FAMILY}", size = 13, bold = true }}
dock_folder  = {{ family = "{TITLE_FONT_FAMILY}", size = 12, bold = true }}
window_overview_label = {{ family = "{TITLE_FONT_FAMILY}", size = 13, bold = true }}
menu         = {{ family = "{TITLE_FONT_FAMILY}", size = {MENU_FONT_SIZE} }}
search         = {{ family = "{TITLE_FONT_FAMILY}", size = 12 }}
search_heading = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
search_label   = {{ family = "{TITLE_FONT_FAMILY}", size = 12, bold = true }}
dialog_title = {{ family = "{TITLE_FONT_FAMILY}", size = 13, bold = true }}
dialog_text  = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
theme_installer_heading = {{ family = "{TITLE_FONT_FAMILY}", size = 15, bold = true }}
theme_installer_text    = {{ family = "{TITLE_FONT_FAMILY}", size = 13 }}
about_version = {{ family = "{TITLE_FONT_FAMILY}", size = 12 }}
about_label   = {{ family = "{TITLE_FONT_FAMILY}", size = 11, bold = true }}
about_value   = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
about_button  = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
about_footer  = {{ family = "{TITLE_FONT_FAMILY}", size = 10 }}
app_store_tab     = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
app_store_heading = {{ family = "{TITLE_FONT_FAMILY}", size = 14 }}
app_store_title   = {{ family = "{TITLE_FONT_FAMILY}", size = 22 }}
app_store_name    = {{ family = "{TITLE_FONT_FAMILY}", size = 12 }}
app_store_text    = {{ family = "{TITLE_FONT_FAMILY}", size = 12 }}
app_store_small   = {{ family = "{TITLE_FONT_FAMILY}", size = 10 }}
app_store_button  = {{ family = "{TITLE_FONT_FAMILY}", size = 9, bold = true }}
program_switcher = {{ family = "{TITLE_FONT_FAMILY}", size = 12, bold = true }}
desktop_icon = {{ family = "{TITLE_FONT_FAMILY}", size = 12, bold = true }}
banner_title = {{ family = "{TITLE_FONT_FAMILY}", size = 13, bold = true }}
banner_text  = {{ family = "{TITLE_FONT_FAMILY}", size = 11 }}
app_grid_label = {{ family = "{TITLE_FONT_FAMILY}", size = 13, bold = true }}

[window.titlebar]
height = {TITLEBAR_HEIGHT}

background          = {active_gradient}
background_inactive = {inactive_gradient}

separator          = {color_list(SEPARATOR_ACTIVE, colors)}
separator_inactive = {color_list(SEPARATOR_INACTIVE, colors)}

highlight          = ["{TOP_LINE}"]
highlight_inactive = ["{TOP_LINE}"]

background_joined = {{ gradient = ["#e8e8e8", "#d1d1d1"], positions = [0, 1], overlay = {{ image = "images/titlebar-noise.png", opacity = {NOISE_OPACITY}, blend = "overlay" }} }}
separator_joined = ["#00000000"]
background_joined_inactive = {{ gradient = ["#f8f8f8", "#eeeeee"], positions = [0, 1], overlay = {{ image = "images/titlebar-noise.png", opacity = {NOISE_OPACITY}, blend = "overlay" }} }}
separator_joined_inactive = ["#00000000"]

title_color          = "{TITLE_COLOR}"
title_color_inactive = "{inactive_title_color}"

title_align = "center"
title_padding = 8

[window.buttons]
size = [{picture_width}, {picture_height}]

spacing = {spacing}
edge_inset = {edge_inset}

hover_together = true

{button_tables(picture_width)}
[window.frame]
border_width = 0
border_color          = "#000000"
border_color_inactive = "#000000"

resize_grip = 4
resize_grip_in_title_bar = 3
resize_corner = 15

corner_radius = {CORNER_RADIUS}
bottom_corner_radius = {CORNER_RADIUS}

rim          = ["#00000026", "#00000042", "#00000054"]
rim_inactive = ["#0000001c", "#0000002b", "#00000029"]

[window.snap_preview]
color = "#5199db40"
border_color = "#5199dbcc"
border_width = 2

[window.shadow]
color = "#000000a3"
blur_radius = 56
offset = [0, 26]
color_inactive = "#00000068"
blur_radius_inactive = 12
offset_inactive = [0, 5]

[panel]
height = {PANEL_HEIGHT}

background = {{ image = "images/panel-background.png" }}

separator = ["#00000000"]

highlight = ["{TOP_LINE}"]

logo = "images/panel-logo.png"

text_color = "#000000"
edge_padding = 17
logo_spacing = 16
item_spacing = 20

app_name_bold = true

shadow = {{ color = "#000000d9", blur = 16 }}

title_highlight = {{ image = "images/panel-title-highlight.png" }}
title_highlight_text_color = "#ffffff"
logo_highlighted = "images/panel-logo-highlighted.png"
logo_highlight_overhang = 7

[panel.status]
spacing = 14
text_gap = 4

[panel.status.gaps]
"preferences-system-notifications" = 9

[panel.status.icons]
{status_icon_lines(status_icons)}

[dialog]
width = 417
title_bar = true
background = "{CONTROL_WINDOW_COLOR}"
text_color = "#000000"
icon_size = 64
icon_inset = [12, 12]
text_left = 93
text_width = 307
title_top = 20
title_line_height = 17
text_gap = 6
text_line_height = 14
button_gap_above = 8
button_inset = [14, 12]
button_padding = 19
button_min_width = 91
button_height = 32
button = {{ image = "images/controls/button.png", slice = {CONTROL_BUTTON_SLICE} }}
button_pressed = {{ image = "images/controls/button-pressed.png", slice = {CONTROL_BUTTON_SLICE} }}
button_default = {{ image = "images/controls/button-default.png", slice = {CONTROL_BUTTON_SLICE} }}
button_default_pressed = {{ image = "images/controls/button-default-pressed.png", slice = {CONTROL_BUTTON_SLICE} }}
button_text_baseline_row = 19
top_share = 0.22

[dialog.icons]
{status_icon_lines(dialog_icons)}

[about]
width = 307
background = "{CONTROL_WINDOW_COLOR}"
text_color = "#000000"
version_color = "#808080"
footer_color = "#5f5f5f"
picture = "images/about.png"
picture_size = [{ABOUT_PICTURE_SIZE[0]}, {ABOUT_PICTURE_SIZE[1]}]
picture_top = 29
version_top = 148
update_button_top = 169
rows_top = 209
row_pitch = 29
row_height = 14
row_gap = 9
side_margin = 20
more_button_gap = 7
footer_gap = 10
footer_pitch = 13
bottom_gap = 13
button_height = 27
button_width = 133
button_padding = 17
button_text_baseline_row = 16
button = {{ image = "images/controls/button-small.png", slice = {ABOUT_BUTTON_SLICE} }}
button_pressed = {{ image = "images/controls/button-small-pressed.png", slice = {ABOUT_BUTTON_SLICE} }}
button_disabled = {{ image = "images/controls/button-small-disabled.png", slice = {ABOUT_BUTTON_SLICE} }}
button_disabled_text_color = "#00000060"

{APP_STORE_TABLE}{APP_STORE_PICTURES}
[theme_installer]
width = 640
height = 500
# The window at login that offers a new import: as wide as System
# Preferences' content.
news_width = 668
news_icon_size = 64
news_item_gap = 4
background = {{ color = "#ececec" }}
sidebar_width = 180
sidebar = {{ color = "#e3e6eb" }}
sidebar_line_color = "#c4c4c4"
steps_top = 36
step_pitch = 30
step_inset = 22
step_marker_size = 10
step_marker_gap = 10
step_text_color = "#7a7a7a"
step_current_text_color = "#000000"
step_marker_waiting = {{ color = "#00000000", radius = 5, border = {{ color = "#a0a0a0", width = 1 }} }}
step_marker_current = {{ color = "#3875d7", radius = 5 }}
step_marker_done = {{ color = "#8e8e8e", radius = 5 }}
content_margin = 22
heading_height = 40
text_color = "#000000"
secondary_text_color = "#6e6e6e"
pane = {{ color = "#ffffff", radius = 3, border = {{ color = "#bdbdbd", width = 1 }} }}
pane_padding = 18
paragraph_gap = 12
summary_mark_size = 44
note = {{ color = "#fff8dc", radius = 3, border = {{ color = "#e3d39a", width = 1 }} }}
drop_well = {{ color = "#f6f6f6", radius = 10, border = {{ color = "#b2b2b2", width = 2 }} }}
drop_well_ready = {{ color = "#edf2fb", radius = 10, border = {{ color = "#3875d7", width = 2 }} }}
drop_well_height = 220
drop_well_icon_size = 64
drop_well_picture = "share/icons/SeaLion/128x128/devices/media-optical.png"
progress_height = 8
progress_track = {{ color = "#dedede", radius = 4 }}
progress_fill = {{ color = "#3875d7", radius = 4 }}
stage_pitch = 24
stage_waiting_color = "#9a9a9a"
stage_running_color = "#3875d7"
stage_done_color = "#3a8a3a"
stage_failed_color = "#c03030"
button_bar_height = 56
disabled_button_opacity = 0.45

[settings.overview]
columns = 8
icon_size = 40
cell_pitch = 82
first_cell_middle = 47
line_pitch = 92
group_bottom = 19
icon_top = 22
label_baseline = 79
label_line_pitch = 14
label_font_size = 12
label_color = "#313131"
stripe = "#ededed"
stripe_alternate = "#e5e5e5"
line_colors = ["#d4d4d4", "#cdcdcd"]
light_line_colors = ["#efefef", "#f4f4f4"]

[program_switcher]
background = {{ color = "#00000059", radius = 22 }}
icon_size = 128
icon_gap = 4
padding = [28, 24, 30, 24]
mark = {{ color = "#00000078", radius = 11, border = {{ color = "#ffffff", width = 3 }} }}
mark_reach = 4
name = {{ color = "#ffffff", height = 18, padding = 12, gap = 8, background = {{ color = "#00000078", radius = 9 }} }}
appear_after_ms = 190

[search]
width = 430
right_inset = 46
header_height = 31
header_background = {{ gradient = ["#647ff5", "#2850f2"] }}
header_line_color = "#173ee8"
header_text_color = "#ffffff"
header_text_shadow_color = "#00000040"
label_gap = 6
heading_width = 120
heading_gap = 8
heading_color = "#8c8c8c"
heading_column_color = "#ffffff"
divider_color = "#dedede"
field_inset = 9
field_right = 6
field_top = 4
field_height = 22
field_text_baseline_row = 16
field = {{ color = "#ffffff", radius = 11, border = {{ width = 1, color = "#354db3" }} }}
field_text_inset = 12
field_text_color = "#000000"
row_height = 20
highlight = {{ gradient = ["#5979ee", "#6283f7", "#2457f6", "#0e46eb"], positions = [0.025, 0.075, 0.925, 0.975] }}
highlight_text_color = "#ffffff"
row_gap = 1
kind_gap = 9
results_padding = 5
icon_size = 16
icon_inset = 9
name_inset = 33

[search.clear_button]
size = 14
right_inset = 4
color = "#b9b9b9"
cross_color = "#ffffff"
cross_length = 8
cross_thickness = 2

[search.box]
background = "#fafafaf8"

[screenshot_flash]
color = "#ffffff"
opacity = 0.6

[animation.screenshot_flash]
duration_ms = 250
curve = "ease-out"

[notifications.list]
width = 300
background = {{ gradient = ["#39393f", "#3e3e45", "#36363c"] }}
edge_shadow = ["#2d2d31", "#2f3035", "#323338", "#35363b", "#38393f", "#3b3c42", "#3e3f45"]
text_color = "#ffffff"
heading_color = "#ffffff80"
heading_height = 21
section_height = 24
section_background = {{ image = "images/notifications/list-section.png", slice = [0, 0, 0, 0] }}
icon_left = 10
text_left = 30
close_image = "images/notifications/list-close.png"
close_pressed_image = "images/notifications/list-close-pressed.png"
close_right = 4
unread_image = "images/notifications/list-unread.png"
unread_left = 11
unread_top = 9
title_top = 2
line_height = 17
time_right = 10
entry_bottom_padding = 6
divider_color = "#00000040"
empty_text_top = 289

# As on 10.9, the desktop slides aside and uncovers the list.
[animation.notification_list]
kind = "push"
duration_ms = 265
curve = "ease-in-out"

[notifications.banner]
width = 320
top_gap = 11
edge_gap = 15
backgrounds = [{banner_background_list}]
heights = {BANNER_HEIGHTS}
background_margins = {BANNER_MARGINS}
icon_size = 32
icon_inset = [8, 6]
text_left = 48
text_right = 12
title_baseline_row = 20
text_baseline_row = 36
text_line_height = 14
title_color = "#4d4d4d"
text_color = "#4d4d4d"
gap = 0

[app_grid]
background_blur = 14
background_veil = "#00000010"
columns = 7
rows = 5
column_pitch = 170
row_pitch = 122
first_row_top = 77
columns_shift = -3
icon_size = 64
label_baseline = 82
label_color = "#ffffff"
label_shadow_color = "#000000a0"

[app_grid.search]
top = 25
width = 250
height = 27
background = {{ image = "images/app_grid/search-background.png", slice = {APP_GRID_SEARCH_SLICE} }}
icon = "images/app_grid/search-icon.png"
icon_inset = [8, 5]
text_left = 28
text_color = "#ffffff"

[app_grid.page_dots]
size = 8
pitch = 15
center_above_bottom = 111
color = "#ffffff"
other_color = "#ffffff30"

[window_overview]
drawn_by = "shell"
backdrop = {{ gradient = ["#46464e", "#37373d"] }}
stage_scale = 0.697
stage_top_share = 0.1775
stage_shadow = {{ color = "#00000080", blur = 14 }}
windows_top_share = 0.206
windows_bottom_share = 0.769
windows_side_margin = 28
most_window_scale = 0.85
group_gap = 50
group_icon_size = 48
group_icon_rise = 5
group_icon_shadow = {{ image = "images/window_overview/icon-shadow.png", size = 167, drop = 8 }}
group_name_gap = 0
label_color = "#ffffff"
label_shadow_color = "#000000b0"
selection = {{ image = "images/window_overview/selection.png", slice = {WINDOW_OVERVIEW_SELECTION_SLICE} }}
selection_reach = 7

[window_overview.strip]
top = 22
picture_scale = 0.1
mark_width = 2
mark_color = "#c8c8c8"
name_gap = 14
slide = 30
picture_gap = 80
close_button = {{ image = "images/window_overview/desktop-close.png", pressed = "images/window_overview/desktop-close-pressed.png", size = 30 }}
close_delay_ms = 800
add_tile = {{ image = "images/window_overview/desktop-add.png", image_size = [24, 23], shown_share = 0.5, opacity = 0.6, slide_ms = 150 }}

[menu]
background = "#fffffff2"
corner_radius = {MENU_CORNER_RADIUS}
padding = {MENU_PADDING}
item_height = {MENU_ITEM_HEIGHT}
text_baseline_row = {MENU_TEXT_BASELINE_ROW}
text_inset = {MENU_TEXT_INSET}
text_color = "#000000"
disabled_text_color = "#00000066"
separator_height = {MENU_SEPARATOR_HEIGHT}
separator_line_row = {MENU_SEPARATOR_LINE_ROW}
separator_color = "#0000001a"
highlight = {{ image = "images/menu-highlight.png" }}
highlight_text_color = "#ffffff"
shortcut_end_inset = {MENU_SHORTCUT_END_INSET}
shortcut_gap = 20
shadow = {{ color = "#0000004d", offset = [0, 10], blur = 14 }}
rim = "#00000024"
submenu_arrow = "\u25b6"
submenu_arrow_shape = {{ width = 7, height = 9, color = "#404040", end_inset = 13, middle_above_baseline = 5 }}
check_mark = "\u2713"
check_mark_shape = {{ width = 10, height = 11, left_inset = 6, stroke = 2, color = "#404040", start = [0.5, 5.0], bend = [4.5, 9.5], end = [9.2, 0.3], middle_above_baseline = 4 }}

[menu.key_symbols]
Control = "\u2303"
Shift = "\u21e7"
Alt = "\u2325"
Super = "\u2318"
Left = "\u2190"
Up = "\u2191"
Right = "\u2192"
Down = "\u2193"
Backspace = "\u232b"

{MENU_CLOSE_TABLE}
[dock]
icon_size = {DOCK_ICON_SIZE}
compact_icon_size = {DOCK_COMPACT_ICON_SIZE}
compact_up_to_height = {DOCK_COMPACT_UP_TO_HEIGHT}
tile_margin = {DOCK_TILE_MARGIN}
headroom = {DOCK_HEADROOM}
base = {DOCK_BASE}
end_padding = {DOCK_END_PADDING}
row_shift = {DOCK_ROW_SHIFT}
end_trim = {DOCK_END_TRIM}
fit_spare_tiles = {DOCK_FIT_SPARE_TILES}
fit_overhang = {DOCK_FIT_OVERHANG}
largest_icon_size = {DOCK_LARGEST_ICON_SIZE}
smallest_icon_size = 16
magnification_reach = {DOCK_MAGNIFICATION_REACH}
magnification_ramp_ms = {DOCK_MAGNIFICATION_RAMP_MS}
pressed_darken = {DOCK_PRESSED_DARKEN}
tile_slide_ms = {DOCK_TILE_SLIDE_MS}
tile_appear_ms = {DOCK_TILE_APPEAR_MS}
tile_appear_scale = 0.1

[dock.menu]
background = "#252525e0"
text_color = "#ffffff"
disabled_text_color = "#ffffff59"
separator_color = "#ffffff33"
rim = ["#151515e6", "#c8c8c859", "#c8c8c859", "#0000005c"]
pointer = {{ width = 25, height = 13 }}
tile_gap = 4
# A folder's list: 16 icons before the names, rows of 21, and when it is
# taller than the room above the dock it ends 5 below the menu bar and
# scrolls by an arrow strip one row high.
icon_size = 16
icon_gap = 4
icon_item_height = 21
scroll_arrow_height = 19
scroll_speed = 100
top_margin = 5

[volume_display]
width = 211
height = 206
bottom = 140
background = {{ color = "#00000028", radius = 18 }}

[volume_display.pictures]
volume = "images/volume-display-volume.png"
muted = "images/volume-display-muted.png"

[volume_display.segments]
count = 16
width = 7
height = 9
pitch = 9
left = 34
top = 169
color = "#ffffff"
empty_color = "#00000040"

[animation.volume_display]
stay_ms = 1100
fade_out_ms = 700

[lock]
text_color = "#000000"
backdrop_veil = {{ gradient = ["#00000099", "#000000c2"] }}
name_baseline = 145

[lock.box]
width = 238
height = 238
center_y = 0.393
fill = {{ gradient = ["#f2f4f3", "#e2e3e3"], radius = 4 }}
shadow = {{ color = "#00000080", offset = [0, 6], blur = 12 }}

[lock.picture]
size = 84
top = 38
round = true

[lock.field]
top = 184
width = 162
height = 20
fill = {{ color = "#ffffff", border = {{ width = 2, color = "#a5c6e7" }} }}
focus_rings = ["#6ea0d1", "#9bbdde", "#bfd0e0"]
text_inset = 5
placeholder_color = "#a0a0a0"

[lock.clock]
right = 24
baseline = 18
color = "#ffffff8c"

[animation.lock_wrong]
kind = "shake"
swings = [-14, 14, -14, 8, -8, 6]
swing_ms = 67

[sounds]
pack = "SeaLion"

[dock.minimized]
shows = "picture"
badge_share = 0.39

[dock.remove_effect]
image = "images/dock-poof.png"
frames = 5
size = 64
frame_ms = 50

[dock.folder]
open_image = "images/dock-folder-tile-open.png"

[dock.folder.grid]
side_padding = 16
cell_width = 128
cell_height = 128
first_row_top = 40
bottom_padding = 10
# The picture is the cell less 28, flush with the cell's top; the name sits
# in the 28 below it.
icon_size = 100
label_baseline = 111
label_width = 116
label_color = "#ffffff"
label_shadow_color = "#000000a0"
title_baseline = 19
title_color = "#ffffff6b"
screen_margin = 32
# A grid is sized for 80% of the free width and height, counting each cell
# 28 wider and higher than it is; the cells shrink to fit (measured on 10.9
# with folders of 1 to 99 files at 1024x768, 1280x800 and 1920x1080).
stage_share = 0.8
fit_spacing = 28
# The entry the arrow keys chose: the whole cell, light at the top and
# fading downwards (measured over the box: grey 212 at the top to 104 at the
# bottom where the box shows 66), corners of 5.
selection = {{ gradient = ["#ffffffc4", "#ffffff33"], radius = 5 }}
# A grid that scrolls fades out over 10 at its top and bottom, and moves by
# whole rows, a row in about 230 ms at an even pace (measured from frames).
scroll_fade = 10
scroll_snap_ms = 230
scroll_snap_curve = "linear"
# The thin scroll bar while it scrolls: the classic one's lane, 6 in from the
# box's ends (measured with scroll bars always shown); the thin one's place
# in it and its fading are not measured.
scroll_bar_width = 7
scroll_bar_image = "images/dock-folder-scroll-thumb.png"
scroll_bar_slice = 3
scroll_bar_inset = 6
scroll_bar_fade_ms = 300
# A name cut short shows whole while the pointer is on it: dark on a light
# capsule 18 high, grey 219 at its top to 165 at its bottom (measured).
label_expanded = {{ fill = {{ gradient = ["#dbdbdb", "#a5a5a5"], radius = 9 }}, text_color = "#000000", height = 18, padding = 9 }}
# Back to the folder before, at the box's top left, beside the title.
back_button = {{ image = "images/dock-folder-back.png", pressed_image = "images/dock-folder-back-pressed.png", left = 7, top = 5 }}
open_image = "images/dock-folder-open.png"

[dock.folder.fan]
icon_size = 64
pitch = 64
turn_per_entry = 1.5
most_entries = 8
label_gap = 5
label_lift = 2
label_width = 240
open_image = "images/dock-folder-open.png"
open_image_size = 56

[dock.folder.grid.box]
pieces = "images/dock-folder-box"
piece_size = 40
margins = [27, 22, 27, 31]
pointer_width = 45
pointer_height = 13

[dock.shelf]
shape = "slab"
height_above_base = {DOCK_SHELF_HEIGHT_ABOVE_BASE}
slant = {DOCK_SHELF_SLANT}
face_height = {DOCK_SHELF_FACE_HEIGHT}
line_height = {DOCK_SHELF_LINE_HEIGHT}
back_corner_radius = {DOCK_SHELF_BACK_CORNER_RADIUS}
front_corner_heights = {DOCK_SHELF_FRONT_CORNER_HEIGHTS}
front_corner_sizes = {DOCK_SHELF_FRONT_CORNER_SIZES}
clear_above = {DOCK_SHELF_CLEAR_ABOVE}
clear_beside = {DOCK_SHELF_CLEAR_BESIDE}
surface = {displayed_greys(DOCK_SHELF_SURFACE)}
face = {displayed_greys(DOCK_SHELF_FACE)}
line = "{displayed_grey(*DOCK_SHELF_LINE)}"
outline = "{displayed_grey(*DOCK_SHELF_OUTLINE)}"
outline_width = {DOCK_SHELF_OUTLINE_WIDTH}

[dock.indicator]
fill = {{ image = "images/dock-indicator.png" }}
width = 54
height = 12
small_fill = {{ image = "images/dock-indicator-small.png" }}
small_width = 28
small_below_tile_size = {DOCK_INDICATOR_SMALL_BELOW_TILE_SIZE}
center_height = {DOCK_INDICATOR_CENTER_HEIGHT}

[dock.separator]
image = "images/dock-separator.png"
width = {DOCK_SEPARATOR_WIDTH}
bottom = {DOCK_SEPARATOR_BOTTOM}
top_gap = {DOCK_SEPARATOR_TOP_GAP}

[dock.reflection]
opacity = {DOCK_REFLECTION_OPACITY}
overlap = {DOCK_REFLECTION_OVERLAP}
clip_bottom = {DOCK_REFLECTION_CLIP_BOTTOM}
blur = {DOCK_REFLECTION_BLUR:.4f}
contrast = {DOCK_REFLECTION_CONTRAST}
brightness = {DOCK_REFLECTION_BRIGHTNESS}

[dock.floor_shadow]
color = "{displayed_grey(*DOCK_FLOOR_SHADOW)}"
height = 0.5
blur = {DOCK_REFLECTION_BLUR:.4f}

[dock.icon_shadow]
color = "{displayed_grey(*DOCK_ICON_SHADOW)}"
blur = {DOCK_ICON_SHADOW_BLUR}
raise = {DOCK_ICON_SHADOW_RAISE}

[dock.trash]
empty_image = "images/dock-trash-empty.png"
full_image = "images/dock-trash-full.png"

[dock.label]
background = "#1e1e1e99"
text_color = "#ffffff"
height = 20
padding = 11
pointer_width = 12
pointer_height = 6
gap = 5
fade_out_ms = 167

[dock.launch_bounce]
half_period_ms = {DOCK_BOUNCE_HALF_PERIOD_MS}
height = {DOCK_BOUNCE_HEIGHT}
longest_ms = {DOCK_BOUNCE_LONGEST_MS}

[icons]
theme = "SeaLion"

# Made by make_cursors.py into the icon theme's folder.
[cursors]
theme = "SeaLion"
size = 24

[controls]
kvantum_base = "KvMojaveLight"
window_color = "{CONTROL_WINDOW_COLOR}"
text_color = "#000000"
base_color = "#ffffff"
highlight_color = "#5199db"
highlight_text_color = "#ffffff"

[controls.tool_bar]
fill = ["#d1d1d1", "#b0b0b0"]
bottom_line = "#696969"
fill_inactive = ["#eeeeee", "#e0e0e0"]
bottom_line_inactive = "#a6a6a6"

[controls.selected_item]
fill = ["#73b8ea", "#3984d2"]
top_line = "#5ea3df"
bottom_line = "#3577c2"
text_color = "#ffffff"

[controls.button]
slice = {CONTROL_BUTTON_BOX_SLICE}
normal = "images/controls/button-box.png"
pressed = "images/controls/button-pressed-box.png"
disabled = "images/controls/button-disabled-box.png"
default = "images/controls/button-default-box.png"
default_pressed = "images/controls/button-default-pressed-box.png"

[controls.checkbox]
size = {CONTROL_INDICATOR_SIZE}
off = {{ normal = "images/controls/checkbox-off.png", pressed = "images/controls/checkbox-off-pressed.png", disabled = "images/controls/checkbox-off-disabled.png" }}
on = {{ normal = "images/controls/checkbox-on.png", pressed = "images/controls/checkbox-on-pressed.png", disabled = "images/controls/checkbox-on-disabled.png" }}
mixed = {{ normal = "images/controls/checkbox-mixed.png" }}

[controls.radio]
size = {CONTROL_INDICATOR_SIZE}
off = {{ normal = "images/controls/radio-off.png", pressed = "images/controls/radio-off-pressed.png", disabled = "images/controls/radio-off-disabled.png" }}
on = {{ normal = "images/controls/radio-on.png", pressed = "images/controls/radio-on-pressed.png", disabled = "images/controls/radio-on-disabled.png" }}

[controls.text_field]
slice = {CONTROL_TEXT_FIELD_SLICE}
normal = "images/controls/text-field.png"
disabled = "images/controls/text-field-disabled.png"

[controls.progress_bar]
track = "images/controls/progress-track.png"
track_slice = {CONTROL_PROGRESS_TRACK_SLICE}
fill = "images/controls/progress-fill.png"
shows_percentage = false

[controls.tabs]
slice = {CONTROL_TAB_SLICE}
normal = "images/controls/tab.png"
pressed = "images/controls/tab-pressed.png"
selected = "images/controls/tab-selected.png"
selected_pressed = "images/controls/tab-selected-pressed.png"
disabled = "images/controls/tab-disabled.png"
selected_text_color = "#ffffff"
joined = true
centered = true
on_pane_edge = true
pane = "images/controls/tab-pane.png"
pane_slice = {CONTROL_TAB_PANE_SLICE}

[controls.popup_button]
slice = {CONTROL_POPUP_SLICE}
normal = "images/controls/popup.png"
pressed = "images/controls/popup-pressed.png"
disabled = "images/controls/popup-disabled.png"
includes_arrows = true
text_inset = {CONTROL_POPUP_TEXT_INSET}

[controls.tool_button]
slice = {CONTROL_TOOL_BUTTON_SLICE}
normal = "images/controls/tool-button.png"
pressed = "images/controls/tool-button-pressed.png"
disabled = "images/controls/tool-button-disabled.png"

[controls.slider]
track = "images/controls/slider-track.png"
track_slice = {CONTROL_SLIDER_TRACK_SLICE}
thumb = "images/controls/slider-thumb.png"
thumb_pressed = "images/controls/slider-thumb-pressed.png"
thumb_disabled = "images/controls/slider-thumb-disabled.png"

[animation.minimize]
kind = "genie"
duration_ms = 500

[animation.window_open]
kind = "zoom"
duration_ms = 200
curve = "linear"

[animation.window_resize]
kind = "blend"
duration_ms = 320
curve = "ease-in-out"

[desktops]
gap = 64
gap_color = "#000000"

[animation.desktop_switch]
duration_ms = 517
curve = [0.3, 0.525, 0.25, 1.0]

[animation.dock_hide]
duration_ms = 240
curve = "ease-in-out"
show_delay_ms = 150
hide_delay_ms = 300

[animation.window_overview]
duration_ms = 240
curve = "ease-in-out"

[animation.app_grid]
kind = "fade"
duration_ms = 150
close_duration_ms = 220

[animation.dock_folder]
kind = "grow"
duration_ms = 180
close_duration_ms = 110

[animation.dock_folder_fan]
kind = "rise"
duration_ms = 300
close_duration_ms = 160

[animation.app_grid_page]
kind = "slide"
duration_ms = 490
curve = "ease-out"

[animation.banner]
arrive = {{ kind = "drop", duration_ms = 400, curve = "ease-out" }}
leave = {{ kind = "slide", fade = true, duration_ms = 570, curve = "ease-in" }}

[desktop]
background = {{ image = "{THEME_DESKTOP_PICTURES}/{DEFAULT_DESKTOP_PICTURE}", fit = "cover" }}
pictures = "{THEME_DESKTOP_PICTURES}"

[desktop.icons]
icon_size = 64
row_pitch = 112
column_pitch = 112
top = 8
edge_to_icon_middle = 62
bottom_room = 110
picked = {{ color = "#0000004f", radius = 3, border = {{ color = "#ffffff3b", width = 2 }} }}
picked_reach = 4
name = {{ color = "#ffffff", shadow_color = "#000000a0", height = 18, padding = 7, gap = 5, most_width = 110, picked_background = {{ color = "#3a70d7", radius = 9 }} }}
preview_frame = {{ color = "#ffffff", width = 3, share = 0.86, shadow_color = "#00000080", shadow_blur = 4, shadow_drop = 1 }}
band = {{ color = "#ffffff26", border_color = "#ffffff91", border_width = 1 }}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("work", type=Path, help="the work folder: root/, artwork/ and coreui-colors.json")
    parser.add_argument("output", type=Path, help="theme directory to create")
    arguments = parser.parse_args()

    root = arguments.work / "root"
    artwork_dir = arguments.work / "artwork"
    colors = {color["number"]: color for color in load_json(arguments.work / "coreui-colors.json")["colors"]}

    images_dir = arguments.output / "images"
    images_dir.mkdir(parents=True, exist_ok=True)

    picture_size = copy_button_pictures(artwork_dir, images_dir)
    copy_noise_picture(artwork_dir, images_dir)
    copy_shared_art(artwork_dir, images_dir, PANEL_BACKGROUND_ENTRY, "panel-background")
    copy_shared_art(artwork_dir, images_dir, PANEL_LOGO_ENTRY, "panel-logo")
    copy_shared_art(artwork_dir, images_dir, PANEL_TITLE_HIGHLIGHT_ENTRY, "panel-title-highlight")
    copy_shared_art(artwork_dir, images_dir, PANEL_LOGO_HIGHLIGHTED_ENTRY, "panel-logo-highlighted")
    copy_shared_art(artwork_dir, images_dir, MENU_HIGHLIGHT_ENTRY, "menu-highlight")

    copy_dock_pictures(root / DOCK_RESOURCES, images_dir)
    join_scroll_thumb(root / DOCK_RESOURCES, images_dir)
    copy_control_pictures(artwork_dir, images_dir)
    cut_button_boxes(images_dir)
    trim_toolbar_separator(images_dir)
    make_toolbar_glyphs(artwork_dir, images_dir)
    copy_app_grid_pictures(root / DOCK_RESOURCES, images_dir)
    copy_window_overview_pictures(root / DOCK_RESOURCES, images_dir)

    copy_desktop_pictures(root / DESKTOP_PICTURES,
        arguments.output / THEME_DESKTOP_PICTURES)
    copy_typeface(root, arguments.output / THEME_FONTS)
    status_icons = make_status_pictures(root, images_dir)
    make_volume_display_pictures(root, images_dir)
    dialog_icons = make_dialog_pictures(root, images_dir)
    copy_about_picture(root, images_dir)
    banner_backgrounds = copy_notification_pictures(root, images_dir)
    copy_notification_list_pictures(root, images_dir)
    status_icons["preferences-system-notifications"] = "images/notifications/list-status.png"
    (arguments.output / "theme.toml").write_text(
        build_theme_text(colors, picture_size, status_icons, dialog_icons, banner_backgrounds))

    picture_count = len(list(images_dir.glob("**/*.png")))
    print(f"wrote {arguments.output}/theme.toml and {picture_count} pictures")


if __name__ == "__main__":
    main()
