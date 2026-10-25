// Status item: opens and closes the list of past notifications.

import QtQuick
import ".."

StatusItem {
    iconName: "preferences-system-notifications"
    onActivated: panel.shell.run("notification_list")
}
