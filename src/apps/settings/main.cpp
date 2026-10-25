// The desktop's settings window.
//
//   mavernix-settings [PANE]
//
// Shows the overview of all panes, or the pane named ("dock", "desktop").
// The session's settings are found through DESKTOP_SHELL_SETTINGS.

#include <QApplication>
#include <QFont>

#include "choices.hpp"
#include "settings-window.hpp"

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    // The dock and the panel recognise the windows by their desktop entry.
    QApplication::setDesktopFileName(QStringLiteral("mavernix-settings"));

    Choices choices;
    // The theme's interface face, as in the shell; else the toolkit's.
    const QVariantMap face = choices.themeTable(QStringLiteral("fonts")).value(QStringLiteral("interface")).toMap();
    if (face.contains(QStringLiteral("family")))
    {
        QFont font(face.value(QStringLiteral("family")).toString());
        font.setPixelSize(face.value(QStringLiteral("size"), 13).toInt());
        QApplication::setFont(font);
    }
    SettingsWindow window(choices);
    const QStringList arguments = application.arguments();
    if (arguments.size() > 1)
    {
        window.showPane(arguments[1]);
    }
    window.show();

    return application.exec();
}
