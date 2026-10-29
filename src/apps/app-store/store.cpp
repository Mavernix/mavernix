#include "store.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

// The back end, as installed (meson defines BACKEND_DIRECTORY).
static const QString back_end = QStringLiteral(BACKEND_DIRECTORY "/app_store.py");

// How long closing the window waits for an install to finish its swap.
static const int finish_ms = 5000;

static QString log_path()
{
    QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
    if (cache.isEmpty())
    {
        cache = QDir::homePath() + QStringLiteral("/.cache");
    }
    return cache + QStringLiteral("/mavernix/app-store.log");
}

// Each line the back end reports is one JSON object.
static QList<QVariantMap> take_reports(QByteArray& unread)
{
    QList<QVariantMap> reports;
    qsizetype end;
    while ((end = unread.indexOf('\n')) >= 0)
    {
        reports << QJsonDocument::fromJson(unread.left(end)).object().toVariantMap();
        unread.remove(0, end + 1);
    }
    return reports;
}

Store::Store(const QString& organisation, const QString& topic, QObject* parent)
    : QObject(parent), organisation(organisation), topic(topic)
{
    refresh();
}

Store::~Store()
{
    // An install stopped halfway would leave a half-made folder behind.
    if (job_process != nullptr)
    {
        // Only the running job finishes: its end mustn't start the next one,
        // or a refresh, while the window goes away.
        waiting_jobs.clear();
        disconnect(job_process, nullptr, this, nullptr);
        job_process->waitForFinished(finish_ms);
    }
}

void Store::log(const QString& text) const
{
    QFile file(log_path());
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

// Starts the back end; the process deletes itself once its `finished` handlers have run.
QProcess* Store::startBackEnd(const QStringList& arguments)
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardError, this,
        [this, process]() { log(QString::fromUtf8(process->readAllStandardError())); });
    connect(process, &QProcess::finished, process, &QObject::deleteLater, Qt::QueuedConnection);
    // A program that never started doesn't finish; say it did, so the same handlers run.
    connect(process, &QProcess::errorOccurred, process, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            log(QStringLiteral("could not start python3: %1").arg(process->errorString()));
            // Queued: start() can report this before the caller has connected its handlers.
            QMetaObject::invokeMethod(
                process, [process]() { emit process->finished(-1, QProcess::CrashExit); }, Qt::QueuedConnection);
        }
    });
    process->start(QStringLiteral("python3"), QStringList{back_end} + arguments);
    return process;
}

void Store::refresh()
{
    if (catalog_process != nullptr)
    {
        refresh_again = true;
        return;
    }
    catalog_process = startBackEnd({QStringLiteral("catalog"), organisation, topic});
    emit loadingChanged();

    QProcess* process = catalog_process;
    connect(process, &QProcess::finished, this, [this, process]() {
        QByteArray output = process->readAllStandardOutput();
        const QList<QVariantMap> reports = take_reports(output);
        const QVariantMap report = reports.isEmpty() ? QVariantMap() : reports.last();
        const QString event = report.value(QStringLiteral("event")).toString();
        if (event == QLatin1String("catalog"))
        {
            app_list = report.value(QStringLiteral("apps")).toList();
            catalog_problem = report.value(QStringLiteral("problem")).toString();
            emit appsChanged();
        }
        else if (event == QLatin1String("failed"))
        {
            catalog_problem = report.value(QStringLiteral("message")).toString();
        }
        else
        {
            catalog_problem =
                tr("The App Store couldn't read what's on offer. What went wrong is in %1.").arg(log_path());
        }

        catalog_process = nullptr;
        emit loadingChanged();
        if (refresh_again)
        {
            refresh_again = false;
            refresh();
        }
    });
}

void Store::install(const QString& name)
{
    queue({QStringLiteral("install"), name});
}

void Store::remove(const QString& name)
{
    queue({QStringLiteral("remove"), name});
}

void Store::open(const QString& name)
{
    for (const QVariant& app : app_list)
    {
        const QVariantMap details = app.toMap();
        if (details.value(QStringLiteral("name")).toString() == name)
        {
            const QString command = details.value(QStringLiteral("installed_command")).toString();
            if (command.isEmpty() || !QProcess::startDetached(command, {}))
            {
                log(QStringLiteral("could not start %1 (%2)").arg(name, command));
            }
            return;
        }
    }
}

void Store::queue(const Job& job)
{
    // Pressing the button twice doesn't do the work twice.
    if (job_states.contains(job.name))
    {
        return;
    }
    failure_texts.remove(job.name);
    waiting_jobs << job;
    setJobState(job.name, QStringLiteral("waiting"));
    startNextJob();
}

void Store::startNextJob()
{
    if (job_process != nullptr || waiting_jobs.isEmpty())
    {
        return;
    }
    running_job = waiting_jobs.takeFirst();
    unread_report.clear();
    job_failure.clear();
    setJobState(running_job.name,
        running_job.action == QLatin1String("remove") ? QStringLiteral("removing") : QStringLiteral("downloading"));
    log(QStringLiteral("%1 %2").arg(running_job.action, running_job.name));

    const QStringList arguments = running_job.action == QLatin1String("install")
                                      ? QStringList{QStringLiteral("install"), organisation, topic, running_job.name}
                                      : QStringList{QStringLiteral("remove"), running_job.name};
    job_process = startBackEnd(arguments);
    connect(job_process, &QProcess::readyReadStandardOutput, this, [this]() {
        unread_report += job_process->readAllStandardOutput();
        for (const QVariantMap& report : take_reports(unread_report))
        {
            handleJobReport(report);
        }
    });
    connect(job_process, &QProcess::finished, this, &Store::jobFinished);
}

void Store::handleJobReport(const QVariantMap& report)
{
    const QString event = report.value(QStringLiteral("event")).toString();
    if (event == QLatin1String("progress"))
    {
        setJobState(running_job.name, report.value(QStringLiteral("step")).toString(),
            report.value(QStringLiteral("fraction")).toDouble());
    }
    else if (event == QLatin1String("failed"))
    {
        job_failure = report.value(QStringLiteral("message")).toString();
    }
}

void Store::jobFinished(int exit_code, QProcess::ExitStatus status)
{
    // The last lines can arrive together with the end.
    unread_report += job_process->readAllStandardOutput();
    for (const QVariantMap& report : take_reports(unread_report))
    {
        handleJobReport(report);
    }

    const bool worked = status == QProcess::NormalExit && exit_code == 0;
    if (!worked)
    {
        if (job_failure.isEmpty())
        {
            job_failure = tr("Something went wrong. What it was is in %1.").arg(log_path());
        }
        failure_texts.insert(running_job.name, job_failure);
        log(QStringLiteral("%1 %2 failed: %3").arg(running_job.action, running_job.name, job_failure));
    }

    job_states.remove(running_job.name);
    emit jobsChanged();
    job_process = nullptr;
    refresh();
    startNextJob();
}

void Store::setJobState(const QString& name, const QString& state, double fraction)
{
    job_states.insert(name, QVariantMap{{QStringLiteral("state"), state}, {QStringLiteral("fraction"), fraction}});
    emit jobsChanged();
}
