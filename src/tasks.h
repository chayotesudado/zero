#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Todoist's tasks and projects as the app uses them, parsed from the API v1
// JSON. Everything here is pure, so tests drive it without a network.

struct Project {
    QString id;
    QString name;
    QString color;
    QString parentId;
    int order = 0;
    bool inbox = false;

    static Project fromJson(const QJsonObject &object);
};

// A heading inside a project, which Todoist calls a section.
struct Section {
    QString id;
    QString name;
    QString projectId;
    int order = 0;

    static Section fromJson(const QJsonObject &object);
};

struct Task {
    QString id;
    QString content;
    QString description;
    QString projectId;
    QString parentId;
    QString sectionId;
    QStringList labels;
    // Todoist's API counts priority upside down: 4 is what users call p1.
    int priority = 1;
    int childOrder = 0;
    QDate dueDate;
    QTime dueTime;
    QString dueString;
    bool recurring = false;

    bool hasDue() const { return dueDate.isValid(); }
    bool hasTime() const { return dueTime.isValid(); }
    // Past its date, or due earlier today at a time that has passed.
    bool isOverdue(const QDateTime &now) const;
    QString url() const { return "https://app.todoist.com/app/task/" + id; }

    static Task fromJson(const QJsonObject &object);
};

namespace Tasks {

QList<Task> parse(const QJsonArray &array);
QList<Project> parseProjects(const QJsonArray &array);
// Live sections, each project's in Todoist's order.
QList<Section> parseSections(const QJsonArray &array);

// The order the Today and Upcoming views list tasks in: by when they're due,
// timed tasks before untimed ones on the same day, then by priority.
void sortByDue(QList<Task> &tasks);

// The order a project lists tasks in: Todoist's own order, with each subtask
// under its parent. Fills depths with each task's nesting level.
QList<Task> sortAsTree(const QList<Task> &tasks, QHash<QString, int> *depths);

// A project's list: the tasks in no section first, then each section's in
// the sections' order, each group as a tree. A task in a section that isn't
// listed counts as in none.
QList<Task> sortBySection(const QList<Task> &tasks, const QList<Section> &sections, QHash<QString, int> *depths);

// The color Todoist names a project with, as hex.
QString colorHex(const QString &name);
// Todoist's project colors by name, in the order its own picker shows them.
QStringList colorNames();

// What QML and the command line show for a task. group is "overdue", or the
// due date as yyyy-MM-dd, or "" when it has no date.
QVariantMap toVariant(const Task &task, const QDateTime &now, const QHash<QString, Project> &projects, int depth = 0);
QJsonObject toJson(const Task &task, const QDateTime &now, const QHash<QString, Project> &projects);

} // namespace Tasks
