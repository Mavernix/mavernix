// Everything the App Store's parts need to know about how to look and what to
// say, in one object that Main.qml makes and hands to each part as `looks`.
//
// Sizes, colours and fills come from the theme's [app_store] table
// (docs/themes/programs.md, "App Store"); a key the theme leaves out takes the
// value in `defaults`, the plain theme's. Fonts are the theme's
// app_store_* fonts. Words come from the desktop config's [app_store.words]
// (docs/configuration.md); a word it leaves out is the one in `defaultWords`.

import QtQml
import "../../../shell/ThemeValues.js" as ThemeValues

QtObject {
    id: looks

    required property var settings

    readonly property var theme: settings.theme.app_store ?? ({})
    readonly property var fonts: settings.theme.fonts
    readonly property var words: (settings.config.app_store ?? ({})).words ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    readonly property var defaults: ({
        width: 960,
        height: 660,
        background: "#ffffff",
        text_color: "#202020",
        heading_color: "#202020",
        secondary_text_color: "#808080",
        failure_text_color: "#c03030",
        separator_color: "#dcdcdc",
        side_margin: 24,
        toolbar_height: 48,
        toolbar: { color: "#ececec" },
        toolbar_inactive: { color: "#f4f4f4" },
        toolbar_border_color: "#c0c0c0",
        toolbar_border_color_inactive: "#d4d4d4",
        toolbar_margin: 12,
        back_forward_size: [56, 24],
        back_forward: { color: "#ffffff", radius: 4, border: { color: "#b0b0b0", width: 1 } },
        back_forward_pressed: { color: "#e0e0e0", radius: 4, border: { color: "#b0b0b0", width: 1 } },
        back_forward_inactive: { color: "#f8f8f8", radius: 4, border: { color: "#d0d0d0", width: 1 } },
        back_forward_separator: { color: "#b0b0b0" },
        back_forward_separator_width: 1,
        back_forward_separator_inset: [1, 1],
        arrow_color: "#404040",
        search_field_size: [160, 24],
        search_field: { color: "#ffffff", radius: 12, border: { color: "#b0b0b0", width: 1 } },
        search_text_color: "#202020",
        tab_height: 26,
        tab_padding: 12,
        tab_text_color: "#505050",
        tab_selected_text_color: "#000000",
        tab_selected: { color: "#d4d4d4", radius: 4 },
        badge: { color: "#3a6ea5", radius: 8 },
        badge_text_color: "#ffffff",
        heading_height: 36,
        tile_width: 290,
        tile_height: 96,
        tile_icon_size: 64,
        page_icon_size: 128,
        information_width: 200,
        screenshot_height: 340,
        row_height: 76,
        row_icon_size: 48,
        button_height: 24,
        small_button_height: 20,
        button_padding: 12,
        button: { color: "#ffffff", radius: 4, border: { color: "#b0b0b0", width: 1 } },
        button_pressed: { color: "#e0e0e0", radius: 4, border: { color: "#b0b0b0", width: 1 } },
        button_text_color: "#202020",
        button_confirm: { color: "#3a8a3a", radius: 4 },
        button_confirm_pressed: { color: "#2e6e2e", radius: 4 },
        button_confirm_text_color: "#ffffff",
        progress_track: { color: "#e4e4e4", radius: 4 },
        progress_bar: { color: "#3a6ea5", radius: 4 },
    })

    readonly property var defaultWords: ({
        featured: "Featured",
        installed: "Installed",
        updates: "Updates",
        search: "Search",
        results: "Results for “{text}”",
        no_results: "No apps match “{text}”.",
        featured_heading: "Apps for Mavernix",
        get: "GET",
        install: "INSTALL",
        open: "OPEN",
        update: "UPDATE",
        update_all: "UPDATE ALL",
        remove: "Remove",
        waiting: "WAITING",
        installing: "INSTALLING",
        removing: "REMOVING",
        whats_new: "What's New in Version {version}",
        information: "Information",
        category: "Category",
        released: "Updated",
        version: "Version",
        size: "Size",
        developer: "Developer",
        license: "License",
        website: "Website",
        loading: "Loading…",
        try_again: "Try Again",
        nothing_offered: "There are no apps in the App Store yet.",
        nothing_installed: "Apps you get from the App Store show up here.",
        no_updates: "No Updates Available",
        one_update: "1 Update Available",
        updates_available: "{count} Updates Available",
    })

    // A picture of the theme (back_glyph and the like), as a URL, or "" when it has none.
    function picture(key) {
        const path = theme[key] ?? ""
        return path === "" ? "" : "file://" + themeDirectory + "/" + path
    }

    function value(key) {
        return theme[key] ?? defaults[key]
    }

    function color(key) {
        return ThemeValues.color(value(key))
    }

    // A font of the theme, by the part of its name after "app_store_".
    function font(name) {
        return fonts["app_store_" + name] ?? fonts.interface
    }

    // A word of the config, with {name} replaced by what `values` holds for it.
    function word(key, values) {
        let text = words[key] ?? defaultWords[key] ?? key
        for (const name in values ?? {}) {
            // A function, so a "$" in the value (searched-for text) isn't read as a pattern.
            text = text.replace("{" + name + "}", () => values[name])
        }
        return text
    }
}
