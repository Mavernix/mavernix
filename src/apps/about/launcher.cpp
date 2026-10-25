#include "launcher.hpp"

#include <QProcess>

#include <cstdio>

void Launcher::run(const QStringList& command) const
{
    if (command.isEmpty())
    {
        return;
    }
    if (!QProcess::startDetached(command.first(), command.mid(1)))
    {
        std::fprintf(stderr,
            "mavernix-about: could not start \"%s\". Check the command "
            "in the desktop config's [about] table.\n",
            qPrintable(command.join(QLatin1Char(' '))));
    }
}
