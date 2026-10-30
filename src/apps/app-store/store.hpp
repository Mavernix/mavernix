// The App Store's apps, and what is being done to them, for the window.
//
// The back end (backend/app_store.py) does the work, one run for each thing:
// reading the catalog, installing an app, removing one. It reports one JSON
// object per line, which this class turns into properties. Installs and
// removals wait in a queue and run one at a time; after each, the catalog is
// read again, so `apps` always says what is installed.
//
// What the back end prints on its error output goes to the log,
// ~/.cache/mavernix/app-store.log.

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>

class Store : public QObject
{
    Q_OBJECT

    // Every app offered, as backend/catalog.py describes it, plus
    // installed_version and installed_command ("" when it isn't installed).
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // Why the catalog couldn't be read, or "". When it couldn't, `apps` still
    // holds the installed ones.
    Q_PROPERTY(QString problem READ problem NOTIFY loadingChanged)
    // What is happening to each app, by name: {state, fraction}. The state is
    // "waiting", "downloading", "installing" or "removing"; the fraction (0 to
    // 1) is how far the download is.
    Q_PROPERTY(QVariantMap jobs READ jobs NOTIFY jobsChanged)
    // What went wrong the last time something was done to an app, by name.
    Q_PROPERTY(QVariantMap failures READ failures NOTIFY jobsChanged)

  public:
    // The apps are the repositories of `organisation` that carry `topic`.
    Store(const QString& organisation, const QString& topic, QObject* parent = nullptr);
    ~Store() override;

    QVariantList apps() const { return app_list; }
    bool loading() const { return catalog_process != nullptr; }
    QString problem() const { return catalog_problem; }
    QVariantMap jobs() const { return job_states; }
    QVariantMap failures() const { return failure_texts; }

    Q_INVOKABLE void refresh();
    // Installs the app, or updates it to the version offered.
    Q_INVOKABLE void install(const QString& name);
    Q_INVOKABLE void remove(const QString& name);
    // Starts the installed app.
    Q_INVOKABLE void open(const QString& name);
    // Markdown, such as a release's notes, as plain text, for a line that has
    // room only for the start of it.
    Q_INVOKABLE QString plainText(const QString& markdown) const;

  signals:
    void appsChanged();
    void loadingChanged();
    void jobsChanged();

  private:
    struct Job
    {
        QString action; // "install" or "remove"
        QString name;
    };

    QProcess* startBackEnd(const QStringList& arguments);
    void log(const QString& text) const;
    void queue(const Job& job);
    void startNextJob();
    void handleJobReport(const QVariantMap& report);
    void jobFinished(int exit_code, QProcess::ExitStatus status);
    void setJobState(const QString& name, const QString& state, double fraction = 0);

    QString organisation;
    QString topic;

    QVariantList app_list;
    QPointer<QProcess> catalog_process;
    QString catalog_problem;
    // A refresh asked for while one runs; it runs when that one ends.
    bool refresh_again = false;

    QList<Job> waiting_jobs;
    QPointer<QProcess> job_process;
    Job running_job;
    QByteArray unread_report;
    QString job_failure;
    QVariantMap job_states;
    QVariantMap failure_texts;
};
