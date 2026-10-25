// Runs the theme import (backend/import_theme.py) and tells QML how it goes.
//
// The back end reports one JSON object per line on its standard output
// (the plan of stages, each start, progress, detail, the end); this class
// turns that into properties for the window.

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>

class ImportJob : public QObject
{
    Q_OBJECT

    // Each {id, label, state}; state is "waiting", "running" or "done".
    Q_PROPERTY(QVariantList stages READ stages NOTIFY stagesChanged)
    // From 0 to 1.
    Q_PROPERTY(double fraction READ fraction NOTIFY progressChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY progressChanged)
    // -1 while there is no estimate yet.
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY progressChanged)
    // "idle", "running", "done", "failed" or "stopped".
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY stateChanged)
    // What the import wants the user to know at the end, besides how it ended.
    Q_PROPERTY(QStringList notes READ notes NOTIFY stateChanged)

    // What the machine lacks for an import: [{program, package, purpose}],
    // and whether the installer can be read in a sandbox.
    Q_PROPERTY(QVariantList missingPrograms READ missingPrograms NOTIFY checked)
    Q_PROPERTY(bool sandbox READ sandbox NOTIFY checked)
    Q_PROPERTY(bool sandboxInstalled READ sandboxInstalled NOTIFY checked)
    Q_PROPERTY(QString sandboxPackage READ sandboxPackage NOTIFY checked)
    // The looks the back end can make: [{id, name, description, source,
    // drop_hint, licence_note, desktop, installed}].
    Q_PROPERTY(QVariantList looks READ looks NOTIFY checked)

  public:
    explicit ImportJob(QObject* parent = nullptr);
    ~ImportJob() override;

    QVariantList stages() const { return stage_list; }
    double fraction() const { return overall_fraction; }
    QString detail() const { return current_detail; }
    int secondsLeft() const { return seconds_left; }
    QString state() const { return job_state; }
    QString problem() const { return problem_text; }
    QStringList notes() const { return note_list; }
    QVariantList missingPrograms() const { return missing; }
    bool sandbox() const { return has_sandbox; }
    bool sandboxInstalled() const { return sandbox_installed; }
    QString sandboxPackage() const { return sandbox_package; }
    QVariantList looks() const { return look_list; }

    // Asks the back end what is missing on this machine.
    Q_INVOKABLE void check();
    // `path` is a file or the .app folder.
    Q_INVOKABLE void start(const QString& path);
    Q_INVOKABLE void stop();
    // Which look `path` makes: {look: id} or {problem: text}. Takes a second
    // or two: only the image's list of files is read.
    Q_INVOKABLE QVariantMap identify(const QString& path) const;
    // Switches the running session to the desktop called `name`.
    Q_INVOKABLE void useDesktop(const QString& name) const;

    // {name, size in words for a file}, or {} if there is nothing at `path`.
    Q_INVOKABLE QVariantMap describe(const QString& path) const;

  signals:
    void stagesChanged();
    void progressChanged();
    void stateChanged();
    void checked();

  private:
    void readReport();
    void handle(const QVariantMap& event);
    void setStageState(const QString& id, const QString& state);
    void updateFraction();
    void finished(int exit_code);
    void setState(const QString& state);

    QProcess process;
    QByteArray unread;
    QVariantList stage_list;
    QString current_stage;
    double stage_fraction = 0;
    double overall_fraction = 0;
    QString current_detail;
    int seconds_left = -1;
    QElapsedTimer running_time;
    QString job_state = QStringLiteral("idle");
    QString problem_text;
    QStringList note_list;
    QVariantList missing;
    bool has_sandbox = false;
    bool sandbox_installed = false;
    QString sandbox_package;
    QVariantList look_list;
};
