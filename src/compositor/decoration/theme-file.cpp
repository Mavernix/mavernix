#include "theme-file.hpp"

#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <toml++/toml.hpp>

namespace themed
{
namespace
{
/**
 * Builds the error for a problem in the theme file. Every message names the
 * key, written the way it appears in the file ("window.titlebar.height").
 */
std::runtime_error problem(const std::string& key, const std::string& what)
{
    return std::runtime_error("theme key \"" + key + "\": " + what);
}

/**
 * Refuses keys the format does not define: a misspelt key must not quietly
 * fall back to a default.
 */
void reject_unknown_keys(const toml::table& table, const std::string& table_name,
    const std::set<std::string>& known_keys)
{
    for (const auto& [key, value] : table)
    {
        const std::string name{key.str()};
        if (known_keys.count(name) == 0)
        {
            throw problem(table_name + "." + name, "is not a key this version understands");
        }
    }
}

int hex_digit(char character)
{
    if ((character >= '0') && (character <= '9'))
    {
        return character - '0';
    }

    if ((character >= 'a') && (character <= 'f'))
    {
        return character - 'a' + 10;
    }

    if ((character >= 'A') && (character <= 'F'))
    {
        return character - 'A' + 10;
    }

    return -1;
}

/** Parses "#rrggbb" or "#rrggbbaa". */
rgba_t parse_color(const std::string& text, const std::string& key)
{
    const bool has_alpha = (text.size() == 9);
    if ((text.size() != 7) && !has_alpha)
    {
        throw problem(key, "\"" + text + "\" is not a colour; write #rrggbb or #rrggbbaa");
    }

    if (text[0] != '#')
    {
        throw problem(key, "\"" + text + "\" is not a colour; it must start with #");
    }

    // Two hex digits per channel, after the leading '#'.
    auto channel = [&] (size_t index)
    {
        const int high = hex_digit(text[1 + index * 2]);
        const int low  = hex_digit(text[2 + index * 2]);
        if ((high < 0) || (low < 0))
        {
            throw problem(key, "\"" + text + "\" contains something that is not a hex digit");
        }

        return (high * 16 + low) / 255.0;
    };

    rgba_t color;
    color.red   = channel(0);
    color.green = channel(1);
    color.blue  = channel(2);
    color.alpha = has_alpha ? channel(3) : 1.0;
    return color;
}

/* --- Reading single values, with clear errors when the type is wrong --- */

const toml::table& require_table(const toml::table& parent, std::string_view name,
    std::string_view parent_name)
{
    const toml::table *table = parent[name].as_table();
    if (table == nullptr)
    {
        const std::string place = parent_name.empty() ? std::string(name)
            : std::string(parent_name) + "." + std::string(name);
        throw problem(place, "this table is missing");
    }

    return *table;
}

double require_number(const toml::table& table, const std::string& name,
    const std::string& table_name)
{
    auto value = table[name].value<double>();
    if (!value)
    {
        throw problem(table_name + "." + name, "is missing or is not a number");
    }

    return *value;
}

std::string require_string(const toml::table& table, const std::string& name,
    const std::string& table_name)
{
    auto value = table[name].value<std::string>();
    if (!value)
    {
        throw problem(table_name + "." + name, "is missing or is not text in quotes");
    }

    return *value;
}

rgba_t require_color(const toml::table& table, const std::string& name,
    const std::string& table_name)
{
    return parse_color(require_string(table, name, table_name), table_name + "." + name);
}

/** Gradient colours run top to bottom, spread evenly unless `positions` places them. */
std::vector<gradient_stop_t> read_gradient(const toml::table& fill, const std::string& key)
{
    const toml::array *colors = fill["gradient"].as_array();
    if ((colors == nullptr) || (colors->size() < 2))
    {
        throw problem(key + ".gradient", "must be a list of at least two colours");
    }

    const toml::array *positions = fill["positions"].as_array();
    if ((positions != nullptr) && (positions->size() != colors->size()))
    {
        throw problem(key + ".positions", "must have one number for each colour in the gradient");
    }

    std::vector<gradient_stop_t> stops;
    for (size_t index = 0; index < colors->size(); index++)
    {
        auto color_text = (*colors)[index].value<std::string>();
        if (!color_text)
        {
            throw problem(key + ".gradient", "contains something that is not a colour in quotes");
        }

        double position = static_cast<double>(index) / (colors->size() - 1);
        if (positions != nullptr)
        {
            auto given = (*positions)[index].value<double>();
            if (!given || (*given < 0.0) || (*given > 1.0))
            {
                throw problem(key + ".positions", "each position must be a number from 0.0 to 1.0");
            }

            position = *given;
        }

        stops.push_back({position, parse_color(*color_text, key + ".gradient")});
    }

    return stops;
}

overlay_t::blend_t parse_blend(const std::string& text, const std::string& key)
{
    if (text == "normal")
    {
        return overlay_t::blend_t::NORMAL;
    }

    if (text == "overlay")
    {
        return overlay_t::blend_t::OVERLAY;
    }

    if (text == "multiply")
    {
        return overlay_t::blend_t::MULTIPLY;
    }

    if (text == "screen")
    {
        return overlay_t::blend_t::SCREEN;
    }

    throw problem(key + ".blend",
        "\"" + text + "\" is not one of \"normal\", \"overlay\", \"multiply\", \"screen\"");
}

/**
 * Reads one fill: `{ color = ... }`, `{ gradient = [...] }` or
 * `{ image = "..." }`, each with its optional extras.
 */
fill_t read_fill(const toml::table& parent, const std::string& name,
    const std::string& parent_name, const std::string& theme_directory)
{
    const std::string key = parent_name + "." + name;
    const toml::table *fill_table = parent[name].as_table();
    if (fill_table == nullptr)
    {
        throw problem(key, "is missing or is not a fill; write { color = \"#rrggbb\" }, "
                           "{ gradient = [...] } or { image = \"...\" }");
    }

    reject_unknown_keys(*fill_table, key,
        {"color", "gradient", "positions", "image", "slice", "radius", "border", "overlay"});

    const bool has_color    = fill_table->contains("color");
    const bool has_gradient = fill_table->contains("gradient");
    const bool has_image    = fill_table->contains("image");
    if (has_color + has_gradient + has_image != 1)
    {
        throw problem(key, "must have exactly one of color, gradient or image");
    }

    fill_t fill;
    if (has_color)
    {
        fill.kind  = fill_t::kind_t::COLOR;
        fill.color = require_color(*fill_table, "color", key);
    } else if (has_gradient)
    {
        fill.kind     = fill_t::kind_t::GRADIENT;
        fill.gradient = read_gradient(*fill_table, key);
    } else
    {
        fill.kind = fill_t::kind_t::IMAGE;
        // Picture paths in a theme are relative to the theme's directory.
        fill.image_path = theme_directory + "/" + require_string(*fill_table, "image", key);

        if (const toml::array *slice = (*fill_table)["slice"].as_array())
        {
            if (slice->size() != 4)
            {
                throw problem(key + ".slice", "must be four numbers: top, right, bottom, left");
            }

            for (size_t edge = 0; edge < 4; edge++)
            {
                fill.slice[edge] = (*slice)[edge].value_or(0);
            }
        }
    }

    fill.radius = (*fill_table)["radius"].value_or(0.0);

    if (const toml::table *border = (*fill_table)["border"].as_table())
    {
        reject_unknown_keys(*border, key + ".border", {"color", "width"});
        fill.border_color = require_color(*border, "color", key + ".border");
        fill.border_width = require_number(*border, "width", key + ".border");
    }

    if (const toml::table *overlay = (*fill_table)["overlay"].as_table())
    {
        const std::string overlay_key = key + ".overlay";
        reject_unknown_keys(*overlay, overlay_key, {"image", "opacity", "blend"});
        fill.overlay.image_path =
            theme_directory + "/" + require_string(*overlay, "image", overlay_key);
        fill.overlay.opacity = (*overlay)["opacity"].value_or(1.0);
        fill.overlay.blend   =
            parse_blend((*overlay)["blend"].value_or(std::string("normal")), overlay_key);
    }

    return fill;
}

/**
 * Reads a shadow's offset, a pair [right, down], into `shadow`. A missing
 * key leaves the shadow as it is.
 */
void read_offset(const toml::table& table, const std::string& name, shadow_t& shadow)
{
    const toml::array *pair = table[name].as_array();
    if (pair == nullptr)
    {
        return;
    }

    const auto right = (pair->size() == 2) ? (*pair)[0].value<double>() : std::nullopt;
    const auto down  = (pair->size() == 2) ? (*pair)[1].value<double>() : std::nullopt;
    if (!right || !down)
    {
        throw problem("window.shadow." + name, "must be two numbers: [right, down]");
    }

    shadow.offset_x = *right;
    shadow.offset_y = *down;
}

/** Reads a list of colours, such as the title bar's separator lines. */
std::vector<rgba_t> read_color_list(const toml::table& table, const std::string& name,
    const std::string& table_name)
{
    std::vector<rgba_t> colors;
    const toml::array *list = table[name].as_array();
    if (list == nullptr)
    {
        // The key is optional: no list means no lines.
        return colors;
    }

    for (const auto& element : *list)
    {
        auto color_text = element.value<std::string>();
        if (!color_text)
        {
            throw problem(table_name + "." + name, "must be a list of colours in quotes");
        }

        colors.push_back(parse_color(*color_text, table_name + "." + name));
    }

    return colors;
}

button_theme_t read_button(const toml::table& buttons, const std::string& name,
    const std::string& theme_directory)
{
    const std::string key = "window.buttons." + name;
    const toml::table& button_table = require_table(buttons, name, "window.buttons");
    reject_unknown_keys(button_table, key, {"normal", "hover", "pressed", "inactive", "disabled"});

    button_theme_t button;
    // NORMAL is required: it is what every other state falls back to.
    button.looks[button_state_t::NORMAL] =
        read_fill(button_table, "normal", key, theme_directory);

    const std::pair<const char*, button_state_t> optional_states[] = {
        {"hover", button_state_t::HOVER},
        {"pressed", button_state_t::PRESSED},
        {"inactive", button_state_t::INACTIVE},
        {"disabled", button_state_t::DISABLED},
    };
    for (const auto& [state_name, state] : optional_states)
    {
        if (button_table.contains(state_name))
        {
            button.looks[state] = read_fill(button_table, state_name, key, theme_directory);
        }
    }

    return button;
}

title_align_t parse_align(const std::string& text)
{
    if (text == "left")
    {
        return title_align_t::LEFT;
    }

    if (text == "center")
    {
        return title_align_t::CENTER;
    }

    if (text == "right")
    {
        return title_align_t::RIGHT;
    }

    throw problem("window.titlebar.title_align",
        "\"" + text + "\" is not one of \"left\", \"center\", \"right\"");
}
} // anonymous namespace

const fill_t& button_theme_t::look_for(button_state_t state) const
{
    // A button that does nothing is greyed: without a look of its own, it
    // takes the one of a window in the background.
    if ((state == button_state_t::DISABLED) && (looks.count(state) == 0))
    {
        state = button_state_t::INACTIVE;
    }

    auto found = looks.find(state);
    if (found != looks.end())
    {
        return found->second;
    }

    return looks.at(button_state_t::NORMAL);
}

window_theme_t load_window_theme(const std::string& theme_directory)
{
    const std::string file_path = theme_directory + "/theme.toml";

    toml::table document;
    try
    {
        document = toml::parse_file(file_path);
    } catch (const toml::parse_error& error)
    {
        std::ostringstream message;
        message << "cannot read " << file_path << ": " << error.description()
                << " (line " << error.source().begin.line << ")";
        throw std::runtime_error(message.str());
    }

    window_theme_t theme;

    /* [fonts] */
    const toml::table& fonts = require_table(document, "fonts", "");
    const toml::table& title_font = require_table(fonts, "window_title", "fonts");
    reject_unknown_keys(title_font, "fonts.window_title", {"family", "size"});
    theme.title_font_family  = require_string(title_font, "family", "fonts.window_title");
    theme.title_font_size = require_number(title_font, "size", "fonts.window_title");

    const toml::table& window = require_table(document, "window", "");
    // "snap_preview" is not the frame's: the session reads it for the
    // outline a dragged window shows at the screen's side.
    reject_unknown_keys(window, "window", {"titlebar", "buttons", "frame", "shadow", "snap_preview"});

    /* [window.titlebar] */
    const toml::table& titlebar = require_table(window, "titlebar", "window");
    reject_unknown_keys(titlebar, "window.titlebar",
        {"height", "background", "background_inactive", "separator",
            "separator_inactive", "highlight", "highlight_inactive",
            "background_joined", "background_joined_inactive", "separator_joined",
            "separator_joined_inactive", "title_color",
            "title_color_inactive", "title_align", "title_padding"});
    theme.titlebar_height = require_number(titlebar, "height", "window.titlebar");
    theme.titlebar_background =
        read_fill(titlebar, "background", "window.titlebar", theme_directory);
    theme.titlebar_background_inactive =
        read_fill(titlebar, "background_inactive", "window.titlebar", theme_directory);
    theme.titlebar_separator = read_color_list(titlebar, "separator", "window.titlebar");
    theme.titlebar_separator_inactive =
        read_color_list(titlebar, "separator_inactive", "window.titlebar");
    theme.titlebar_highlight = read_color_list(titlebar, "highlight", "window.titlebar");
    theme.titlebar_highlight_inactive =
        read_color_list(titlebar, "highlight_inactive", "window.titlebar");
    // Optional: for a window with a tool bar joined to the title bar. What
    // a theme does not give is the same as for any other window.
    theme.titlebar_background_joined = titlebar.contains("background_joined") ?
        read_fill(titlebar, "background_joined", "window.titlebar", theme_directory) :
        theme.titlebar_background;
    theme.titlebar_background_joined_inactive = titlebar.contains("background_joined_inactive") ?
        read_fill(titlebar, "background_joined_inactive", "window.titlebar", theme_directory) :
        theme.titlebar_background_inactive;
    theme.titlebar_separator_joined = titlebar.contains("separator_joined") ?
        read_color_list(titlebar, "separator_joined", "window.titlebar") :
        theme.titlebar_separator;
    theme.titlebar_separator_joined_inactive = titlebar.contains("separator_joined_inactive") ?
        read_color_list(titlebar, "separator_joined_inactive", "window.titlebar") :
        theme.titlebar_separator_inactive;
    theme.title_color = require_color(titlebar, "title_color", "window.titlebar");
    theme.title_color_inactive =
        require_color(titlebar, "title_color_inactive", "window.titlebar");
    theme.title_align =
        parse_align(require_string(titlebar, "title_align", "window.titlebar"));
    theme.title_padding = require_number(titlebar, "title_padding", "window.titlebar");

    /* [window.buttons] */
    const toml::table& buttons = require_table(window, "buttons", "window");
    reject_unknown_keys(buttons, "window.buttons",
        {"size", "spacing", "edge_inset", "hover_together", "close", "minimize", "maximize"});
    const toml::array *button_size = buttons["size"].as_array();
    if ((button_size == nullptr) || (button_size->size() != 2))
    {
        throw problem("window.buttons.size", "must be two numbers: width and height");
    }

    theme.button_width      = (*button_size)[0].value_or(0);
    theme.button_height     = (*button_size)[1].value_or(0);
    theme.button_spacing    = require_number(buttons, "spacing", "window.buttons");
    theme.button_edge_inset = require_number(buttons, "edge_inset", "window.buttons");
    // Optional: a theme that does not say lights only the button pointed at.
    if (buttons.contains("hover_together"))
    {
        const auto together = buttons["hover_together"].value<bool>();
        if (!together)
        {
            throw problem("window.buttons.hover_together", "must be true or false");
        }

        theme.buttons_hover_together = *together;
    }
    theme.close    = read_button(buttons, "close", theme_directory);
    theme.minimize = read_button(buttons, "minimize", theme_directory);
    theme.maximize = read_button(buttons, "maximize", theme_directory);

    /* [window.frame] */
    const toml::table& frame = require_table(window, "frame", "window");
    reject_unknown_keys(frame, "window.frame",
        {"border_width", "border_color", "border_color_inactive", "corner_radius",
            "bottom_corner_radius", "rim", "rim_inactive", "resize_grip", "resize_grip_in_title_bar",
            "resize_corner"});
    theme.border_width = require_number(frame, "border_width", "window.frame");
    // Optional: a theme that does not say gets the usual few pixels.
    if (frame.contains("resize_grip"))
    {
        theme.resize_grip = require_number(frame, "resize_grip", "window.frame");
    }
    if (frame.contains("resize_grip_in_title_bar"))
    {
        theme.resize_grip_in_title_bar =
            require_number(frame, "resize_grip_in_title_bar", "window.frame");
    }
    if (frame.contains("resize_corner"))
    {
        theme.resize_corner = require_number(frame, "resize_corner", "window.frame");
    }
    theme.border_color = require_color(frame, "border_color", "window.frame");
    theme.border_color_inactive = require_color(frame, "border_color_inactive", "window.frame");
    theme.corner_radius = require_number(frame, "corner_radius", "window.frame");
    // Optional: a theme that does not say keeps square bottom corners.
    if (frame.contains("bottom_corner_radius"))
    {
        theme.bottom_corner_radius = require_number(frame, "bottom_corner_radius", "window.frame");
    }
    theme.rim = read_color_list(frame, "rim", "window.frame");
    theme.rim_inactive = read_color_list(frame, "rim_inactive", "window.frame");
    for (const auto *rim : {&theme.rim, &theme.rim_inactive})
    {
        if (!rim->empty() && (rim->size() != RIM_COLOR_COUNT))
        {
            throw problem("window.frame.rim",
                "rim and rim_inactive must each list three colours: top, sides, bottom");
        }
    }

    /* [window.shadow], optional. What it does not say for inactive windows
     * is the same as for active ones. */
    if (const toml::table *shadow = window["shadow"].as_table())
    {
        reject_unknown_keys(*shadow, "window.shadow",
            {"color", "blur_radius", "offset", "color_inactive", "blur_radius_inactive",
                "offset_inactive"});
        theme.has_shadow = true;
        theme.shadow.color = require_color(*shadow, "color", "window.shadow");
        theme.shadow.blur_radius = require_number(*shadow, "blur_radius", "window.shadow");
        read_offset(*shadow, "offset", theme.shadow);

        theme.shadow_inactive = theme.shadow;
        if (shadow->contains("color_inactive"))
        {
            theme.shadow_inactive.color = require_color(*shadow, "color_inactive", "window.shadow");
        }
        if (shadow->contains("blur_radius_inactive"))
        {
            theme.shadow_inactive.blur_radius =
                require_number(*shadow, "blur_radius_inactive", "window.shadow");
        }
        read_offset(*shadow, "offset_inactive", theme.shadow_inactive);
    }

    return theme;
}

window_theme_t fallback_window_theme()
{
    // Deliberately dull: mid greys that are readable on any wallpaper and
    // cannot be mistaken for a finished theme.
    const rgba_t bar_grey   = {0.80, 0.80, 0.80, 1.0};
    const rgba_t faded_grey = {0.90, 0.90, 0.90, 1.0};
    const rgba_t dark_text  = {0.10, 0.10, 0.10, 1.0};
    const rgba_t faded_text = {0.50, 0.50, 0.50, 1.0};

    auto solid = [] (rgba_t color)
    {
        fill_t fill;
        fill.kind  = fill_t::kind_t::COLOR;
        fill.color = color;
        return fill;
    };

    window_theme_t theme;
    theme.titlebar_height = 24;
    theme.titlebar_background = solid(bar_grey);
    theme.titlebar_background_inactive = solid(faded_grey);
    theme.titlebar_background_joined = theme.titlebar_background;
    theme.titlebar_background_joined_inactive = theme.titlebar_background_inactive;
    theme.title_color = dark_text;
    theme.title_color_inactive = faded_text;
    theme.title_align = title_align_t::LEFT;
    theme.title_padding = 8;
    theme.title_font_family  = "sans-serif";
    theme.title_font_size = 13;

    theme.button_width      = 14;
    theme.button_height     = 14;
    theme.button_spacing    = 6;
    theme.button_edge_inset = 6;
    for (button_theme_t *button : {&theme.close, &theme.minimize, &theme.maximize})
    {
        button->looks[button_state_t::NORMAL] = solid({0.55, 0.55, 0.55, 1.0});
        button->looks[button_state_t::HOVER]  = solid({0.35, 0.35, 0.35, 1.0});
    }

    theme.border_width = 1;
    theme.border_color = {0.55, 0.55, 0.55, 1.0};
    theme.border_color_inactive = {0.70, 0.70, 0.70, 1.0};
    theme.corner_radius = 0;
    return theme;
}
}
