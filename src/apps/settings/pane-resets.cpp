#include "pane-resets.hpp"

#include <QMap>

QList<Choices::Key> pane_choices(const QString& pane)
{
    // { table, { keys } }; no keys is the whole table.
    using Tables = QList<QPair<QString, QStringList>>;
    static const QMap<QString, Tables> panes = {
        {QStringLiteral("dock"),
            {{QStringLiteral("dock"),
                 {QStringLiteral("icon_size"), QStringLiteral("largest_icon_size"), QStringLiteral("magnification"),
                     QStringLiteral("autohide"), QStringLiteral("click_minimizes"),
                     QStringLiteral("window_reflections"), QStringLiteral("window_reflection_opacity")}},
                {QStringLiteral("blur"),
                    {QStringLiteral("dock_shelf"), QStringLiteral("menu_bar"), QStringLiteral("offset")}},
                {QStringLiteral("panel"), {QStringLiteral("hidden"), QStringLiteral("clock_format")}},
                {QStringLiteral("notifications"), {QStringLiteral("banner_seconds")}}}},
        {QStringLiteral("desktop"), {{QStringLiteral("panel"), {QStringLiteral("translucent_menu_bar")}},
                                        {QStringLiteral("blur"), {QStringLiteral("panel_opacity")}},
                                        {QStringLiteral("desktop_files"), {QStringLiteral("show")}},
                                        {QStringLiteral("desktop_menus"), {QStringLiteral("name")}},
                                        {QStringLiteral("desktop"), {QStringLiteral("picture")}}}},
        {QStringLiteral("icons"), {{QStringLiteral("app_icons"), {}}, {QStringLiteral("app_names"), {}},
                                      {QStringLiteral("app_grid"), {QStringLiteral("hidden")}}}},
        {QStringLiteral("sound"),
            {{QStringLiteral("sounds"), {QStringLiteral("alert"), QStringLiteral("enabled"), QStringLiteral("pack"),
                                            QStringLiteral("volume_feedback")}}}},
        {QStringLiteral("trackpad"),
            {{QStringLiteral("input"),
                {QStringLiteral("tap_to_click"), QStringLiteral("natural_scroll"), QStringLiteral("touchpad_speed"),
                    QStringLiteral("touchpad_acceleration"), QStringLiteral("secondary_click")}}}},
        {QStringLiteral("mouse"),
            {{QStringLiteral("input"), {QStringLiteral("mouse_speed"), QStringLiteral("mouse_acceleration"),
                                           QStringLiteral("mouse_natural_scroll"), QStringLiteral("mouse_scroll_speed"),
                                           QStringLiteral("left_handed"), QStringLiteral("middle_click_paste"),
                                           QStringLiteral("shake_to_find")}}}},
        {QStringLiteral("energy"),
            {{QStringLiteral("idle"), {QStringLiteral("lock_before_sleep"), QStringLiteral("screen_off_after_seconds"),
                                          QStringLiteral("sleep_after_seconds")}},
                {QStringLiteral("power"), {QStringLiteral("lid_closed"), QStringLiteral("lid_turns_off_screen")}}}},
        {QStringLiteral("display"),
            {{QStringLiteral("display"), {QStringLiteral("scale"), QStringLiteral("screens"),
                                             QStringLiteral("main_screen"), QStringLiteral("sharp_x11")}}}},
        {QStringLiteral("desktops"),
            {{QStringLiteral("desktops"),
                {QStringLiteral("count"), QStringLiteral("drag_to_edge_ms"), QStringLiteral("swipe_fingers")}}}},
        {QStringLiteral("windows"),
            {{QStringLiteral("window"),
                 {QStringLiteral("snap_to_edges"), QStringLiteral("quit_closes_app"),
                     QStringLiteral("borderless_full_screen"), QStringLiteral("snap_distance"),
                     QStringLiteral("resize_reach"), QStringLiteral("drag_key"), QStringLiteral("title_double_click"),
                     QStringLiteral("button_side"), QStringLiteral("button_order")}},
                {QStringLiteral("gtk"), {QStringLiteral("button_side"), QStringLiteral("button_order")}}}},
        {QStringLiteral("toolkits"), {{QStringLiteral("gtk"), {QStringLiteral("use_theme")}},
                                         {QStringLiteral("qt"), {QStringLiteral("use_theme")}}}},
        {QStringLiteral("keyboard"),
            {{QStringLiteral("shortcuts"), {}}, {QStringLiteral("commands"), {}},
                {QStringLiteral("game_keys"), {QStringLiteral("all_x11")}},
                {QStringLiteral("input"), {QStringLiteral("keyboard_layout"), QStringLiteral("keyboard_variant"),
                                              QStringLiteral("keyboard_switch_key")}}}},
        {QStringLiteral("startup"), {{QStringLiteral("session"), {QStringLiteral("users_autostart")}}}},
    };

    QList<Choices::Key> keys;
    for (const auto& [table, names] : panes.value(pane))
    {
        if (names.isEmpty())
        {
            keys.append({table, QString()});
        }
        for (const QString& name : names)
        {
            keys.append({table, name});
        }
    }
    return keys;
}
