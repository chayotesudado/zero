#include "backend.h"

#include <QCoreApplication>
#include <algorithm>
#include <memory>

Backend::Backend(const Credentials &credentials, QObject *parent)
    : QObject(parent), m_credentials(credentials) {
    m_client.setToken(m_credentials.read());
    // Changes made in Todoist elsewhere (phone, web, the bar) show up here
    // within a couple of minutes, and the day rolls over on its own.
    m_poll.setInterval(120000);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    m_poll.start();
    if (signedIn())
        QTimer::singleShot(0, this, &Backend::refresh);
}

QString Backend::viewName() const {
    if (m_view == "today")
        return tr("Today");
    if (m_view == "upcoming")
        return tr("Upcoming");
    if (m_view == "inbox")
        return tr("Inbox");
    if (m_view.startsWith("project:"))
        return m_projectsById.value(m_view.mid(8)).name;
    return {};
}

QVariantList Backend::tasks() const {
    const QDateTime now = QDateTime::currentDateTime();
    QVariantList list;
    for (const auto &task : m_tasks)
        list << Tasks::toVariant(task, now, m_projectsById, m_depths.value(task.id));
    return list;
}

QVariantList Backend::projects() const {
    QVariantList list;
    QHash<QString, int> depth;
    for (const auto &project : m_projects) {
        if (project.inbox)
            continue;
        // Parents come before their sub-projects (Tasks::parseProjects).
        const int level = depth.contains(project.parentId) ? depth.value(project.parentId) + 1 : 0;
        depth.insert(project.id, level);
        list << QVariantMap{{"id", project.id}, {"name", project.name}, {"colorName", project.color},
                            {"color", Tasks::colorHex(project.color)}, {"depth", level}};
    }
    return list;
}

QVariantList Backend::destinations() const {
    QVariantList list;
    for (const auto &project : m_projects)
        if (project.inbox)
            list << QVariantMap{{"id", project.id}, {"name", tr("Inbox")}, {"color", QString()}, {"depth", 0}, {"inbox", true}};
    for (const auto &entry : projects()) {
        QVariantMap map = entry.toMap();
        map.insert("inbox", false);
        list << map;
    }
    return list;
}

QString Backend::inboxId() const {
    for (const auto &project : m_projects)
        if (project.inbox)
            return project.id;
    return {};
}

void Backend::begin() {
    if (m_busy++ == 0)
        emit busyChanged();
}

void Backend::end() {
    if (--m_busy == 0)
        emit busyChanged();
}

void Backend::fail(const QString &message) {
    if (message == m_error)
        return;
    m_error = message;
    emit errorChanged();
}

void Backend::dismissError() { fail({}); }

void Backend::showView(const QString &view) {
    if (view == m_view)
        return;
    m_view = view;
    ++m_generation;
    m_tasks.clear();
    m_depths.clear();
    m_loaded = false;
    emit viewChanged();
    emit viewNameChanged();
    emit tasksChanged();
    loadView();
}

void Backend::refresh() {
    if (!signedIn())
        return;
    begin();
    const int generation = m_generation;
    // Projects first: the Inbox view needs the Inbox's id, and every view
    // shows project names.
    m_client.projects([this, generation](const QJsonArray &results, const QString &error) {
        end();
        if (!error.isEmpty()) {
            fail(error);
            return;
        }
        m_projects = Tasks::parseProjects(results);
        m_projectsById.clear();
        for (const auto &project : m_projects)
            m_projectsById.insert(project.id, project);
        emit projectsChanged();
        emit viewNameChanged();
        // A project deleted elsewhere falls back to Today.
        if (m_view.startsWith("project:") && !m_projectsById.contains(m_view.mid(8))) {
            showView("today");
            loadCounts();
            return;
        }
        // A view picked while the projects were loading couldn't load then.
        if (generation == m_generation || m_viewWaiting)
            loadView();
        loadCounts();
    });
}

void Backend::loadView() {
    if (!signedIn())
        return;
    // Every view needs the projects (the Inbox's id, project names); the
    // refresh that brings them loads this view then.
    m_viewWaiting = m_projects.isEmpty();
    if (m_viewWaiting)
        return;
    const int generation = m_generation;
    const QString view = m_view;
    auto done = [this, generation, view](const QJsonArray &results, const QString &error) {
        end();
        if (generation != m_generation)
            return;
        if (!error.isEmpty()) {
            fail(error);
            return;
        }
        fail({});
        QList<Task> tasks = Tasks::parse(results);
        m_depths.clear();
        if (view == "today" || view == "upcoming")
            Tasks::sortByDue(tasks);
        else
            tasks = Tasks::sortAsTree(tasks, &m_depths);
        applyTasks(tasks);
    };
    begin();
    if (view == "today")
        m_client.tasksMatching(todayQuery(), done);
    else if (view == "upcoming")
        m_client.tasksMatching(upcomingQuery(), done);
    else if (view == "inbox")
        m_client.tasksInProject(inboxId(), done);
    else
        m_client.tasksInProject(view.mid(8), done);
}

void Backend::loadCounts() {
    begin();
    m_client.tasksMatching(todayQuery(), [this](const QJsonArray &results, const QString &error) {
        end();
        if (error.isEmpty() && int(results.size()) != m_todayCount) {
            m_todayCount = int(results.size());
            emit countsChanged();
        }
    });
    const QString inbox = inboxId();
    if (inbox.isEmpty())
        return;
    begin();
    m_client.tasksInProject(inbox, [this](const QJsonArray &results, const QString &error) {
        end();
        if (error.isEmpty() && int(results.size()) != m_inboxCount) {
            m_inboxCount = int(results.size());
            emit countsChanged();
        }
    });
}

void Backend::applyTasks(const QList<Task> &tasks) {
    m_tasks = tasks;
    m_loaded = true;
    emit tasksChanged();
}

void Backend::signIn(const QString &token) {
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        emit signInFailed(tr("Paste your Todoist API token first."));
        return;
    }
    // Check the token against Todoist before keeping it.
    auto probe = new Todoist(this);
    probe->setToken(trimmed);
    begin();
    probe->projects([this, probe, trimmed](const QJsonArray &, const QString &error) {
        end();
        probe->deleteLater();
        if (!error.isEmpty()) {
            emit signInFailed(error);
            return;
        }
        const QString saveError = m_credentials.write(trimmed);
        if (!saveError.isEmpty()) {
            emit signInFailed(saveError);
            return;
        }
        m_client.setToken(trimmed);
        fail({});
        emit signedInChanged();
        refresh();
    });
}

void Backend::signOut() {
    const QString error = m_credentials.clear();
    if (!error.isEmpty()) {
        fail(error);
        return;
    }
    m_client.setToken({});
    ++m_generation;
    m_tasks.clear();
    m_projects.clear();
    m_projectsById.clear();
    m_undo.clear();
    m_redo.clear();
    m_todayCount = m_inboxCount = 0;
    m_loaded = false;
    fail({});
    emit signedInChanged();
    emit tasksChanged();
    emit projectsChanged();
    emit countsChanged();
    emit historyChanged();
}

void Backend::removeLocally(const QString &taskId) {
    // Subtasks go with their parent, as they do in Todoist.
    QSet<QString> gone{taskId};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto &task : m_tasks)
            if (!gone.contains(task.id) && gone.contains(task.parentId)) {
                gone.insert(task.id);
                grew = true;
            }
    }
    const QDateTime now = QDateTime::currentDateTime();
    QList<Task> kept;
    for (const auto &task : m_tasks) {
        if (!gone.contains(task.id)) {
            kept << task;
            continue;
        }
        // A recurring task moves to its next date instead of disappearing,
        // which the next refresh shows.
        if (!task.recurring && task.hasDue() && (task.isOverdue(now) || task.dueDate == now.date()))
            m_todayCount = qMax(0, m_todayCount - 1);
    }
    m_tasks = kept;
    emit tasksChanged();
    emit countsChanged();
}

void Backend::pushUndo(const Step &step) {
    m_undo << step;
    if (m_undo.size() > 100)
        m_undo.removeFirst();
    emit historyChanged();
}

void Backend::complete(const QString &taskId) {
    Task task;
    for (const auto &candidate : m_tasks)
        if (candidate.id == taskId)
            task = candidate;
    if (task.id.isEmpty())
        return;
    // Shown done right away; a failure brings it back with the reason.
    removeLocally(taskId);
    begin();
    m_client.close(taskId, [this, task](const QJsonObject &, const QString &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't complete “%1”. %2").arg(task.content, error));
            loadView();
            loadCounts();
            return;
        }
        m_redo.clear();
        pushUndo({Step::Completed, task.id, task.content, task, {}, {}});
        if (task.recurring) {
            loadView();
            loadCounts();
        }
    });
}

void Backend::add(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    // Todoist's quick add can't be told a project, so the task is moved
    // there after it's made. A #project or a date in the text wins.
    const QString target = m_view.startsWith("project:") ? m_view.mid(8) : QString();
    const bool namesProject = trimmed.contains('#');
    const bool wantsToday = m_view == "today";
    begin();
    m_client.quickAdd(trimmed, [this, trimmed, target, namesProject, wantsToday](const QJsonObject &result,
                                                                                  const QString &error) {
        if (!error.isEmpty()) {
            end();
            fail(tr("Couldn't add “%1”. %2").arg(trimmed, error));
            return;
        }
        const Task task = Task::fromJson(result);
        const bool filing = !target.isEmpty() && !namesProject && task.projectId != target;
        m_redo.clear();
        // Kept with the project it's filed into, so deleting that project
        // drops the step too.
        pushUndo({Step::Added, task.id, trimmed, task, {}, filing ? target : QString()});
        emit added(task.content.isEmpty() ? trimmed : task.content);

        auto finish = [this, trimmed](const QString &followUpError) {
            end();
            if (!followUpError.isEmpty())
                fail(tr("Added “%1”, but couldn't file it here. %2").arg(trimmed, followUpError));
            loadView();
            loadCounts();
        };
        auto dateIt = [this, task, wantsToday, finish](const QString &moveError) {
            if (!moveError.isEmpty() || !wantsToday || task.hasDue()) {
                finish(moveError);
                return;
            }
            m_client.update(task.id, QJsonObject{{"due_string", "today"}},
                            [finish](const QJsonObject &, const QString &e) { finish(e); });
        };
        if (filing)
            m_client.move(task.id, target, [dateIt](const QJsonObject &, const QString &e) { dateIt(e); });
        else
            dateIt({});
    });
}

QVariantList Backend::colors() const {
    QVariantList list;
    for (const auto &name : Tasks::colorNames())
        list << QVariantMap{{"name", name}, {"hex", Tasks::colorHex(name)}};
    return list;
}

void Backend::setProjectColor(const QString &projectId, const QString &color) {
    if (!m_projectsById.contains(projectId) || !Tasks::colorNames().contains(color))
        return;
    const QString previous = m_projectsById.value(projectId).color;
    if (previous == color)
        return;
    // Recolored at once; a failure puts the old color back.
    auto recolor = [this, projectId](const QString &value) {
        for (auto &project : m_projects)
            if (project.id == projectId)
                project.color = value;
        m_projectsById[projectId].color = value;
        emit projectsChanged();
        emit tasksChanged();
    };
    recolor(color);
    begin();
    m_client.updateProject(projectId, QJsonObject{{"color", color}},
                           [this, projectId, previous, recolor](const QJsonObject &, const QString &error) {
                               end();
                               if (!error.isEmpty()) {
                                   recolor(previous);
                                   fail(tr("Couldn't change the color of “%1”. %2")
                                            .arg(m_projectsById.value(projectId).name, error));
                               }
                           });
}

void Backend::createProject(const QString &name, const QString &color) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    begin();
    const QString chosen = Tasks::colorNames().contains(color) ? color : QString();
    m_client.createProject(trimmed, chosen, [this, trimmed](const QJsonObject &result, const QString &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't create “%1”. %2").arg(trimmed, error));
            return;
        }
        const Project project = Project::fromJson(result);
        // Shown at once, before the refresh brings Todoist's full list.
        if (!project.id.isEmpty() && !m_projectsById.contains(project.id)) {
            Project added = project;
            added.order = m_projects.isEmpty() ? 0 : m_projects.last().order + 1;
            m_projects << added;
            m_projectsById.insert(added.id, added);
            emit projectsChanged();
        }
        emit projectCreated(trimmed);
        if (!project.id.isEmpty())
            showView("project:" + project.id);
        refresh();
    });
}

QStringList Backend::descendantsOf(const QString &projectId) const {
    QStringList found;
    QStringList frontier{projectId};
    while (!frontier.isEmpty()) {
        const QString parent = frontier.takeFirst();
        for (const auto &project : m_projects)
            if (project.parentId == parent && !found.contains(project.id)) {
                found << project.id;
                frontier << project.id;
            }
    }
    return found;
}

void Backend::inspectProject(const QString &projectId) {
    const QStringList ids = QStringList{projectId} + descendantsOf(projectId);
    QStringList names;
    for (const auto &id : ids.mid(1))
        names << m_projectsById.value(id).name;
    // One listing per project, summed when all have answered.
    auto total = std::make_shared<int>(0);
    auto pending = std::make_shared<int>(int(ids.size()));
    auto failed = std::make_shared<bool>(false);
    for (const auto &id : ids) {
        begin();
        m_client.tasksInProject(id, [this, projectId, names, total, pending, failed](const QJsonArray &results,
                                                                                       const QString &error) {
            end();
            if (!error.isEmpty())
                *failed = true;
            *total += int(results.size());
            if (--*pending == 0)
                emit projectContents(projectId, *failed ? -1 : *total, names);
        });
    }
}

void Backend::deleteProject(const QString &projectId) {
    const Project project = m_projectsById.value(projectId);
    if (project.id.isEmpty() || project.inbox)
        return;
    const QStringList gone = QStringList{projectId} + descendantsOf(projectId);
    if (m_view.startsWith("project:") && gone.contains(m_view.mid(8)))
        showView("inbox");
    // Steps that touch the deleted projects can't be undone or redone.
    auto dropSteps = [&gone](QList<Step> &steps) {
        steps.erase(std::remove_if(steps.begin(), steps.end(), [&gone](const Step &step) {
                        return gone.contains(step.task.projectId) || gone.contains(step.from) || gone.contains(step.to);
                    }),
                    steps.end());
    };
    dropSteps(m_undo);
    dropSteps(m_redo);
    emit historyChanged();

    begin();
    m_client.deleteProject(projectId, [this, project](const QJsonObject &, const QString &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't delete “%1”. %2").arg(project.name, error));
            return;
        }
        emit projectDeleted(project.name);
        refresh();
    });
}

const Task *Backend::findTask(const QString &taskId) const {
    for (const auto &task : m_tasks)
        if (task.id == taskId)
            return &task;
    return nullptr;
}

// What puts a task's date back the way it was: its own wording when Todoist
// gave one (which keeps "every monday" recurring), else the plain date.
QString Backend::dueStringOf(const Task &task) {
    if (!task.hasDue())
        return QStringLiteral("no date");
    if (!task.dueString.isEmpty())
        return task.dueString;
    QString when = task.dueDate.toString(Qt::ISODate);
    if (task.hasTime())
        when += " " + task.dueTime.toString("HH:mm");
    return when;
}

void Backend::reloadSoon() {
    loadView();
    loadCounts();
}

void Backend::applyMove(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    const QString project = forward ? step.to : step.from;
    // Leaves a project's list right away; the dated lists just relabel it.
    if (m_view.startsWith("project:") || m_view == "inbox") {
        const QString shown = m_view == "inbox" ? inboxId() : m_view.mid(8);
        if (project != shown)
            removeLocally(step.taskId);
    }
    begin();
    m_client.move(step.taskId, project, [this, done](const QJsonObject &, const ApiError &error) {
        end();
        done(error);
        reloadSoon();
    });
}

void Backend::applySchedule(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    begin();
    m_client.update(step.taskId, QJsonObject{{"due_string", forward ? step.to : step.from}},
                    [this, done](const QJsonObject &, const ApiError &error) {
                        end();
                        done(error);
                        reloadSoon();
                    });
}

void Backend::moveTask(const QString &taskId, const QString &projectId) {
    const Task *task = findTask(taskId);
    if (!task || projectId.isEmpty() || task->projectId == projectId)
        return;
    const Step step{Step::Moved, task->id, task->content, *task, task->projectId, projectId};
    applyMove(step, true, [this, step](const QString &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::schedule(const QString &taskId, const QString &dueString) {
    const Task *task = findTask(taskId);
    const QString wanted = dueString.trimmed();
    if (!task || wanted.isEmpty())
        return;
    const Step step{Step::Scheduled, task->id, task->content, *task, dueStringOf(*task), wanted};
    applySchedule(step, true, [this, step](const QString &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't change the date of “%1”. %2").arg(step.text, error));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

// A failure that may pass (offline, Todoist busy) keeps the step to try
// again; one that won't (the task was deleted elsewhere) drops it, so it
// doesn't block every step under it.
void Backend::undoFailed(const Step &step, const ApiError &error) {
    if (error.isTransient()) {
        fail(tr("Couldn't undo. %1").arg(error.message));
        m_undo << step;
    } else {
        fail(tr("Couldn't undo “%1”, so it's off the undo list. %2").arg(step.text, error.message));
    }
    emit historyChanged();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const Step step = m_undo.takeLast();
    emit historyChanged();
    if (step.kind == Step::Moved || step.kind == Step::Scheduled) {
        auto done = [this, step](const ApiError &error) {
            if (!error.isEmpty()) {
                undoFailed(step, error);
                return;
            }
            m_redo << step;
            emit historyChanged();
        };
        if (step.kind == Step::Moved)
            applyMove(step, false, done);
        else
            applySchedule(step, false, done);
        return;
    }
    begin();
    auto done = [this, step](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            undoFailed(step, error);
            return;
        }
        m_redo << step;
        emit historyChanged();
        loadView();
        loadCounts();
    };
    if (step.kind == Step::Completed)
        m_client.reopen(step.taskId, done);
    else
        m_client.remove(step.taskId, done);
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    const Step step = m_redo.takeLast();
    emit historyChanged();
    if (step.kind == Step::Moved || step.kind == Step::Scheduled) {
        auto done = [this, step](const QString &error) {
            if (!error.isEmpty()) {
                fail(tr("Couldn't redo. %1").arg(error));
                return;
            }
            pushUndo(step);
        };
        if (step.kind == Step::Moved)
            applyMove(step, true, done);
        else
            applySchedule(step, true, done);
        return;
    }
    if (step.kind == Step::Completed) {
        begin();
        m_client.close(step.taskId, [this, step](const QJsonObject &, const QString &error) {
            end();
            if (!error.isEmpty()) {
                fail(tr("Couldn't redo. %1").arg(error));
                return;
            }
            pushUndo(step);
            loadView();
            loadCounts();
        });
    } else {
        // A deleted task can't come back with its old id, so redoing an add
        // adds the same text again.
        begin();
        m_client.quickAdd(step.text, [this, step](const QJsonObject &result, const QString &error) {
            end();
            if (!error.isEmpty()) {
                fail(tr("Couldn't redo. %1").arg(error));
                return;
            }
            pushUndo({Step::Added, Task::fromJson(result).id, step.text, Task::fromJson(result), {}, {}});
            loadView();
            loadCounts();
        });
    }
}
