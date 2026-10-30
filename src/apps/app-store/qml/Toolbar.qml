// The bar along the top: ◀ ▶ on the left, the tabs in the middle
// (Featured, Installed, Updates), the search field on the right. The Updates
// tab carries the number of updates waiting. A theme with pictures for the
// tabs (featured_glyph and the like) gets them over the tabs' names, as
// OS X's toolbars have them; without, the tabs are their names.
//
// A theme can make the window's title bar run on into this bar (the desktop
// config's joined_tool_bar_apps): it then starts its fills where the title
// bar's end, and turns lighter with it when the window isn't the active one.

import QtQuick
import QtQuick.Window
import "../../../shell" as Shell

Item {
    id: toolbar

    required property var looks
    // The tab that's showing, or "" on an app's page.
    required property string currentTab
    required property bool canGoBack
    required property bool canGoForward
    required property int updateCount
    property alias searchText: search.text

    signal tabChosen(string tab)
    signal backChosen()
    signal forwardChosen()

    readonly property bool active: Window.active
    // From the bar's top to ◀ ▶ and the search field; centred if the theme doesn't say.
    function controlsTop(controlHeight) {
        return looks.value("toolbar_controls_top") ?? Math.round((height - controlHeight) / 2)
    }

    height: looks.value("toolbar_height")

    Shell.Fill {
        anchors.fill: parent
        themeDirectory: toolbar.looks.themeDirectory
        fill: toolbar.looks.value(toolbar.active ? "toolbar" : "toolbar_inactive")
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: toolbar.looks.color(toolbar.active ? "toolbar_border_color" : "toolbar_border_color_inactive")
    }

    BackForwardButtons {
        anchors.left: parent.left
        anchors.leftMargin: toolbar.looks.value("toolbar_margin")
        y: toolbar.controlsTop(height)
        looks: toolbar.looks
        canGoBack: toolbar.canGoBack
        canGoForward: toolbar.canGoForward
        windowActive: toolbar.active
        onBackChosen: toolbar.backChosen()
        onForwardChosen: toolbar.forwardChosen()
    }

    SearchField {
        id: search
        anchors.right: parent.right
        anchors.rightMargin: toolbar.looks.value("toolbar_margin")
        y: toolbar.controlsTop(height)
        looks: toolbar.looks
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.floor((parent.height - toolbar.looks.value("tab_height")) / 2)
        spacing: toolbar.looks.value("tab_spacing") ?? 4

        Repeater {
            model: ["featured", "installed", "updates"]

            Tab {
                required property string modelData
                name: modelData
                text: toolbar.looks.word(modelData)
                selected: toolbar.currentTab === modelData
                badge: modelData === "updates" && toolbar.updateCount > 0 ? toolbar.updateCount : 0
                onClicked: toolbar.tabChosen(modelData)
            }
        }
    }

    component Tab: Item {
        id: tab

        required property string name
        property alias text: label.text
        property bool selected: false
        // A number in a bubble after the label; 0 for none.
        property int badge: 0

        signal clicked()

        // Greyed in a window that isn't the active one, as ◀ ▶ are.
        readonly property string glyph: toolbar.looks.picture(name + (toolbar.active ? "_glyph" : "_glyph_disabled"))
                                        || toolbar.looks.picture(name + "_glyph")
        readonly property real labelWidth: label.implicitWidth + (badge > 0 ? bubble.width + 6 : 0)

        width: Math.max(labelWidth, glyph !== "" ? icon.implicitWidth : 0) + 2 * toolbar.looks.value("tab_padding")
        height: toolbar.looks.value("tab_height")

        Shell.Fill {
            anchors.fill: parent
            visible: tab.selected
            themeDirectory: toolbar.looks.themeDirectory
            fill: (toolbar.active ? null : toolbar.looks.value("tab_selected_inactive")) ?? toolbar.looks.value("tab_selected")
        }

        Image {
            id: icon
            visible: tab.glyph !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            y: toolbar.looks.value("tab_icon_top") ?? 0
            source: tab.glyph
        }

        StoreText {
            id: label
            x: Math.round((tab.width - tab.labelWidth) / 2)
            // Under the icon at the theme's baseline, or in the middle.
            y: tab.glyph !== "" ? (toolbar.looks.value("tab_label_baseline") ?? icon.y + icon.height + baselineOffset) - baselineOffset
                                : Math.round((tab.height - height) / 2)
            looks: toolbar.looks
            kind: "tab"
            color: toolbar.looks.color(!toolbar.active && toolbar.looks.value("tab_text_color_inactive") !== undefined
                                       ? "tab_text_color_inactive"
                                       : tab.selected ? "tab_selected_text_color" : "tab_text_color")
        }

        Item {
            id: bubble
            visible: tab.badge > 0
            anchors.left: label.right
            anchors.leftMargin: 6
            anchors.verticalCenter: label.verticalCenter
            width: Math.max(height, number.implicitWidth + 10)
            height: number.implicitHeight + 2

            Shell.Fill {
                anchors.fill: parent
                themeDirectory: toolbar.looks.themeDirectory
                fill: toolbar.looks.value("badge")
            }

            StoreText {
                id: number
                anchors.centerIn: parent
                looks: toolbar.looks
                kind: "small"
                color: toolbar.looks.color("badge_text_color")
                text: tab.badge
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: tab.clicked()
        }
    }
}
