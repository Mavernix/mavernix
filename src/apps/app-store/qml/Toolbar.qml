// The bar along the top: ◀ ▶ on the left, the tabs in the middle
// (Featured, Installed, Updates), the search field on the right. The Updates
// tab carries the number of updates waiting.
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
        anchors.centerIn: parent
        spacing: 4

        Repeater {
            model: ["featured", "installed", "updates"]

            Tab {
                required property string modelData
                text: toolbar.looks.word(modelData)
                selected: toolbar.currentTab === modelData
                badge: modelData === "updates" && toolbar.updateCount > 0 ? toolbar.updateCount : 0
                onClicked: toolbar.tabChosen(modelData)
            }
        }
    }

    component Tab: Item {
        id: tab

        property alias text: label.text
        property bool selected: false
        // A number in a bubble after the label; 0 for none.
        property int badge: 0

        signal clicked()

        width: label.implicitWidth + 2 * toolbar.looks.value("tab_padding") + (badge > 0 ? bubble.width + 6 : 0)
        height: toolbar.looks.value("tab_height")

        Shell.Fill {
            anchors.fill: parent
            visible: tab.selected
            themeDirectory: toolbar.looks.themeDirectory
            fill: toolbar.looks.value("tab_selected")
        }

        StoreText {
            id: label
            x: toolbar.looks.value("tab_padding")
            anchors.verticalCenter: parent.verticalCenter
            looks: toolbar.looks
            kind: "tab"
            color: toolbar.looks.color(tab.selected ? "tab_selected_text_color" : "tab_text_color")
        }

        Item {
            id: bubble
            visible: tab.badge > 0
            anchors.left: label.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
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
