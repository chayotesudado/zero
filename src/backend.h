#pragma once

#include <QDateTime>
#include <QHash>
#include <QSet>
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
    // What the list shows: tasks, and in a project or the Inbox, a heading
    // for each section ({kind: "section"}) before its tasks. Each row has a
    // unique key ("task:<id>", "section:<id>").
    Q_PROPERTY(QVariantList rows READ rows NOTIFY tasksChanged)
    Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)
    // Every project a task can move to, the Inbox first, each followed by
    // its sections: {key, kind: "project"|"section", projectId, sectionId,
    // name, path, depth, color, inbox}.
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
    QVariantList rows() const;
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
    // sectionId files it into one of the list's sections.
    Q_INVOKABLE void add(const QString &text, const QString &sectionId = {});
    // Gone for good, with its subtasks; Todoist has no way back.
    Q_INVOKABLE void deleteTask(const QString &taskId);
    // How many of its subtasks are in the list on screen.
    Q_INVOKABLE int subtaskCount(const QString &taskId) const;
    // An empty sectionId puts it in the project outside any section.
    Q_INVOKABLE void moveTask(const QString &taskId, const QString &projectId, const QString &sectionId = {});
    // Natural language, as Todoist reads it: "tomorrow 5pm", "next monday",
    // "every friday", or "no date" to clear it.
    Q_INVOKABLE void schedule(const QString &taskId, const QString &dueString);
    Q_INVOKABLE void createProject(const QString &name, const QString &color = {});
    Q_INVOKABLE void setProjectColor(const QString &projectId, const QString &color);
    // Counts what deleting would take with it; answers with projectContents.
    Q_INVOKABLE void inspectProject(const QString &projectId);
    Q_INVOKABLE void deleteProject(const QString &projectId);
    // Makes it a sub-project of parentId, or top-level when that's empty.
    Q_INVOKABLE void moveProject(const QString &projectId, const QString &parentId);
    // Where a project can move: top level, then every project but the Inbox,
    // itself, and its own sub-projects. Same shape as destinations.
    Q_INVOKABLE QVariantList parentChoices(const QString &projectId) const;
    // A section in the project or Inbox on screen.
    Q_INVOKABLE void createSection(const QString &name);
    // Todoist deletes the section's tasks with it.
    Q_INVOKABLE void deleteSection(const QString &sectionId);
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
    void projectMoved(const QString &name);
    void taskDeleted(const QString &content);
    void sectionCreated(const QString &name);
    void sectionDeleted(const QString &name);
    // taskCount is -1 when it couldn't be counted; subprojects are names.
    void projectContents(const QString &projectId, int taskCount, const QStringList &subprojects);

private:
    struct Step {
        // For ProjectMoved, taskId is the project's id.
        enum Kind { Completed, Added, Moved, Scheduled, ProjectMoved } kind;
        QString taskId;
        QString text;
        Task task;
        // For a move, the projects; for a reschedule, the due strings; for
        // an add, to is the project it was filed into, if it was moved there;
        // for a project's move, its parents.
        QString from, to;
        // For a move or an add, the sections, if any.
        QString fromSection = {}, toSection = {};
    };

    void begin();
    void end();
    void fail(const QString &message);
    void loadFailed(const QString &message);
    void loadSucceeded();
    void loadView();
    void loadCounts();
    void applyTasks(const QList<Task> &tasks);
    // Returns the ids removed: the task and its subtasks.
    QSet<QString> removeLocally(const QString &taskId);
    void dropSteps(const std::function<bool(const Step &)> &gone);
    QString viewProjectId() const;
    QList<Section> sectionsOf(const QString &projectId) const;
    void pushUndo(const Step &step);
    QString inboxId() const;
    QStringList descendantsOf(const QString &projectId) const;
    const Task *findTask(const QString &taskId) const;
    static QString dueStringOf(const Task &task);
    void applyMove(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applySchedule(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applyProjectMove(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void undoFailed(const Step &step, const ApiError &error);
    void reloadSoon();

    Credentials m_credentials;
    Todoist m_client;
    QString m_view = "today";
    QList<Task> m_tasks;
    QHash<QString, int> m_depths;
    QList<Project> m_projects;
    QHash<QString, Project> m_projectsById;
    QList<Section> m_sections;
    int m_todayCount = 0;
    int m_inboxCount = 0;
    int m_busy = 0;
    bool m_loaded = false;
    // Responses for a view the user has since left are dropped.
    int m_generation = 0;
    // The view on screen asked to load before there were projects to load it with.
    bool m_viewWaiting = false;
    QString m_error;
    // The error a failed load showed, which the next good load clears.
    QString m_loadError;
    QList<Step> m_undo, m_redo;
    QTimer m_poll;
};
