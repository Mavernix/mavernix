// Runs the theme import (backend/import_theme.py) and tells QML how it goes.
//
// The back end reports one JSON object per line on its standard output
// (the plan of stages, each start, progress, detail, the end); this class
// turns that into properties for the window. Nothing here waits for the back
// end: checking the machine, reading an installer's list of files (minutes,
// from a slow disk) and switching desktop all run beside the window.
//
// What happens is written to the log the back end writes too,
// ~/.cache/mavernix/theme-installer.log.

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
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

    // Whether the machine is being checked; the properties below are known
    // once it is done.
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    // Why the check itself failed, or "".
    Q_PROPERTY(QString checkProblem READ checkProblem NOTIFY checked)
    // What the machine lacks for an import: [{program, package, purpose}],
    // and whether the installer can be read in a sandbox.
    Q_PROPERTY(QVariantList missingPrograms READ missingPrograms NOTIFY checked)
    Q_PROPERTY(bool sandbox READ sandbox NOTIFY checked)
    Q_PROPERTY(bool sandboxInstalled READ sandboxInstalled NOTIFY checked)
    Q_PROPERTY(QString sandboxPackage READ sandboxPackage NOTIFY checked)
    // The looks the back end can make: [{id, name, description, source,
    // drop_hint, licence_note, desktop, installed, import_brings,
    // offer_import}]; import_brings lists what a new import would bring an
    // imported look, offer_import whether to offer it (backend/updates.py).
    Q_PROPERTY(QVariantList looks READ looks NOTIFY checked)
    // The installer last imported from, if it is still there; else "".
    Q_PROPERTY(QString lastInstaller READ lastInstaller NOTIFY checked)

    // The installer last given to identify(), whether it is still being
    // read, and which look it makes: {look: id} or {problem: text}, or {}
    // while it is read.
    Q_PROPERTY(QString identifiedPath READ identifiedPath NOTIFY identifiedChanged)
    Q_PROPERTY(bool identifying READ identifying NOTIFY identifiedChanged)
    Q_PROPERTY(QVariantMap identified READ identified NOTIFY identifiedChanged)

    // Whether useDesktop() is under way, and why it failed, or "".
    Q_PROPERTY(bool switching READ switching NOTIFY switchChanged)
    Q_PROPERTY(QString switchProblem READ switchProblem NOTIFY switchChanged)

    Q_PROPERTY(QString logPath READ logPath CONSTANT)

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
    bool checking() const { return check_process != nullptr; }
    QString checkProblem() const { return check_problem; }
    QVariantList missingPrograms() const { return missing; }
    bool sandbox() const { return has_sandbox; }
    bool sandboxInstalled() const { return sandbox_installed; }
    QString sandboxPackage() const { return sandbox_package; }
    QVariantList looks() const { return look_list; }
    QString lastInstaller() const { return last_installer; }
    QString identifiedPath() const { return identified_path; }
    bool identifying() const { return identify_process != nullptr; }
    QVariantMap identified() const { return identified_answer; }
    bool switching() const { return switch_process != nullptr; }
    QString switchProblem() const { return switch_problem; }
    QString logPath() const;

    // Asks the back end what is missing on this machine.
    Q_INVOKABLE void check();
    // `path` is a file or the .app folder.
    Q_INVOKABLE void start(const QString& path);
    Q_INVOKABLE void stop();
    // Works out which look `path` makes (see `identified`). Another path, or
    // "", stops reading the one before.
    Q_INVOKABLE void identify(const QString& path);
    // Switches the running session to the desktop called `name`; `switched`
    // when it has.
    Q_INVOKABLE void useDesktop(const QString& name);

    // {name, size in words for a file}, or {} if there is nothing at `path`.
    Q_INVOKABLE QVariantMap describe(const QString& path) const;

    // Stops offering a new import of these looks until an update brings
    // something more.
    Q_INVOKABLE void putOff(const QStringList& look_ids) const;
    // Opens the Theme Installer's own window, with `path` chosen if it is
    // not "".
    Q_INVOKABLE void openInstaller(const QString& path) const;

  signals:
    void stagesChanged();
    void progressChanged();
    void stateChanged();
    void checkingChanged();
    void checked();
    void identifiedChanged();
    void switchChanged();
    void switched();

  private:
    QProcess* startBackEnd(const QStringList& arguments, const QString& what);
    void log(const QString& text) const;
    void logErrors(QProcess* process, const QString& what) const;
    void readReport();
    void handle(const QVariantMap& event);
    void setStageState(const QString& id, const QString& state);
    void updateFraction();
    void finished(int exit_code, QProcess::ExitStatus status);
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

    QPointer<QProcess> check_process;
    QString check_problem;
    QVariantList missing;
    bool has_sandbox = false;
    bool sandbox_installed = false;
    QString sandbox_package;
    QVariantList look_list;
    QString last_installer;

    QPointer<QProcess> identify_process;
    QString identified_path;
    QVariantMap identified_answer;
    QElapsedTimer identify_time;

    QPointer<QProcess> switch_process;
    QString switch_problem;
};
