#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "credentials.h"
#include "tasks.h"
#include "todoist.h"

// The app's state: which list is showing, its tasks, and the work of
// completing and adding them, kept in step with Todoist.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool tokenFromEnvironment READ tokenFromEnvironment CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY tasksChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString view READ view NOTIFY viewChanged)
    Q_PROPERTY(QString viewName READ viewName NOTIFY viewNameChanged)
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged)
    Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)
    // Every project a task can move to, the Inbox first.
    Q_PROPERTY(QVariantList destinations READ destinations NOTIFY projectsChanged)
    // Todoist's project colors: [{name: "berry_red", hex: "#b8255f"}, ...].
    Q_PROPERTY(QVariantList colors READ colors CONSTANT)
    Q_PROPERTY(int todayCount READ todayCount NOTIFY countsChanged)
    Q_PROPERTY(int inboxCount READ inboxCount NOTIFY countsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    explicit Backend(const Credentials &credentials = Credentials(), QObject *parent = nullptr);

    bool signedIn() const { return !m_client.token().isEmpty(); }
    bool tokenFromEnvironment() const { return m_credentials.fromEnvironment(); }
    bool busy() const { return m_busy > 0; }
    bool loaded() const { return m_loaded; }
    QString error() const { return m_error; }
    QString view() const { return m_view; }
    QString viewName() const;
    QVariantList tasks() const;
    QVariantList projects() const;
    QVariantList destinations() const;
    QVariantList colors() const;
    int todayCount() const { return m_todayCount; }
    int inboxCount() const { return m_inboxCount; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    // Views are "today", "upcoming", "inbox", or "project:<id>".
    Q_INVOKABLE void showView(const QString &view);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void signIn(const QString &token);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void complete(const QString &taskId);
    // Quick add, filed into the list on screen: a project view's project,
    // and today's date in Today, unless the text names its own.
    Q_INVOKABLE void add(const QString &text);
    Q_INVOKABLE void moveTask(const QString &taskId, const QString &projectId);
    // Natural language, as Todoist reads it: "tomorrow 5pm", "next monday",
    // "every friday", or "no date" to clear it.
    Q_INVOKABLE void schedule(const QString &taskId, const QString &dueString);
    Q_INVOKABLE void createProject(const QString &name, const QString &color = {});
    Q_INVOKABLE void setProjectColor(const QString &projectId, const QString &color);
    // Counts what deleting would take with it; answers with projectContents.
    Q_INVOKABLE void inspectProject(const QString &projectId);
    Q_INVOKABLE void deleteProject(const QString &projectId);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void dismissError();

    // The filter queries behind the Today and Upcoming views.
    static QString todayQuery() { return QStringLiteral("today | overdue"); }
    static QString upcomingQuery() { return QStringLiteral("overdue | 7 days"); }

signals:
    void signedInChanged();
    void busyChanged();
    void errorChanged();
    void viewChanged();
    void viewNameChanged();
    void tasksChanged();
    void projectsChanged();
    void countsChanged();
    void historyChanged();
    void signInFailed(const QString &message);
    void added(const QString &content);
    void projectCreated(const QString &name);
    void projectDeleted(const QString &name);
    // taskCount is -1 when it couldn't be counted; subprojects are names.
    void projectContents(const QString &projectId, int taskCount, const QStringList &subprojects);

private:
    struct Step {
        enum Kind { Completed, Added, Moved, Scheduled } kind;
        QString taskId;
        QString text;
        Task task;
        // For a move, the projects; for a reschedule, the due strings.
        QString from, to;
    };

    void begin();
    void end();
    void fail(const QString &message);
    void loadView();
    void loadCounts();
    void applyTasks(const QList<Task> &tasks);
    void removeLocally(const QString &taskId);
    void pushUndo(const Step &step);
    QString inboxId() const;
    QStringList descendantsOf(const QString &projectId) const;
    const Task *findTask(const QString &taskId) const;
    static QString dueStringOf(const Task &task);
    void applyMove(const Step &step, bool forward, std::function<void(const QString &)> done);
    void applySchedule(const Step &step, bool forward, std::function<void(const QString &)> done);
    void reloadSoon();

    Credentials m_credentials;
    Todoist m_client;
    QString m_view = "today";
    QList<Task> m_tasks;
    QHash<QString, int> m_depths;
    QList<Project> m_projects;
    QHash<QString, Project> m_projectsById;
    int m_todayCount = 0;
    int m_inboxCount = 0;
    int m_busy = 0;
    bool m_loaded = false;
    // Responses for a view the user has since left are dropped.
    int m_generation = 0;
    QString m_error;
    QList<Step> m_undo, m_redo;
    QTimer m_poll;
};
