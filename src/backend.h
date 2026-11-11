#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

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
    // sectionId files it into one of the list's sections; parentId makes it
    // a subtask of that task instead, wherever the task is.
    Q_INVOKABLE void add(const QString &text, const QString &sectionId = {}, const QString &parentId = {});
    // Gone for good, with its subtasks; Todoist has no way back.
    Q_INVOKABLE void deleteTask(const QString &taskId);
    // How many of its subtasks are in the list on screen.
    Q_INVOKABLE int subtaskCount(const QString &taskId) const;
    // An empty sectionId puts it in the project outside any section.
    Q_INVOKABLE void moveTask(const QString &taskId, const QString &projectId, const QString &sectionId = {});
    // Natural language, as Todoist reads it: "tomorrow 5pm", "next monday",
    // "every friday", or "no date" to clear it.
    Q_INVOKABLE void schedule(const QString &taskId, const QString &dueString);
    // Changes any of "content", "description", and "priority" (1 is p1, as
    // users count it). Unchanged fields, and an empty content, are left be.
    Q_INVOKABLE void editTask(const QString &taskId, const QVariantMap &changes);
    Q_INVOKABLE void setPriority(const QString &taskId, int priority);
    // In a project's list: makes the task a subtask of the one above it at
    // its level, or (outdent) a sibling of its parent, just after it.
    Q_INVOKABLE void indentTask(const QString &taskId);
    Q_INVOKABLE void outdentTask(const QString &taskId);
    // Moves a task among its siblings in a project's list, a section among
    // its project's, or a project among its siblings in the sidebar, by
    // delta places (-1 up, 1 down).
    Q_INVOKABLE void moveTaskBy(const QString &taskId, int delta);
    Q_INVOKABLE void moveSectionBy(const QString &sectionId, int delta);
    Q_INVOKABLE void moveProjectBy(const QString &projectId, int delta);
    // Dragged and dropped in a project's list, in front of the row at
    // rowIndex in rows (rows.length for the end). A task takes the level of
    // the task it lands in front of, or else the top of the section above;
    // a section, its place among the headings above.
    Q_INVOKABLE void dropTask(const QString &taskId, int rowIndex);
    Q_INVOKABLE void dropSection(const QString &sectionId, int rowIndex);
    // In front of the project at index in projects(), among its siblings.
    Q_INVOKABLE void dropProject(const QString &projectId, int index);
    // A task the app knows of, the list's or a loaded subtask, in the shape
    // rows have; empty when it isn't known.
    Q_INVOKABLE QVariantMap taskById(const QString &taskId) const;
    // A task's direct subtasks, in Todoist's order: the ones in the list on
    // screen, and the ones loadSubtasks fetched (a dated list doesn't show
    // them all). Changes announce subtasksChanged.
    Q_INVOKABLE QVariantList subtasksOf(const QString &taskId) const;
    Q_INVOKABLE void loadSubtasks(const QString &taskId);
    // Drops the fetched subtasks, once nothing shows them.
    Q_INVOKABLE void forgetSubtasks();
    Q_INVOKABLE void createProject(const QString &name, const QString &color = {});
    Q_INVOKABLE void setProjectColor(const QString &projectId, const QString &color);
    Q_INVOKABLE void renameProject(const QString &projectId, const QString &name);
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
    Q_INVOKABLE void renameSection(const QString &sectionId, const QString &name);
    // Undoing an add deletes the task from Todoist for good, so the UI asks
    // first: when the next undo would, this is {id, text} of that task, and
    // empty otherwise.
    Q_INVOKABLE QVariantMap undoDeletes() const;
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
    void subtasksChanged();
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
        // For ProjectMoved and ProjectEdited, taskId is the project's id; for
        // SectionEdited, the section's; for Reordered, whatever was moved.
        enum Kind { Completed, Added, Moved, Scheduled, ProjectMoved, Edited, ProjectEdited, SectionEdited, Reordered } kind;
        QString taskId = {};
        QString text = {};
        Task task = {};
        // For a move, the projects; for a reschedule, the due strings; for
        // an add, to is the project it was filed into, if it was moved there;
        // for a project's move, its parents; for a section's rename or a
        // reorder, from is the project it's in.
        QString from = {}, to = {};
        // For a move or an add, the sections, if any.
        QString fromSection = {}, toSection = {};
        // For an add made in Today, which dated it today after quick add.
        bool datedToday = false;
        // For a move, the parent tasks, if any; for an add, the task it was
        // made under.
        QString fromParent = {}, toParent = {};
        // For an edit, the fields before and after, as Todoist's update takes
        // them; for a reorder, the command's args. For a move, the
        // item_reorder args that put its new siblings in order afterwards,
        // if it needs them (an indent or outdent).
        QJsonObject before = {}, after = {};
        // For a reorder, the Sync command that makes it.
        QString command = {};
    };

    // Whether a step is about the task with its taskId, not a project or section.
    static bool aboutTask(const Step &step);

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
    // Files a task quick add just made where an add step says: its section
    // or project (to, toSection), and dated today if datedToday.
    void fileAdded(const Task &task, const Step &step, std::function<void(const ApiError &)> done);
    void applyMove(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applySchedule(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applyProjectMove(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applyEdit(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    void applyReorder(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    // Any step but a completion or an add, which undo and redo handle themselves.
    void applyStep(const Step &step, bool forward, std::function<void(const ApiError &)> done);
    // Patches every copy of a task (the list's, a loaded subtask's).
    // announce false leaves telling QML to the caller, which changes more.
    void patchTask(const QString &taskId, const std::function<void(Task &)> &patch, bool announce = true);
    // Sets the child orders an item_reorder's args name, without re-sorting;
    // false when they name none.
    bool setChildOrders(const QJsonObject &args);
    // Fetches a task's subtasks into m_subtasks, as loadSubtasks does,
    // without watching it.
    void fetchSubtasks(const QString &taskId);
    // Whether a step names any of ids: its task, its parents, or anything its
    // reorder args put in order.
    static bool mentions(const Step &step, const QSet<QString> &ids);
    // Puts the child orders in a reorder command's args into the tasks,
    // sections, or projects they name, and re-sorts what's shown.
    void applyOrders(const QString &command, const QJsonObject &args);
    // Re-sorts a project's list after its tasks' nesting or order changed.
    void resort();
    // The tasks at the same level as one in the list on screen, in the order
    // shown: the same parent, in the same section.
    QList<Task> siblingsOf(const Task &task) const;
    static QJsonObject itemOrders(const QList<Task> &tasks, bool renumber);
    // The task and its subtasks in the list on screen.
    QSet<QString> subtreeOf(const QString &taskId) const;
    QList<Project> projectSiblings(const Project &project) const;
    // Reorder steps, taking a level from old to now (each in order).
    void reorderTasks(const Task &task, const QList<Task> &old, const QList<Task> &now);
    void reorderSections(const Section &section, const QList<Section> &old, const QList<Section> &now);
    void reorderProjects(const Project &project, const QList<Project> &old, const QList<Project> &now);
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
    // Subtasks fetched for a task's details, by parent; m_watching is the
    // task whose subtasks reload with the list.
    QHash<QString, QList<Task>> m_subtasks;
    QString m_watching;
    int m_subtaskGeneration = 0;
    QTimer m_poll;
};
