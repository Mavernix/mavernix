#include "import-job.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <csignal>
#include <unistd.h>

// The back end, as installed (meson defines BACKEND_DIRECTORY).
static const QString back_end = QStringLiteral(BACKEND_DIRECTORY "/import_theme.py");

// No estimate of the time left before this share of the import is done and
// this much time has passed: the first moments say little about the rest.
static const double estimate_after_fraction = 0.1;
static const qint64 estimate_after_ms = 30000;

// How long the window waits, when it closes, for a stopped import to clean up.
static const int clean_up_ms = 5000;

// Each back end runs as a process group of its own, so that ending it ends
// what it started too (7-Zip, and the build when there is no sandbox).
static void own_group(QProcess& process)
{
    process.setChildProcessModifier([] { setpgid(0, 0); });
}

static void end_group(QProcess* process)
{
    if (process != nullptr && process->state() != QProcess::NotRunning)
    {
        // The back end cleans up on SIGTERM.
        ::kill(-pid_t(process->processId()), SIGTERM);
    }
}

ImportJob::ImportJob(QObject* parent) : QObject(parent)
{
    own_group(process);
    connect(&process, &QProcess::readyReadStandardOutput, this, &ImportJob::readReport);
    connect(
        &process, &QProcess::readyReadStandardError, this, [this]() { logErrors(&process, QStringLiteral("import")); });
    connect(&process, &QProcess::finished, this, &ImportJob::finished);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            log(QStringLiteral("window: could not start the import: %1").arg(process.errorString()));
            problem_text = tr("The import could not start: %1.").arg(process.errorString());
            setState(QStringLiteral("failed"));
        }
    });
}

ImportJob::~ImportJob()
{
    // An import left running would go on without a window; stop it.
    stop();
    process.waitForFinished(clean_up_ms);
    end_group(check_process);
    end_group(identify_process);
}

QString ImportJob::logPath() const
{
    // As the back end names it.
    QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
    if (cache.isEmpty())
    {
        cache = QDir::homePath() + QStringLiteral("/.cache");
    }
    return cache + QStringLiteral("/mavernix/theme-installer.log");
}

void ImportJob::log(const QString& text) const
{
    QFile file(logPath());
    QDir().mkpath(QFileInfo(file).path());
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    for (const QString& line : text.trimmed().split(QLatin1Char('\n')))
    {
        file.write((stamp + QLatin1Char(' ') + line + QLatin1Char('\n')).toUtf8());
    }
}

// The back end's error output goes to the log, not to whoever started the window.
void ImportJob::logErrors(QProcess* process, const QString& what) const
{
    const QString text = QString::fromUtf8(process->readAllStandardError());
    if (!text.trimmed().isEmpty())
    {
        log(what + QStringLiteral(": ") + text);
    }
}

// Starts the back end with `arguments`; the process ends itself (deleteLater)
// once it is done and its `finished` handlers have run.
QProcess* ImportJob::startBackEnd(const QStringList& arguments, const QString& what)
{
    auto* started = new QProcess(this);
    own_group(*started);
    connect(started, &QProcess::readyReadStandardError, this, [this, started, what]() { logErrors(started, what); });
    connect(started, &QProcess::finished, started, &QObject::deleteLater, Qt::QueuedConnection);
    // A program that never started does not finish; say it did, so that the
    // same handlers tell the window (they look at error() for the reason).
    connect(started, &QProcess::errorOccurred, started, [started](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            emit started->finished(-1, QProcess::CrashExit);
        }
    });
    started->start(QStringLiteral("python3"), QStringList{back_end} + arguments);
    return started;
}

void ImportJob::check()
{
    if (check_process != nullptr)
    {
        return;
    }
    check_process = startBackEnd({QStringLiteral("check")}, QStringLiteral("check"));
    emit checkingChanged();
    QProcess* checking = check_process;
    connect(checking, &QProcess::finished, this, [this, checking](int exit_code, QProcess::ExitStatus status) {
        const QVariantMap answer = QJsonDocument::fromJson(checking->readAllStandardOutput()).object().toVariantMap();
        check_problem.clear();
        if (checking->error() == QProcess::FailedToStart)
        {
            // Without Python nothing else can be asked.
            missing = {QVariantMap{{QStringLiteral("program"), QStringLiteral("python3")},
                {QStringLiteral("package"), QStringLiteral("python")},
                {QStringLiteral("purpose"), QStringLiteral("runs the import")}}};
            log(QStringLiteral("window: python3 could not be started"));
        }
        else if (status != QProcess::NormalExit || exit_code != 0 || answer.isEmpty())
        {
            check_problem =
                tr("The Theme Installer could not check this computer. What went wrong is in %1.").arg(logPath());
            log(QStringLiteral("window: the check failed (status %1)").arg(exit_code));
        }
        else
        {
            missing = answer.value(QStringLiteral("missing")).toList();
            has_sandbox = answer.value(QStringLiteral("sandbox")).toBool();
            sandbox_installed = answer.value(QStringLiteral("sandbox_installed")).toBool();
            sandbox_package = answer.value(QStringLiteral("sandbox_package")).toString();
            look_list = answer.value(QStringLiteral("looks")).toList();
            last_installer = answer.value(QStringLiteral("last_installer")).toString();
            QStringList lacking;
            for (const QVariant& entry : missing)
            {
                lacking << entry.toMap().value(QStringLiteral("program")).toString();
            }
            log(QStringLiteral("window: checked this computer; missing: %1; sandbox: %2")
                    .arg(lacking.isEmpty() ? QStringLiteral("nothing") : lacking.join(QStringLiteral(", ")),
                        has_sandbox ? QStringLiteral("yes") : QStringLiteral("no")));
        }
        check_process = nullptr;
        emit checked();
        emit checkingChanged();
    });
}

void ImportJob::identify(const QString& path)
{
    // The same file again (the window asks when it opens and when the
    // choice changes): what is known or under way holds.
    if (path == identified_path && (identify_process != nullptr || !identified_answer.isEmpty()))
    {
        return;
    }
    if (identify_process != nullptr)
    {
        log(QStringLiteral("window: stopped reading %1").arg(identified_path));
        end_group(identify_process);
        // Its answer, if it still comes, is for a path no longer chosen.
        identify_process->disconnect(this);
        identify_process = nullptr;
    }
    identified_path = path;
    identified_answer.clear();
    if (path.isEmpty())
    {
        emit identifiedChanged();
        return;
    }

    log(QStringLiteral("window: reading %1").arg(path));
    identify_time.start();
    identify_process = startBackEnd({QStringLiteral("identify"), path}, QStringLiteral("identify"));
    QProcess* identifying = identify_process;
    connect(
        identifying, &QProcess::finished, this, [this, identifying, path](int exit_code, QProcess::ExitStatus status) {
        QVariantMap answer = QJsonDocument::fromJson(identifying->readAllStandardOutput()).object().toVariantMap();
        if (status != QProcess::NormalExit || exit_code != 0 || answer.isEmpty())
        {
            answer = {{QStringLiteral("problem"), tr("The Theme Installer could not read %1. What went wrong is in %2.")
                                                      .arg(QFileInfo(path).fileName(), logPath())}};
        }
        const QString outcome = answer.contains(QStringLiteral("look"))
                                    ? QStringLiteral("makes ") + answer.value(QStringLiteral("look")).toString()
                                    : answer.value(QStringLiteral("problem")).toString();
        log(QStringLiteral("window: read %1 in %2 s: %3")
                .arg(path)
                .arg(identify_time.elapsed() / 1000.0, 0, 'f', 1)
                .arg(outcome));
        identified_answer = answer;
        identify_process = nullptr;
        emit identifiedChanged();
    });
    emit identifiedChanged();
}

void ImportJob::start(const QString& path)
{
    if (job_state == QLatin1String("running"))
    {
        return;
    }
    stage_list.clear();
    current_stage.clear();
    stage_fraction = 0;
    overall_fraction = 0;
    current_detail.clear();
    seconds_left = -1;
    problem_text.clear();
    note_list.clear();
    unread.clear();
    emit stagesChanged();
    emit progressChanged();

    log(QStringLiteral("window: importing %1").arg(path));
    running_time.start();
    setState(QStringLiteral("running"));
    process.start(QStringLiteral("python3"), {back_end, QStringLiteral("import"), path});
}

void ImportJob::stop()
{
    if (process.state() == QProcess::NotRunning)
    {
        return;
    }
    log(QStringLiteral("window: the import was stopped"));
    setState(QStringLiteral("stopped"));
    end_group(&process);
}

void ImportJob::useDesktop(const QString& name)
{
    if (switch_process != nullptr)
    {
        return;
    }
    switch_problem.clear();
    log(QStringLiteral("window: switching to the desktop %1").arg(name));
    switch_process = new QProcess(this);
    QProcess* switching = switch_process;
    connect(switching, &QProcess::finished, switching, &QObject::deleteLater, Qt::QueuedConnection);
    connect(switching, &QProcess::errorOccurred, this, [this, switching](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
        {
            return;
        }
        switch_problem = tr("The desktop could not be switched: mavernix-switch could not be started (%1). "
                            "Choose it in Settings, Theme, or log out and in.")
                             .arg(switching->errorString());
        log(QStringLiteral("window: ") + switch_problem);
        switch_process = nullptr;
        switching->deleteLater();
        emit switchChanged();
    });
    connect(switching, &QProcess::finished, this, [this, switching, name](int exit_code, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(switching->readAllStandardOutput()).trimmed();
        const QString errors = QString::fromUtf8(switching->readAllStandardError()).trimmed();
        if (!output.isEmpty())
        {
            log(QStringLiteral("switch: ") + output);
        }
        if (!errors.isEmpty())
        {
            log(QStringLiteral("switch: ") + errors);
        }
        switch_process = nullptr;
        if (status == QProcess::NormalExit && exit_code == 0)
        {
            log(QStringLiteral("window: switched to %1").arg(name));
            emit switchChanged();
            emit switched();
            return;
        }
        switch_problem =
            tr("The desktop could not be switched: %1")
                .arg(errors.isEmpty() ? tr("mavernix-switch ended with status %1.").arg(exit_code) : errors);
        log(QStringLiteral("window: switching failed with status %1").arg(exit_code));
        emit switchChanged();
    });
    switching->start(QStringLiteral("mavernix-switch"), {name});
    emit switchChanged();
}

QVariantMap ImportJob::describe(const QString& path) const
{
    const QFileInfo info(path);
    if (!info.exists())
    {
        return {};
    }
    QVariantMap described{{QStringLiteral("name"), info.fileName()}};
    if (info.isFile())
    {
        described.insert(QStringLiteral("size"), QLocale().formattedDataSize(info.size()));
    }
    return described;
}

// Both start programs of their own (detached), because the window that asks
// closes right afterwards and would end a child process with it.
void ImportJob::putOff(const QString& look_id) const
{
    log(QStringLiteral("window: offering a new import of %1 is put off until its next update").arg(look_id));
    QProcess::startDetached(QStringLiteral("python3"), {back_end, QStringLiteral("later"), look_id});
}

void ImportJob::openInstaller(const QString& path) const
{
    const QStringList arguments = path.isEmpty() ? QStringList() : QStringList{path};
    QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
}

void ImportJob::readReport()
{
    unread += process.readAllStandardOutput();
    int end;
    while ((end = unread.indexOf('\n')) >= 0)
    {
        const QByteArray line = unread.left(end);
        unread.remove(0, end + 1);
        const QJsonDocument parsed = QJsonDocument::fromJson(line);
        if (parsed.isObject())
        {
            handle(parsed.object().toVariantMap());
        }
    }
}

void ImportJob::handle(const QVariantMap& event)
{
    const QString kind = event.value(QStringLiteral("event")).toString();
    if (kind == QLatin1String("plan"))
    {
        stage_list.clear();
        for (const QVariant& stage : event.value(QStringLiteral("stages")).toList())
        {
            QVariantMap entry = stage.toMap();
            entry.insert(QStringLiteral("state"), QStringLiteral("waiting"));
            stage_list.append(entry);
        }
        emit stagesChanged();
    }
    else if (kind == QLatin1String("stage"))
    {
        if (!current_stage.isEmpty())
        {
            setStageState(current_stage, QStringLiteral("done"));
        }
        current_stage = event.value(QStringLiteral("id")).toString();
        stage_fraction = 0;
        current_detail.clear();
        setStageState(current_stage, QStringLiteral("running"));
        updateFraction();
    }
    else if (kind == QLatin1String("progress"))
    {
        stage_fraction = event.value(QStringLiteral("fraction")).toDouble();
        updateFraction();
    }
    else if (kind == QLatin1String("detail"))
    {
        current_detail = event.value(QStringLiteral("text")).toString();
        emit progressChanged();
    }
    else if (kind == QLatin1String("note"))
    {
        note_list.append(event.value(QStringLiteral("text")).toString());
    }
    else if (kind == QLatin1String("done"))
    {
        setStageState(current_stage, QStringLiteral("done"));
        stage_fraction = 1;
        updateFraction();
    }
    else if (kind == QLatin1String("error"))
    {
        problem_text = event.value(QStringLiteral("message")).toString();
    }
}

void ImportJob::setStageState(const QString& id, const QString& state)
{
    for (QVariant& stage : stage_list)
    {
        QVariantMap entry = stage.toMap();
        if (entry.value(QStringLiteral("id")).toString() == id)
        {
            entry.insert(QStringLiteral("state"), state);
            stage = entry;
        }
    }
    emit stagesChanged();
}

// The whole import's fraction: the stages done, and the share of the
// current one, each counted by its weight.
void ImportJob::updateFraction()
{
    double total = 0;
    double done = 0;
    for (const QVariant& stage : stage_list)
    {
        const QVariantMap entry = stage.toMap();
        const double weight = entry.value(QStringLiteral("weight")).toDouble();
        total += weight;
        const QString state = entry.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("done"))
        {
            done += weight;
        }
        else if (state == QLatin1String("running"))
        {
            done += weight * stage_fraction;
        }
    }
    overall_fraction = total > 0 ? done / total : 0;

    // The time left at the pace so far.
    if (overall_fraction > estimate_after_fraction && running_time.elapsed() > estimate_after_ms &&
        overall_fraction < 1)
    {
        const double elapsed_s = running_time.elapsed() / 1000.0;
        seconds_left = int(elapsed_s / overall_fraction * (1 - overall_fraction));
    }
    else
    {
        seconds_left = -1;
    }
    emit progressChanged();
}

void ImportJob::finished(int exit_code, QProcess::ExitStatus status)
{
    readReport();
    logErrors(&process, QStringLiteral("import"));
    if (job_state != QLatin1String("running"))
    {
        // Stopped, or it never started.
        return;
    }
    if (status == QProcess::NormalExit && exit_code == 0)
    {
        log(QStringLiteral("window: the import finished in %1 s").arg(running_time.elapsed() / 1000));
        // The looks' "installed" marks change with the import.
        check();
        setState(QStringLiteral("done"));
        return;
    }
    log(QStringLiteral("window: the import failed (status %1)").arg(exit_code));
    if (problem_text.isEmpty())
    {
        problem_text = tr("The import stopped unexpectedly. Its log is in %1.").arg(logPath());
    }
    setState(QStringLiteral("failed"));
}

void ImportJob::setState(const QString& state)
{
    job_state = state;
    emit stateChanged();
}
