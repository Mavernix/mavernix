#include "import-job.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

// The back end, as installed (meson defines BACKEND_DIRECTORY).
static const QString back_end = QStringLiteral(BACKEND_DIRECTORY "/import_theme.py");

// No estimate of the time left before this share of the import is done and
// this much time has passed: the first moments say little about the rest.
static const double estimate_after_fraction = 0.1;
static const qint64 estimate_after_ms = 30000;

ImportJob::ImportJob(QObject* parent) : QObject(parent)
{
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&process, &QProcess::readyReadStandardOutput, this, &ImportJob::readReport);
    connect(&process, &QProcess::finished, this, [this](int exit_code) { finished(exit_code); });
}

ImportJob::~ImportJob()
{
    // An import left running would go on without a window; stop it.
    stop();
    process.waitForFinished(5000);
}

void ImportJob::check()
{
    QProcess checking;
    checking.start(QStringLiteral("python3"), {back_end, QStringLiteral("check")});
    checking.waitForFinished(30000);
    const QVariantMap answer = QJsonDocument::fromJson(checking.readAllStandardOutput()).object().toVariantMap();
    missing = answer.value(QStringLiteral("missing")).toList();
    has_sandbox = answer.value(QStringLiteral("sandbox")).toBool();
    sandbox_installed = answer.value(QStringLiteral("sandbox_installed")).toBool();
    sandbox_package = answer.value(QStringLiteral("sandbox_package")).toString();
    look_list = answer.value(QStringLiteral("looks")).toList();
    if (checking.exitCode() != 0 || answer.isEmpty())
    {
        missing = {QVariantMap{{QStringLiteral("program"), QStringLiteral("python3")},
            {QStringLiteral("package"), QStringLiteral("python")},
            {QStringLiteral("purpose"), QStringLiteral("runs the import")}}};
    }
    emit checked();
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
    setState(QStringLiteral("stopped"));
    // The back end cleans up on SIGTERM; QProcess::terminate sends it.
    process.terminate();
}

QVariantMap ImportJob::identify(const QString& path) const
{
    QProcess identifying;
    identifying.start(QStringLiteral("python3"), {back_end, QStringLiteral("identify"), path});
    identifying.waitForFinished(30000);
    const QVariantMap answer = QJsonDocument::fromJson(identifying.readAllStandardOutput()).object().toVariantMap();
    if (answer.isEmpty())
    {
        return {
            {QStringLiteral("problem"), tr("The Theme Installer could not read %1.").arg(QFileInfo(path).fileName())}};
    }
    return answer;
}

void ImportJob::useDesktop(const QString& name) const
{
    QProcess::startDetached(QStringLiteral("mavernix-switch"), {name});
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

void ImportJob::finished(int exit_code)
{
    readReport();
    if (job_state == QLatin1String("stopped"))
    {
        return;
    }
    if (exit_code == 0)
    {
        // The looks' "installed" marks change with the import.
        check();
        setState(QStringLiteral("done"));
        return;
    }
    if (problem_text.isEmpty())
    {
        problem_text = tr("The import stopped unexpectedly. Its log is in %1.")
                           .arg(QDir::homePath() + QStringLiteral("/.cache/mavernix/theme-installer.log"));
    }
    setState(QStringLiteral("failed"));
}

void ImportJob::setState(const QString& state)
{
    job_state = state;
    emit stateChanged();
}
