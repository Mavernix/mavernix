// The App Store's window: the toolbar, and under it the page that's showing,
// one of the three tabs (Featured, Installed, Updates) or an app's own page.
// ◀ and ▶ go through the pages shown before, like a browser's. While there's
// text in the search field, the apps that match it show instead.
//
// `settings` and `store` come from main.cpp; how the window looks and what it
// says come from Looks.qml. The window's frame and title bar are the
// compositor's, like any window's.

import QtQuick
import QtQuick.Window

Window {
    id: window

    // Under another name, so that Looks' own `settings` doesn't hide it.
    readonly property var desktopSettings: settings
    readonly property Looks looks: Looks {
        settings: window.desktopSettings
    }

    // What's showing: {tab} or {app: name}. `history` holds what was shown
    // before, `future` what ◀ went back from.
    property var shown: ({ tab: "featured" })
    property var history: []
    property var future: []
    readonly property string searchText: toolbar.searchText.trim()

    readonly property var installedApps: store.apps.filter(app => app.installed_version !== "")
    readonly property var appsWithUpdates: installedApps.filter(app => app.installed_version !== app.version)

    function show(page) {
        toolbar.searchText = ""
        history = history.concat([shown])
        future = []
        shown = page
    }

    function goBack() {
        toolbar.searchText = ""
        future = future.concat([shown])
        shown = history[history.length - 1]
        history = history.slice(0, -1)
    }

    function goForward() {
        toolbar.searchText = ""
        history = history.concat([shown])
        shown = future[future.length - 1]
        future = future.slice(0, -1)
    }

    function matchesSearch(app) {
        const wanted = searchText.toLowerCase()
        return [app.title, app.summary, app.category, app.developer].some(text => (text ?? "").toLowerCase().includes(wanted))
    }

    function appNamed(name) {
        return store.apps.find(app => app.name === name) ?? null
    }

    title: (settings.config.app_store ?? ({})).name ?? "App Store"
    width: looks.value("width")
    height: looks.value("height")
    minimumWidth: 720
    minimumHeight: 480
    color: looks.color("background")
    visible: true

    Toolbar {
        id: toolbar
        width: parent.width
        looks: window.looks
        currentTab: window.shown.tab ?? ""
        canGoBack: window.history.length > 0
        canGoForward: window.future.length > 0
        updateCount: window.appsWithUpdates.length
        onTabChosen: tab => {
            // Opening Updates looks for new versions, as OS X's store does.
            // An unchanged answer from GitHub costs nothing (web.py).
            if (tab === "updates") {
                store.refresh()
            }
            window.show({ tab: tab })
        }
        onBackChosen: window.goBack()
        onForwardChosen: window.goForward()
    }

    // When GitHub can't be asked, the installed apps still show; this line says
    // why the others don't.
    StoreText {
        id: notice
        anchors.top: toolbar.bottom
        width: parent.width
        visible: store.problem !== "" && store.apps.length > 0
        height: visible ? implicitHeight : 0
        padding: 6
        horizontalAlignment: Text.AlignHCenter
        looks: window.looks
        kind: "small"
        color: window.looks.color("failure_text_color")
        text: store.problem
    }

    Loader {
        anchors.top: notice.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        // Until the catalog arrives, or when it can't be read, a message stands in for the pages.
        active: store.apps.length > 0
        sourceComponent: {
            if (window.searchText !== "") {
                return searchResults
            }
            if (window.shown.app !== undefined) {
                return appPage
            }
            return { featured: featuredPage, installed: installedPage, updates: updatesPage }[window.shown.tab]
        }
    }

    Component {
        id: featuredPage

        FeaturedPage {
            looks: window.looks
            apps: store.apps
            heading: window.looks.word("featured_heading")
            onAppChosen: name => window.show({ app: name })
        }
    }

    Component {
        id: searchResults

        FeaturedPage {
            looks: window.looks
            apps: store.apps.filter(app => window.matchesSearch(app))
            heading: window.looks.word(apps.length > 0 ? "results" : "no_results", { text: window.searchText })
            onAppChosen: name => window.show({ app: name })
        }
    }

    Component {
        id: installedPage

        AppListPage {
            looks: window.looks
            apps: window.installedApps
            heading: window.looks.word("installed")
            emptyText: window.looks.word("nothing_installed")
            removable: true
            onAppChosen: name => window.show({ app: name })
        }
    }

    Component {
        id: updatesPage

        AppListPage {
            looks: window.looks
            apps: window.appsWithUpdates
            heading: {
                const count = window.appsWithUpdates.length
                if (count === 0) {
                    return window.looks.word("no_updates")
                }
                return count === 1 ? window.looks.word("one_update") : window.looks.word("updates_available", { count: count })
            }
            offersUpdateAll: true
            onAppChosen: name => window.show({ app: name })
        }
    }

    Component {
        id: appPage

        AppPage {
            looks: window.looks
            app: window.appNamed(window.shown.app) ?? ({})
        }
    }

    Column {
        anchors.centerIn: parent
        visible: store.apps.length === 0
        spacing: 12

        StoreText {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, window.width - 80)
            looks: window.looks
            color: window.looks.color("secondary_text_color")
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: {
                if (store.loading) {
                    return window.looks.word("loading")
                }
                return store.problem !== "" ? store.problem : window.looks.word("nothing_offered")
            }
        }

        StoreText {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !store.loading && store.problem !== ""
            looks: window.looks
            text: window.looks.word("try_again")
            font.underline: true

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: store.refresh()
            }
        }
    }
}
