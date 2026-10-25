// Status item: opens and closes the search field.

import QtQuick
import ".."

StatusItem {
    iconName: "edit-find"
    held: panel.shell.searchOpen
    onActivated: panel.shell.run("search")
}
