#include "tasks.h"

#include <QJsonValue>
#include <QSet>
#include <algorithm>
#include <functional>

Project Project::fromJson(const QJsonObject &object) {
    Project project;
    project.id = object.value("id").toVariant().toString();
    project.name = object.value("name").toString();
    project.color = object.value("color").toString();
    project.parentId = object.value("parent_id").toVariant().toString();
    project.order = object.value("child_order").toInt(object.value("order").toInt());
    project.inbox = object.value("inbox_project").toBool() || object.value("is_inbox_project").toBool();
    return project;
}

Task Task::fromJson(const QJsonObject &object) {
    Task task;
    task.id = object.value("id").toVariant().toString();
    task.content = object.value("content").toString();
    task.description = object.value("description").toString();
    task.projectId = object.value("project_id").toVariant().toString();
    task.parentId = object.value("parent_id").toVariant().toString();
    task.priority = std::clamp(object.value("priority").toInt(1), 1, 4);
    task.childOrder = object.value("child_order").toInt(object.value("order").toInt());
    for (const auto &label : object.value("labels").toArray())
        task.labels << label.toString();

    const QJsonObject due = object.value("due").toObject();
    if (!due.isEmpty()) {
        // A due date is a plain date, a floating local time
        // (2026-11-06T17:00:00), or an instant in UTC (…Z) when the task has
        // a fixed timezone; v1 may also send the time as a separate datetime.
        QString value = due.value("datetime").toString();
        if (value.isEmpty())
            value = due.value("date").toString();
        if (value.contains('T')) {
            QDateTime moment = QDateTime::fromString(value, Qt::ISODateWithMs);
            if (!moment.isValid())
                moment = QDateTime::fromString(value, Qt::ISODate);
            if (moment.isValid()) {
                if (moment.timeSpec() != Qt::LocalTime)
                    moment = moment.toLocalTime();
                task.dueDate = moment.date();
                task.dueTime = moment.time();
            }
        } else {
            task.dueDate = QDate::fromString(value.left(10), Qt::ISODate);
        }
        task.dueString = due.value("string").toString();
        task.recurring = due.value("is_recurring").toBool();
    }
    return task;
}

bool Task::isOverdue(const QDateTime &now) const {
    if (!hasDue())
        return false;
    if (dueDate != now.date())
        return dueDate < now.date();
    return hasTime() && dueTime < now.time();
}

namespace Tasks {

QList<Task> parse(const QJsonArray &array) {
    QList<Task> tasks;
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        // Completed and deleted tasks can show up in some responses.
        if (object.value("checked").toBool() || object.value("is_deleted").toBool())
            continue;
        tasks << Task::fromJson(object);
    }
    return tasks;
}

QList<Project> parseProjects(const QJsonArray &array) {
    QList<Project> projects;
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        if (object.value("is_archived").toBool() || object.value("is_deleted").toBool())
            continue;
        projects << Project::fromJson(object);
    }
    // Todoist's child_order only ranks siblings, so this walks the tree: the
    // Inbox first, then each project followed by its sub-projects. One whose
    // parent isn't listed (archived, say) is shown at the top level.
    QSet<QString> ids;
    for (const auto &project : projects)
        ids.insert(project.id);
    QHash<QString, QList<Project>> children;
    for (const auto &project : projects)
        children[ids.contains(project.parentId) ? project.parentId : QString()] << project;
    for (auto &list : children)
        std::stable_sort(list.begin(), list.end(), [](const Project &a, const Project &b) {
            if (a.inbox != b.inbox)
                return a.inbox;
            return a.order < b.order;
        });

    QList<Project> ordered;
    QSet<QString> placed;
    std::function<void(const QString &)> walk = [&](const QString &parent) {
        for (const auto &project : children.value(parent)) {
            if (placed.contains(project.id))
                continue;
            placed.insert(project.id);
            ordered << project;
            walk(project.id);
        }
    };
    walk({});
    return ordered;
}

void sortByDue(QList<Task> &tasks) {
    std::stable_sort(tasks.begin(), tasks.end(), [](const Task &a, const Task &b) {
        if (a.hasDue() != b.hasDue())
            return a.hasDue();
        if (a.dueDate != b.dueDate)
            return a.dueDate < b.dueDate;
        if (a.hasTime() != b.hasTime())
            return a.hasTime();
        if (a.hasTime() && a.dueTime != b.dueTime)
            return a.dueTime < b.dueTime;
        if (a.priority != b.priority)
            return a.priority > b.priority;
        return a.childOrder < b.childOrder;
    });
}

QList<Task> sortAsTree(const QList<Task> &tasks, QHash<QString, int> *depths) {
    QHash<QString, QList<Task>> children;
    QSet<QString> ids;
    for (const auto &task : tasks)
        ids.insert(task.id);
    // A subtask whose parent isn't in the list (it lives in another project,
    // or was filtered out) is shown at the top level rather than lost.
    for (const auto &task : tasks)
        children[ids.contains(task.parentId) ? task.parentId : QString()] << task;
    for (auto &list : children)
        std::stable_sort(list.begin(), list.end(), [](const Task &a, const Task &b) { return a.childOrder < b.childOrder; });

    QList<Task> ordered;
    std::function<void(const QString &, int)> walk = [&](const QString &parent, int depth) {
        for (const auto &task : children.value(parent)) {
            if (depths)
                depths->insert(task.id, depth);
            ordered << task;
            if (depth < 8)
                walk(task.id, depth + 1);
        }
    };
    walk({}, 0);
    return ordered;
}

QStringList colorNames() {
    return {"berry_red", "red",  "orange", "yellow",   "olive_green", "lime_green", "green",
            "mint_green", "teal", "sky_blue", "light_blue", "blue",      "grape",      "violet",
            "lavender",  "magenta", "salmon", "charcoal", "grey",      "taupe"};
}

QString colorHex(const QString &name) {
    // Todoist's named project colors.
    static const QHash<QString, QString> colors{
        {"berry_red", "#b8255f"}, {"red", "#db4035"},      {"orange", "#ff9933"},  {"yellow", "#fad000"},
        {"olive_green", "#afb83b"}, {"lime_green", "#7ecc49"}, {"green", "#299438"},  {"mint_green", "#6accbc"},
        {"teal", "#158fad"},      {"sky_blue", "#14aaf5"},  {"light_blue", "#96c3eb"}, {"blue", "#4073ff"},
        {"grape", "#884dff"},     {"violet", "#af38eb"},    {"lavender", "#eb96eb"}, {"magenta", "#e05194"},
        {"salmon", "#ff8d85"},    {"charcoal", "#808080"},  {"grey", "#b8b8b8"},    {"taupe", "#ccac93"},
    };
    return colors.value(name, "#808080");
}

QVariantMap toVariant(const Task &task, const QDateTime &now, const QHash<QString, Project> &projects, int depth) {
    const bool overdue = task.isOverdue(now);
    const Project project = projects.value(task.projectId);
    return {
        {"id", task.id},
        {"content", task.content},
        {"description", task.description},
        {"priority", 5 - task.priority},
        {"labels", task.labels},
        {"hasDue", task.hasDue()},
        {"dueDate", task.hasDue() ? task.dueDate.toString(Qt::ISODate) : QString()},
        {"dueTime", task.hasTime() ? task.dueTime.toString("HH:mm") : QString()},
        {"dueString", task.dueString},
        {"recurring", task.recurring},
        {"dayOffset", task.hasDue() ? int(now.date().daysTo(task.dueDate)) : 0},
        {"overdue", overdue},
        {"group", overdue && task.dueDate < now.date() ? QStringLiteral("overdue")
                  : task.hasDue() ? task.dueDate.toString(Qt::ISODate) : QString()},
        {"projectId", task.projectId},
        {"projectName", project.inbox ? QString() : project.name},
        {"projectColor", colorHex(project.color)},
        {"inbox", project.inbox},
        {"depth", depth},
        {"url", task.url()},
    };
}

QJsonObject toJson(const Task &task, const QDateTime &now, const QHash<QString, Project> &projects) {
    const Project project = projects.value(task.projectId);
    QJsonObject object{
        {"id", task.id},
        {"content", task.content},
        {"description", task.description},
        {"priority", 5 - task.priority},
        {"labels", QJsonArray::fromStringList(task.labels)},
        {"overdue", task.isOverdue(now)},
        {"project", QJsonObject{{"id", task.projectId}, {"name", project.name}, {"inbox", project.inbox}}},
        {"url", task.url()},
    };
    if (task.hasDue())
        object.insert("due", QJsonObject{
            {"date", task.dueDate.toString(Qt::ISODate)},
            {"time", task.hasTime() ? QJsonValue(task.dueTime.toString("HH:mm")) : QJsonValue()},
            {"string", task.dueString},
            {"recurring", task.recurring},
        });
    else
        object.insert("due", QJsonValue());
    return object;
}

} // namespace Tasks
