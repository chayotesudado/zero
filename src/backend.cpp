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

QVariantList Backend::rows() const {
    const QDateTime now = QDateTime::currentDateTime();
    auto taskRow = [&](const Task &task) {
        QVariantMap row = Tasks::toVariant(task, now, m_projectsById, m_depths.value(task.id));
        row.insert("kind", "task");
        row.insert("key", "task:" + task.id);
        return QVariant(row);
    };
    QVariantList list;
    const QString project = viewProjectId();
    if (project.isEmpty()) {
        for (const auto &task : m_tasks)
            list << taskRow(task);
        return list;
    }
    if (!m_loaded)
        return list;
    // m_tasks is already in this order (Tasks::sortBySection); the headings
    // go in between, empty sections included so they can be seen and used.
    const QList<Section> sections = sectionsOf(project);
    QSet<QString> known;
    for (const auto &section : sections)
        known.insert(section.id);
    for (const auto &task : m_tasks)
        if (!known.contains(task.sectionId))
            list << taskRow(task);
    for (const auto &section : sections) {
        int count = 0;
        for (const auto &task : m_tasks)
            count += task.sectionId == section.id ? 1 : 0;
        list << QVariantMap{{"kind", "section"}, {"key", "section:" + section.id}, {"id", section.id},
                            {"name", section.name}, {"projectId", section.projectId}, {"count", count}};
        for (const auto &task : m_tasks)
            if (task.sectionId == section.id)
                list << taskRow(task);
    }
    return list;
}

QString Backend::viewProjectId() const {
    if (m_view == "inbox")
        return inboxId();
    if (m_view.startsWith("project:"))
        return m_view.mid(8);
    return {};
}

QList<Section> Backend::sectionsOf(const QString &projectId) const {
    QList<Section> list;
    for (const auto &section : m_sections)
        if (section.projectId == projectId)
            list << section;
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
                            {"color", Tasks::colorHex(project.color)}, {"depth", level},
                            {"parentId", depth.contains(project.parentId) ? project.parentId : QString()}};
    }
    return list;
}

QVariantList Backend::destinations() const {
    QVariantList list;
    auto add = [&](const QString &id, const QString &name, const QString &color, int depth, bool inbox) {
        list << QVariantMap{{"key", "project:" + id}, {"kind", "project"}, {"projectId", id}, {"sectionId", QString()},
                            {"name", name}, {"path", name}, {"color", color}, {"depth", depth}, {"inbox", inbox}};
        for (const auto &section : sectionsOf(id))
            list << QVariantMap{{"key", "section:" + section.id}, {"kind", "section"}, {"projectId", id},
                                {"sectionId", section.id}, {"name", section.name},
                                {"path", name + " / " + section.name}, {"color", color}, {"depth", depth + 1},
                                {"inbox", inbox}};
    };
    for (const auto &project : m_projects)
        if (project.inbox)
            add(project.id, tr("Inbox"), QString(), 0, true);
    for (const auto &entry : projects()) {
        const QVariantMap map = entry.toMap();
        add(map.value("id").toString(), map.value("name").toString(), map.value("color").toString(),
            map.value("depth").toInt(), false);
    }
    return list;
}

QVariantList Backend::parentChoices(const QString &projectId) const {
    const QStringList excluded = QStringList{projectId} + descendantsOf(projectId);
    QVariantList list{QVariantMap{{"key", "top"}, {"kind", "top"}, {"projectId", QString()}, {"name", tr("No parent (top level)")},
                                  {"path", tr("No parent (top level)")}, {"color", QString()}, {"depth", 0}, {"inbox", false}}};
    for (const auto &entry : projects()) {
        QVariantMap map = entry.toMap();
        if (excluded.contains(map.value("id").toString()))
            continue;
        map.insert("key", "project:" + map.value("id").toString());
        map.insert("kind", "project");
        map.insert("projectId", map.value("id"));
        map.insert("path", map.value("name"));
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

// A failed load says so until a load succeeds. That success clears only
// errors from loading: a failed move or add stays up even though the list
// reloads after it.
void Backend::loadFailed(const QString &message) {
    m_loadError = message;
    fail(message);
}

void Backend::loadSucceeded() {
    if (!m_loadError.isEmpty() && m_error == m_loadError)
        fail({});
    m_loadError.clear();
}

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
    const int generation = m_generation;
    // Projects and sections first: the Inbox view needs the Inbox's id,
    // every view shows project names, and project views their sections.
    struct Answers {
        QJsonArray projects, sections;
        ApiError projectsError, sectionsError;
        int pending = 2;
    };
    auto answers = std::make_shared<Answers>();
    auto settled = [this, generation, answers] {
        if (!answers->projectsError.isEmpty()) {
            loadFailed(answers->projectsError.message);
            return;
        }
        m_projects = Tasks::parseProjects(answers->projects);
        // Without sections, lists still work; the ones already known stay.
        if (answers->sectionsError.isEmpty())
            m_sections = Tasks::parseSections(answers->sections);
        else
            loadFailed(answers->sectionsError.message);
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
    };
    auto answered = [this, answers, settled](QJsonArray *into, ApiError *failed, const QJsonArray &results,
                                             const ApiError &error) {
        end();
        *into = results;
        *failed = error;
        if (--answers->pending == 0)
            settled();
    };
    begin();
    begin();
    m_client.projects([answers, answered](const QJsonArray &results, const ApiError &error) {
        answered(&answers->projects, &answers->projectsError, results, error);
    });
    m_client.sections({}, [answers, answered](const QJsonArray &results, const ApiError &error) {
        answered(&answers->sections, &answers->sectionsError, results, error);
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
    auto done = [this, generation, view](const QJsonArray &results, const ApiError &error) {
        end();
        if (generation != m_generation)
            return;
        if (!error.isEmpty()) {
            loadFailed(error.message);
            return;
        }
        loadSucceeded();
        QList<Task> tasks = Tasks::parse(results);
        m_depths.clear();
        if (view == "today" || view == "upcoming")
            Tasks::sortByDue(tasks);
        else
            tasks = Tasks::sortBySection(tasks, sectionsOf(viewProjectId()), &m_depths);
        applyTasks(tasks);
    };
    // Every task whose subtasks are loaded, so a subtask opened from the
    // details (whose copy is its parent's) shows a new date or place too.
    QStringList parents = m_subtasks.keys();
    if (!m_watching.isEmpty() && !parents.contains(m_watching))
        parents << m_watching;
    for (const auto &parent : parents)
        fetchSubtasks(parent);
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
    m_client.tasksMatching(todayQuery(), [this](const QJsonArray &results, const ApiError &error) {
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
    m_client.tasksInProject(inbox, [this](const QJsonArray &results, const ApiError &error) {
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
    probe->projects([this, probe, trimmed](const QJsonArray &, const ApiError &error) {
        end();
        probe->deleteLater();
        if (!error.isEmpty()) {
            emit signInFailed(error.message);
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
    m_sections.clear();
    m_undo.clear();
    m_redo.clear();
    m_subtasks.clear();
    m_watching.clear();
    ++m_subtaskGeneration;
    m_todayCount = m_inboxCount = 0;
    m_loaded = false;
    fail({});
    emit signedInChanged();
    emit tasksChanged();
    emit projectsChanged();
    emit countsChanged();
    emit historyChanged();
}

QSet<QString> Backend::removeLocally(const QString &taskId) {
    // Subtasks go with their parent, as they do in Todoist.
    QList<Task> known = m_tasks;
    for (const auto &list : std::as_const(m_subtasks))
        known += list;
    QSet<QString> gone{taskId};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto &task : known)
            if (!gone.contains(task.id) && gone.contains(task.parentId)) {
                gone.insert(task.id);
                grew = true;
            }
    }
    bool hadSubtasks = false;
    for (auto it = m_subtasks.begin(); it != m_subtasks.end(); ++it)
        hadSubtasks |= it.value().removeIf([&gone](const Task &task) { return gone.contains(task.id); }) > 0;
    if (hadSubtasks)
        emit subtasksChanged();
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
    return gone;
}

bool Backend::mentions(const Step &step, const QSet<QString> &ids) {
    if (ids.contains(step.taskId) || ids.contains(step.toParent) || ids.contains(step.fromParent))
        return true;
    // The tasks, sections, or projects a reorder (or an indent's reorder)
    // puts in order, which can't be ordered once one is gone.
    for (const QJsonObject &args : {step.before, step.after})
        for (const char *list : {"items", "sections", "projects"})
            for (const auto &value : args.value(list).toArray())
                if (ids.contains(value.toObject().value("id").toString()))
                    return true;
    return false;
}

void Backend::dropSteps(const std::function<bool(const Step &)> &gone) {
    auto drop = [&gone](QList<Step> &steps) {
        steps.erase(std::remove_if(steps.begin(), steps.end(), gone), steps.end());
    };
    drop(m_undo);
    drop(m_redo);
    emit historyChanged();
}

void Backend::deleteTask(const QString &taskId) {
    const Task *found = findTask(taskId);
    if (!found)
        return;
    const Task task = *found;
    const QSet<QString> gone = removeLocally(taskId);
    // Nothing done to these can be undone or redone any more.
    dropSteps([gone](const Step &step) { return step.kind != Step::ProjectMoved && mentions(step, gone); });
    begin();
    m_client.remove(taskId, [this, task](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't delete “%1”. %2").arg(task.content, error.message));
            loadView();
        } else {
            emit taskDeleted(task.content);
        }
        loadCounts();
    });
}

void Backend::pushUndo(const Step &step) {
    m_undo << step;
    if (m_undo.size() > 100)
        m_undo.removeFirst();
    emit historyChanged();
}

void Backend::complete(const QString &taskId) {
    const Task *found = findTask(taskId);
    if (!found)
        return;
    const Task task = *found;
    // Shown done right away; a failure brings it back with the reason.
    removeLocally(taskId);
    begin();
    m_client.close(taskId, [this, task](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't complete “%1”. %2").arg(task.content, error.message));
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

void Backend::add(const QString &text, const QString &sectionId, const QString &parentId) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    // Todoist's quick add can't be told a project or section, so the task is
    // moved there after it's made. A #project or a date in the text wins.
    const QString target = m_view.startsWith("project:") ? m_view.mid(8) : QString();
    const QString onScreen = viewProjectId();
    const QString inbox = inboxId();
    QString section;
    for (const auto &candidate : sectionsOf(onScreen))
        if (candidate.id == sectionId)
            section = candidate.id;
    const bool wantsToday = m_view == "today";
    const QString parent = !parentId.isEmpty() && findTask(parentId) ? parentId : QString();
    begin();
    m_client.quickAdd(trimmed, [this, trimmed, target, onScreen, inbox, section, wantsToday,
                                parent](const QJsonObject &result, const ApiError &error) {
        if (!error.isEmpty()) {
            end();
            fail(tr("Couldn't add “%1”. %2").arg(trimmed, error.message));
            return;
        }
        const Task task = Task::fromJson(result);
        // Quick add only leaves the Inbox when the text named a real project,
        // which then wins. A "#" that matched nothing ("PR #42") doesn't count.
        const bool namedProject = task.projectId != inbox && task.projectId != onScreen;
        Step step{Step::Added, task.id, trimmed, task, {}, namedProject ? QString() : target, {},
                  namedProject ? QString() : section};
        step.datedToday = wantsToday && !task.hasDue();
        // A subtask goes under its parent, wherever that is; that wins
        // over a #project too.
        if (!parent.isEmpty()) {
            step.to.clear();
            step.toSection.clear();
            step.toParent = parent;
            // Nor dated today for being added while Today is on screen.
            step.datedToday = false;
        }
        m_redo.clear();
        // Kept with the project and section it's filed into, so deleting
        // either drops the step too.
        pushUndo(step);
        emit added(task.content.isEmpty() ? trimmed : task.content);
        fileAdded(task, step, [this, trimmed](const ApiError &followUp) {
            end();
            if (!followUp.isEmpty())
                fail(tr("Added “%1”, but couldn't file it here. %2").arg(trimmed, followUp.message));
            reloadSoon();
        });
    });
}

void Backend::fileAdded(const Task &task, const Step &step, std::function<void(const ApiError &)> done) {
    auto dateIt = [this, task, step, done](const ApiError &moveError) {
        if (!moveError.isEmpty() || !step.datedToday || task.hasDue()) {
            done(moveError);
            return;
        }
        m_client.update(task.id, QJsonObject{{"due_string", "today"}},
                        [done](const QJsonObject &, const ApiError &e) { done(e); });
    };
    if (!step.toParent.isEmpty())
        m_client.moveUnder(task.id, step.toParent, [dateIt](const QJsonObject &, const ApiError &e) { dateIt(e); });
    else if (!step.toSection.isEmpty())
        m_client.moveToSection(task.id, step.toSection, [dateIt](const QJsonObject &, const ApiError &e) { dateIt(e); });
    else if (!step.to.isEmpty() && task.projectId != step.to)
        m_client.move(task.id, step.to, [dateIt](const QJsonObject &, const ApiError &e) { dateIt(e); });
    else
        dateIt({});
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
                           [this, projectId, previous, recolor](const QJsonObject &, const ApiError &error) {
                               end();
                               if (!error.isEmpty()) {
                                   recolor(previous);
                                   fail(tr("Couldn't change the color of “%1”. %2")
                                            .arg(m_projectsById.value(projectId).name, error.message));
                               }
                           });
}

void Backend::createProject(const QString &name, const QString &color) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    begin();
    const QString chosen = Tasks::colorNames().contains(color) ? color : QString();
    m_client.createProject(trimmed, chosen, [this, trimmed](const QJsonObject &result, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't create “%1”. %2").arg(trimmed, error.message));
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
                                                                                       const ApiError &error) {
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
    const QSet<QString> goneSet(gone.begin(), gone.end());
    dropSteps([gone, goneSet](const Step &step) {
        return gone.contains(step.task.projectId) || gone.contains(step.from) || gone.contains(step.to) ||
               (!aboutTask(step) && mentions(step, goneSet));
    });

    begin();
    m_client.deleteProject(projectId, [this, project](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't delete “%1”. %2").arg(project.name, error.message));
            return;
        }
        emit projectDeleted(project.name);
        refresh();
    });
}

void Backend::moveProject(const QString &projectId, const QString &parentId) {
    const Project project = m_projectsById.value(projectId);
    if (project.id.isEmpty() || project.inbox || project.parentId == parentId)
        return;
    // Not under the Inbox, itself, or one of its own sub-projects.
    if (!parentId.isEmpty() &&
        (!m_projectsById.contains(parentId) || m_projectsById.value(parentId).inbox ||
         parentId == projectId || descendantsOf(projectId).contains(parentId)))
        return;
    const Step step{Step::ProjectMoved, projectId, project.name, {}, project.parentId, parentId};
    applyProjectMove(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
        emit projectMoved(step.text);
    });
}

void Backend::applyProjectMove(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    begin();
    m_client.moveProject(step.taskId, forward ? step.to : step.from,
                         [this, done](const QJsonObject &, const ApiError &error) {
                             end();
                             done(error);
                             // The sidebar's order and nesting come from Todoist.
                             refresh();
                         });
}

void Backend::createSection(const QString &name) {
    const QString trimmed = name.trimmed();
    const QString project = viewProjectId();
    if (trimmed.isEmpty() || project.isEmpty())
        return;
    begin();
    m_client.createSection(trimmed, project, [this, trimmed](const QJsonObject &result, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't create “%1”. %2").arg(trimmed, error.message));
            return;
        }
        // Shown at once, last in its project, before the refresh.
        Section section = Section::fromJson(result);
        if (!section.id.isEmpty()) {
            int last = -1;
            for (const auto &other : sectionsOf(section.projectId))
                last = qMax(last, other.order);
            section.order = qMax(section.order, last + 1);
            m_sections << section;
            emit tasksChanged();
            emit projectsChanged();
        }
        emit sectionCreated(trimmed);
        refresh();
    });
}

void Backend::deleteSection(const QString &sectionId) {
    Section section;
    for (const auto &candidate : m_sections)
        if (candidate.id == sectionId)
            section = candidate;
    if (section.id.isEmpty())
        return;
    // Gone at once, tasks and all; a failure brings them back.
    QStringList inSection;
    for (const auto &task : m_tasks)
        if (task.sectionId == sectionId)
            inSection << task.id;
    QSet<QString> tasks;
    for (const auto &id : inSection)
        tasks.unite(removeLocally(id));
    m_sections.removeIf([&](const Section &candidate) { return candidate.id == sectionId; });
    dropSteps([tasks, sectionId](const Step &step) {
        return (aboutTask(step) && mentions(step, tasks)) || step.fromSection == sectionId ||
               step.toSection == sectionId || (!aboutTask(step) && mentions(step, {sectionId}));
    });
    emit tasksChanged();
    emit projectsChanged();
    begin();
    m_client.deleteSection(sectionId, [this, section](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty()) {
            fail(tr("Couldn't delete “%1”. %2").arg(section.name, error.message));
            refresh();
            return;
        }
        emit sectionDeleted(section.name);
        loadCounts();
    });
}

QSet<QString> Backend::subtreeOf(const QString &taskId) const {
    QSet<QString> found{taskId};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto &task : m_tasks)
            if (!found.contains(task.id) && found.contains(task.parentId)) {
                found.insert(task.id);
                grew = true;
            }
    }
    return found;
}

int Backend::subtaskCount(const QString &taskId) const {
    // The list's, and the ones loaded for the details, which a dated list
    // doesn't show.
    QList<Task> known = m_tasks;
    for (const auto &list : m_subtasks)
        known += list;
    QSet<QString> found{taskId};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto &task : known)
            if (!found.contains(task.id) && found.contains(task.parentId)) {
                found.insert(task.id);
                grew = true;
            }
    }
    return int(found.size()) - 1;
}

const Task *Backend::findTask(const QString &taskId) const {
    for (const auto &task : m_tasks)
        if (task.id == taskId)
            return &task;
    for (const auto &list : m_subtasks)
        for (const auto &task : list)
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
    const QString section = forward ? step.toSection : step.fromSection;
    const QString parent = forward ? step.toParent : step.fromParent;
    const QJsonObject orders = forward ? step.after : step.before;
    // Leaves a project's list right away, and goes under its new parent at
    // once; the dated lists just relabel it, and a move between sections
    // shows on the reload.
    const QString shown = viewProjectId();
    if (!shown.isEmpty() && project != shown) {
        removeLocally(step.taskId);
    } else if (!shown.isEmpty() && (step.fromParent != step.toParent || !orders.isEmpty())) {
        const Task *under = findTask(parent);
        const QString inSection = under ? under->sectionId : section;
        // All in place first, then the list is rebuilt once.
        patchTask(step.taskId, [parent, inSection](Task &task) {
            task.parentId = parent;
            task.sectionId = inSection;
        }, false);
        // Its subtasks go with it into the section.
        const QSet<QString> subtree = subtreeOf(step.taskId);
        for (auto &task : m_tasks)
            if (subtree.contains(task.id))
                task.sectionId = inSection;
        setChildOrders(orders);
        resort();
        emit subtasksChanged();
    }
    auto finish = [this, done](const ApiError &error) {
        end();
        done(error);
        reloadSoon();
    };
    // An indent or outdent then puts its new level in order.
    auto moved = [this, finish, orders](const QJsonObject &, const ApiError &error) {
        if (!error.isEmpty() || orders.isEmpty()) {
            finish(error);
            return;
        }
        m_client.command("item_reorder", orders, [finish](const QJsonObject &, const ApiError &e) { finish(e); });
    };
    begin();
    if (!parent.isEmpty())
        m_client.moveUnder(step.taskId, parent, moved);
    else if (!section.isEmpty())
        m_client.moveToSection(step.taskId, section, moved);
    else
        m_client.move(step.taskId, project, moved);
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

void Backend::moveTask(const QString &taskId, const QString &projectId, const QString &sectionId) {
    const Task *task = findTask(taskId);
    // A subtask can move to the top of its own project or section.
    if (!task || projectId.isEmpty() ||
        (task->projectId == projectId && task->sectionId == sectionId && task->parentId.isEmpty()))
        return;
    Step step{Step::Moved, task->id, task->content, *task, task->projectId, projectId, task->sectionId, sectionId};
    step.fromParent = task->parentId;
    applyMove(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
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
    applySchedule(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't change the date of “%1”. %2").arg(step.text, error.message));
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

QVariantMap Backend::undoDeletes() const {
    if (m_undo.isEmpty() || m_undo.last().kind != Step::Added)
        return {};
    const Step &step = m_undo.last();
    return {{"id", step.taskId}, {"text", step.task.content.isEmpty() ? step.text : step.task.content}};
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const Step step = m_undo.takeLast();
    emit historyChanged();
    if (step.kind != Step::Completed && step.kind != Step::Added) {
        applyStep(step, false, [this, step](const ApiError &error) {
            if (!error.isEmpty()) {
                undoFailed(step, error);
                return;
            }
            m_redo << step;
            emit historyChanged();
        });
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
    if (step.kind != Step::Completed && step.kind != Step::Added) {
        applyStep(step, true, [this, step](const ApiError &error) {
            if (!error.isEmpty()) {
                fail(tr("Couldn't redo. %1").arg(error.message));
                return;
            }
            pushUndo(step);
        });
        return;
    }
    if (step.kind == Step::Completed) {
        begin();
        m_client.close(step.taskId, [this, step](const QJsonObject &, const ApiError &error) {
            end();
            if (!error.isEmpty()) {
                fail(tr("Couldn't redo. %1").arg(error.message));
                return;
            }
            pushUndo(step);
            loadView();
            loadCounts();
        });
    } else {
        // A deleted task can't come back with its old id, so redoing an add
        // adds the same text again, and files and dates it as the first time.
        begin();
        m_client.quickAdd(step.text, [this, step](const QJsonObject &result, const ApiError &error) {
            if (!error.isEmpty()) {
                end();
                fail(tr("Couldn't redo. %1").arg(error.message));
                return;
            }
            Step again = step;
            again.task = Task::fromJson(result);
            again.taskId = again.task.id;
            pushUndo(again);
            fileAdded(again.task, again, [this, step](const ApiError &followUp) {
                end();
                if (!followUp.isEmpty())
                    fail(tr("Added “%1”, but couldn't file it here. %2").arg(step.text, followUp.message));
                reloadSoon();
            });
        });
    }
}

bool Backend::aboutTask(const Step &step) {
    switch (step.kind) {
    case Step::ProjectMoved:
    case Step::ProjectEdited:
    case Step::SectionEdited:
        return false;
    case Step::Reordered:
        return step.command == "item_reorder";
    default:
        return true;
    }
}

void Backend::applyStep(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    switch (step.kind) {
    case Step::Moved:
        applyMove(step, forward, std::move(done));
        break;
    case Step::ProjectMoved:
        applyProjectMove(step, forward, std::move(done));
        break;
    case Step::Scheduled:
        applySchedule(step, forward, std::move(done));
        break;
    case Step::Edited:
    case Step::ProjectEdited:
    case Step::SectionEdited:
        applyEdit(step, forward, std::move(done));
        break;
    case Step::Reordered:
        applyReorder(step, forward, std::move(done));
        break;
    default:
        break;
    }
}

void Backend::patchTask(const QString &taskId, const std::function<void(Task &)> &patch, bool announce) {
    for (auto &task : m_tasks)
        if (task.id == taskId)
            patch(task);
    bool inSubtasks = false;
    for (auto &list : m_subtasks)
        for (auto &task : list)
            if (task.id == taskId) {
                patch(task);
                inSubtasks = true;
            }
    if (!announce)
        return;
    emit tasksChanged();
    if (inSubtasks)
        emit subtasksChanged();
}

// ------------------------------------------------------------ editing

void Backend::editTask(const QString &taskId, const QVariantMap &changes) {
    const Task *task = findTask(taskId);
    if (!task)
        return;
    QJsonObject before, after;
    if (changes.contains("content")) {
        const QString content = changes.value("content").toString().trimmed();
        if (!content.isEmpty() && content != task->content.trimmed()) {
            before.insert("content", task->content);
            after.insert("content", content);
        }
    }
    if (changes.contains("description")) {
        // Trailing blank lines don't count as a change, the ones a task came
        // with from Todoist included: opening and closing it sends nothing.
        auto trimEnd = [](QString text) {
            while (text.endsWith('\n') || text.endsWith(' '))
                text.chop(1);
            return text;
        };
        const QString description = trimEnd(changes.value("description").toString());
        if (description != trimEnd(task->description)) {
            before.insert("description", task->description);
            after.insert("description", description);
        }
    }
    if (changes.contains("priority")) {
        const int priority = changes.value("priority").toInt();
        // Users' p1 is Todoist's 4.
        if (priority >= 1 && priority <= 4 && 5 - priority != task->priority) {
            before.insert("priority", task->priority);
            after.insert("priority", 5 - priority);
        }
    }
    if (after.isEmpty())
        return;
    Step step{Step::Edited, task->id, task->content, *task};
    step.before = before;
    step.after = after;
    applyEdit(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't change “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::setPriority(const QString &taskId, int priority) {
    editTask(taskId, {{"priority", priority}});
}

void Backend::renameProject(const QString &projectId, const QString &name) {
    const Project project = m_projectsById.value(projectId);
    const QString trimmed = name.trimmed();
    if (project.id.isEmpty() || project.inbox || trimmed.isEmpty() || trimmed == project.name)
        return;
    Step step{Step::ProjectEdited, projectId, project.name};
    step.before = {{"name", project.name}};
    step.after = {{"name", trimmed}};
    applyEdit(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't rename “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::renameSection(const QString &sectionId, const QString &name) {
    Section section;
    for (const auto &candidate : m_sections)
        if (candidate.id == sectionId)
            section = candidate;
    const QString trimmed = name.trimmed();
    if (section.id.isEmpty() || trimmed.isEmpty() || trimmed == section.name)
        return;
    Step step{Step::SectionEdited, sectionId, section.name, {}, section.projectId};
    step.before = {{"name", section.name}};
    step.after = {{"name", trimmed}};
    applyEdit(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't rename “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

// Shown changed at once; a failure puts the old fields back.
void Backend::applyEdit(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    const QJsonObject fields = forward ? step.after : step.before;
    const QJsonObject revert = forward ? step.before : step.after;
    auto put = [this, step](const QJsonObject &values) {
        if (step.kind == Step::Edited) {
            patchTask(step.taskId, [&values](Task &task) {
                if (values.contains("content"))
                    task.content = values.value("content").toString();
                if (values.contains("description"))
                    task.description = values.value("description").toString();
                if (values.contains("priority"))
                    task.priority = std::clamp(values.value("priority").toInt(), 1, 4);
            });
        } else if (step.kind == Step::ProjectEdited) {
            const QString name = values.value("name").toString();
            for (auto &project : m_projects)
                if (project.id == step.taskId)
                    project.name = name;
            if (m_projectsById.contains(step.taskId))
                m_projectsById[step.taskId].name = name;
            emit projectsChanged();
            emit viewNameChanged();
            emit tasksChanged();
        } else {
            for (auto &section : m_sections)
                if (section.id == step.taskId)
                    section.name = values.value("name").toString();
            emit projectsChanged();
            emit tasksChanged();
        }
    };
    put(fields);
    auto answered = [this, done, put, revert](const QJsonObject &, const ApiError &error) {
        end();
        if (!error.isEmpty())
            put(revert);
        done(error);
    };
    begin();
    if (step.kind == Step::Edited)
        m_client.update(step.taskId, fields, answered);
    else if (step.kind == Step::ProjectEdited)
        m_client.updateProject(step.taskId, fields, answered);
    else
        m_client.updateSection(step.taskId, fields, answered);
}

// ------------------------------------------------------------ nesting and order

QList<Task> Backend::siblingsOf(const Task &task) const {
    QSet<QString> ids;
    for (const auto &candidate : m_tasks)
        ids.insert(candidate.id);
    QSet<QString> sections;
    for (const auto &section : sectionsOf(viewProjectId()))
        sections.insert(section.id);
    // As the list shows them: a parent that isn't listed, or a section that
    // isn't, counts as none.
    auto parentOf = [&ids](const Task &t) { return ids.contains(t.parentId) ? t.parentId : QString(); };
    auto sectionOf = [&sections](const Task &t) { return sections.contains(t.sectionId) ? t.sectionId : QString(); };
    QList<Task> list;
    for (const auto &candidate : m_tasks)
        if (parentOf(candidate) == parentOf(task) && sectionOf(candidate) == sectionOf(task))
            list << candidate;
    return list;
}

// A reorder command's args: {list: [{id, field: order}]}, with each one's
// own order, or renumbered 1, 2, 3 in the order given.
template <typename T>
static QJsonObject orderArgs(const QList<T> &things, int T::*order, const char *list, const char *field, bool renumber) {
    QJsonArray items;
    for (int i = 0; i < things.size(); ++i)
        items << QJsonObject{{"id", things[i].id}, {field, renumber ? i + 1 : things[i].*order}};
    return {{list, items}};
}

QJsonObject Backend::itemOrders(const QList<Task> &tasks, bool renumber) {
    return orderArgs(tasks, &Task::childOrder, "items", "child_order", renumber);
}

void Backend::resort() {
    const QString project = viewProjectId();
    if (project.isEmpty())
        return;
    m_depths.clear();
    m_tasks = Tasks::sortBySection(m_tasks, sectionsOf(project), &m_depths);
    emit tasksChanged();
}

bool Backend::setChildOrders(const QJsonObject &args) {
    QHash<QString, int> orders;
    for (const auto &value : args.value("items").toArray())
        orders.insert(value.toObject().value("id").toString(), value.toObject().value("child_order").toInt());
    if (orders.isEmpty())
        return false;
    for (auto &task : m_tasks)
        task.childOrder = orders.value(task.id, task.childOrder);
    for (auto &list : m_subtasks)
        for (auto &task : list)
            task.childOrder = orders.value(task.id, task.childOrder);
    return true;
}

void Backend::applyOrders(const QString &command, const QJsonObject &args) {
    if (command == "item_reorder") {
        if (!setChildOrders(args))
            return;
        resort();
        emit subtasksChanged();
    } else if (command == "section_reorder") {
        for (const auto &value : args.value("sections").toArray()) {
            const QJsonObject item = value.toObject();
            for (auto &section : m_sections)
                if (section.id == item.value("id").toString())
                    section.order = item.value("section_order").toInt();
        }
        Tasks::orderSections(m_sections);
        resort();
        emit projectsChanged();
    } else if (command == "project_reorder") {
        for (const auto &value : args.value("projects").toArray()) {
            const QJsonObject item = value.toObject();
            for (auto &project : m_projects)
                if (project.id == item.value("id").toString())
                    project.order = item.value("child_order").toInt();
        }
        m_projects = Tasks::orderProjects(m_projects);
        m_projectsById.clear();
        for (const auto &project : m_projects)
            m_projectsById.insert(project.id, project);
        emit projectsChanged();
    }
}

void Backend::indentTask(const QString &taskId) {
    const Task *found = findTask(taskId);
    if (!found || viewProjectId().isEmpty())
        return;
    const Task task = *found;
    const QList<Task> siblings = siblingsOf(task);
    int at = -1;
    for (int i = 0; i < siblings.size(); ++i)
        if (siblings[i].id == taskId)
            at = i;
    // Nothing above it at its level to go under.
    if (at <= 0)
        return;
    const Task &parent = siblings[at - 1];
    // Last under its new parent, which keeps it where it is on screen.
    int last = 0;
    for (const auto &candidate : m_tasks)
        if (candidate.parentId == parent.id)
            last = qMax(last, candidate.childOrder);
    Task placed = task;
    placed.childOrder = last + 1;
    Step step{Step::Moved, task.id, task.content, task, task.projectId, task.projectId, task.sectionId, parent.sectionId};
    step.fromParent = task.parentId;
    step.toParent = parent.id;
    step.after = itemOrders({placed}, false);
    // Undoing puts it back among its old siblings where it was.
    step.before = itemOrders(siblings, false);
    applyMove(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't indent “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::outdentTask(const QString &taskId) {
    const Task *found = findTask(taskId);
    if (!found || viewProjectId().isEmpty())
        return;
    const Task task = *found;
    const Task *parentFound = nullptr;
    for (const auto &candidate : m_tasks)
        if (candidate.id == task.parentId)
            parentFound = &candidate;
    // Already at the top of the list.
    if (!parentFound)
        return;
    const Task parent = *parentFound;
    // Just after its old parent, among the parent's siblings.
    QList<Task> level;
    for (const auto &sibling : siblingsOf(parent)) {
        level << sibling;
        if (sibling.id == parent.id)
            level << task;
    }
    Step step{Step::Moved, task.id, task.content, task, task.projectId, task.projectId, task.sectionId, parent.sectionId};
    step.fromParent = task.parentId;
    step.toParent = findTask(parent.parentId) ? parent.parentId : QString();
    step.after = itemOrders(level, true);
    step.before = itemOrders(siblingsOf(task), false);
    applyMove(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't outdent “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::moveTaskBy(const QString &taskId, int delta) {
    const Task *found = findTask(taskId);
    if (!found || viewProjectId().isEmpty() || delta == 0)
        return;
    const QList<Task> siblings = siblingsOf(*found);
    int at = -1;
    for (int i = 0; i < siblings.size(); ++i)
        if (siblings[i].id == taskId)
            at = i;
    const int to = at + delta;
    if (at < 0 || to < 0 || to >= siblings.size())
        return;
    QList<Task> now = siblings;
    now.move(at, to);
    reorderTasks(*found, siblings, now);
}

void Backend::reorderTasks(const Task &task, const QList<Task> &old, const QList<Task> &now) {
    Step step{Step::Reordered, task.id, task.content, task, viewProjectId()};
    step.command = "item_reorder";
    step.before = itemOrders(old, false);
    step.after = itemOrders(now, true);
    applyReorder(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::moveSectionBy(const QString &sectionId, int delta) {
    QString project;
    for (const auto &section : m_sections)
        if (section.id == sectionId)
            project = section.projectId;
    if (project.isEmpty() || delta == 0)
        return;
    const QList<Section> sections = sectionsOf(project);
    int at = -1;
    for (int i = 0; i < sections.size(); ++i)
        if (sections[i].id == sectionId)
            at = i;
    const int to = at + delta;
    if (to < 0 || to >= sections.size())
        return;
    QList<Section> now = sections;
    now.move(at, to);
    reorderSections(sections[at], sections, now);
}

void Backend::reorderSections(const Section &section, const QList<Section> &old, const QList<Section> &now) {
    auto orders = [](const QList<Section> &list, bool renumber) {
        return orderArgs(list, &Section::order, "sections", "section_order", renumber);
    };
    Step step{Step::Reordered, section.id, section.name, {}, section.projectId};
    step.command = "section_reorder";
    step.before = orders(old, false);
    step.after = orders(now, true);
    applyReorder(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

QList<Project> Backend::projectSiblings(const Project &project) const {
    // Its siblings in the sidebar, which the Inbox isn't among.
    auto parentOf = [this](const Project &p) { return m_projectsById.contains(p.parentId) ? p.parentId : QString(); };
    QList<Project> siblings;
    for (const auto &candidate : m_projects)
        if (!candidate.inbox && parentOf(candidate) == parentOf(project))
            siblings << candidate;
    return siblings;
}

void Backend::moveProjectBy(const QString &projectId, int delta) {
    const Project project = m_projectsById.value(projectId);
    if (project.id.isEmpty() || project.inbox || delta == 0)
        return;
    const QList<Project> siblings = projectSiblings(project);
    int at = -1;
    for (int i = 0; i < siblings.size(); ++i)
        if (siblings[i].id == projectId)
            at = i;
    const int to = at + delta;
    if (to < 0 || to >= siblings.size())
        return;
    QList<Project> now = siblings;
    now.move(at, to);
    reorderProjects(project, siblings, now);
}

void Backend::reorderProjects(const Project &project, const QList<Project> &old, const QList<Project> &now) {
    auto orders = [](const QList<Project> &list, bool renumber) {
        return orderArgs(list, &Project::order, "projects", "child_order", renumber);
    };
    Step step{Step::Reordered, project.id, project.name};
    step.command = "project_reorder";
    step.before = orders(old, false);
    step.after = orders(now, true);
    applyReorder(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

// ------------------------------------------------------------ drag and drop

void Backend::dropTask(const QString &taskId, int rowIndex) {
    const QString project = viewProjectId();
    const Task *found = findTask(taskId);
    if (project.isEmpty() || !found)
        return;
    const Task task = *found;
    // It moves with its subtasks, so none of them count as neighbours.
    const QSet<QString> moving = subtreeOf(taskId);
    const QVariantList list = rows();
    auto movingRow = [&moving](const QVariantMap &row) {
        return row.value("kind") == "task" && moving.contains(row.value("id").toString());
    };
    QVariantMap above, below;
    for (int i = qMin(rowIndex, int(list.size())) - 1; i >= 0 && above.isEmpty(); --i)
        if (!movingRow(list[i].toMap()))
            above = list[i].toMap();
    for (int i = qMax(rowIndex, 0); i < list.size() && below.isEmpty(); ++i)
        if (!movingRow(list[i].toMap()))
            below = list[i].toMap();

    QSet<QString> ids, sections;
    for (const auto &t : m_tasks)
        ids.insert(t.id);
    for (const auto &section : sectionsOf(project))
        sections.insert(section.id);
    auto parentOf = [&ids](const Task &t) { return ids.contains(t.parentId) ? t.parentId : QString(); };
    auto sectionOf = [&sections](const Task &t) { return sections.contains(t.sectionId) ? t.sectionId : QString(); };

    // It takes the level of the task it lands in front of; with none (the
    // end of a section, or an empty one), the top of the section above.
    QString parent, section, before;
    if (below.value("kind") == "task") {
        const Task *next = findTask(below.value("id").toString());
        parent = parentOf(*next);
        section = sectionOf(*next);
        before = next->id;
    } else if (above.value("kind") == "section") {
        section = above.value("id").toString();
    } else if (above.value("kind") == "task") {
        section = sectionOf(*findTask(above.value("id").toString()));
    }

    Task placed = task;
    placed.parentId = parent;
    placed.sectionId = section;
    QList<Task> now;
    for (const auto &sibling : siblingsOf(placed))
        if (sibling.id != taskId)
            now << sibling;
    int at = int(now.size());
    for (int i = 0; i < now.size(); ++i)
        if (now[i].id == before)
            at = i;
    now.insert(at, task);
    const QList<Task> old = siblingsOf(task);

    if (parent == parentOf(task) && section == sectionOf(task)) {
        QStringList was, is;
        for (const auto &t : old)
            was << t.id;
        for (const auto &t : now)
            is << t.id;
        if (was != is)
            reorderTasks(task, old, now);
        return;
    }
    Step step{Step::Moved, task.id, task.content, task, task.projectId, task.projectId, task.sectionId, section};
    step.fromParent = task.parentId;
    step.toParent = parent;
    step.before = itemOrders(old, false);
    step.after = itemOrders(now, true);
    applyMove(step, true, [this, step](const ApiError &error) {
        if (!error.isEmpty()) {
            fail(tr("Couldn't move “%1”. %2").arg(step.text, error.message));
            return;
        }
        m_redo.clear();
        pushUndo(step);
    });
}

void Backend::dropSection(const QString &sectionId, int rowIndex) {
    Section section;
    for (const auto &candidate : m_sections)
        if (candidate.id == sectionId)
            section = candidate;
    if (section.id.isEmpty() || section.projectId != viewProjectId())
        return;
    // Its place among the headings above where it's dropped.
    const QVariantList list = rows();
    int place = 0;
    for (int i = 0; i < qMin(rowIndex, int(list.size())); ++i) {
        const QVariantMap row = list[i].toMap();
        if (row.value("kind") == "section" && row.value("id") != sectionId)
            ++place;
    }
    const QList<Section> old = sectionsOf(section.projectId);
    QList<Section> now = old;
    now.removeIf([&sectionId](const Section &s) { return s.id == sectionId; });
    now.insert(qMin(place, int(now.size())), section);
    bool same = true;
    for (int i = 0; i < old.size(); ++i)
        same &= old[i].id == now[i].id;
    if (!same)
        reorderSections(section, old, now);
}

void Backend::dropProject(const QString &projectId, int index) {
    const Project project = m_projectsById.value(projectId);
    if (project.id.isEmpty() || project.inbox)
        return;
    // Among its siblings only: Shift+M is what nests it elsewhere.
    QStringList shown;
    for (const auto &entry : projects())
        shown << entry.toMap().value("id").toString();
    const QList<Project> old = projectSiblings(project);
    QList<Project> now;
    int place = 0;
    for (const auto &sibling : old) {
        if (sibling.id == projectId)
            continue;
        now << sibling;
        if (shown.indexOf(sibling.id) < index)
            place = int(now.size());
    }
    now.insert(place, project);
    bool same = true;
    for (int i = 0; i < old.size(); ++i)
        same &= old[i].id == now[i].id;
    if (!same)
        reorderProjects(project, old, now);
}

// Shown in its new place at once; a failure reloads Todoist's order.
void Backend::applyReorder(const Step &step, bool forward, std::function<void(const ApiError &)> done) {
    const QJsonObject args = forward ? step.after : step.before;
    applyOrders(step.command, args);
    begin();
    m_client.command(step.command, args, [this, done, step](const QJsonObject &, const ApiError &error) {
        end();
        done(error);
        if (!error.isEmpty()) {
            if (step.command == "item_reorder")
                reloadSoon();
            else
                refresh();
        }
    });
}

// ------------------------------------------------------------ subtasks

QVariantMap Backend::taskById(const QString &taskId) const {
    const Task *task = findTask(taskId);
    if (!task)
        return {};
    QVariantMap map = Tasks::toVariant(*task, QDateTime::currentDateTime(), m_projectsById, m_depths.value(taskId));
    // Where it lives, as the move picker names it.
    QString path = m_projectsById.value(task->projectId).inbox ? tr("Inbox") : m_projectsById.value(task->projectId).name;
    for (const auto &section : m_sections)
        if (section.id == task->sectionId)
            path += " / " + section.name;
    map.insert("path", path);
    map.insert("kind", "task");
    return map;
}

QVariantList Backend::subtasksOf(const QString &taskId) const {
    QList<Task> children;
    QSet<QString> seen;
    for (const auto &task : m_tasks)
        if (task.parentId == taskId) {
            children << task;
            seen.insert(task.id);
        }
    for (const auto &task : m_subtasks.value(taskId))
        if (!seen.contains(task.id))
            children << task;
    std::stable_sort(children.begin(), children.end(),
                     [](const Task &a, const Task &b) { return a.childOrder < b.childOrder; });
    const QDateTime now = QDateTime::currentDateTime();
    QVariantList list;
    for (const auto &task : children) {
        QVariantMap map = Tasks::toVariant(task, now, m_projectsById);
        map.insert("kind", "task");
        list << map;
    }
    return list;
}

void Backend::loadSubtasks(const QString &taskId) {
    if (taskId.isEmpty() || !signedIn())
        return;
    m_watching = taskId;
    fetchSubtasks(taskId);
}

void Backend::fetchSubtasks(const QString &taskId) {
    const int generation = m_subtaskGeneration;
    begin();
    m_client.subtasks(taskId, [this, taskId, generation](const QJsonArray &results, const ApiError &error) {
        end();
        // Forgotten since, or not to be had: what's in the list still shows.
        if (generation != m_subtaskGeneration || !error.isEmpty())
            return;
        QList<Task> children;
        for (const auto &task : Tasks::parse(results))
            if (task.parentId == taskId)
                children << task;
        m_subtasks.insert(taskId, children);
        emit subtasksChanged();
    });
}

void Backend::forgetSubtasks() {
    ++m_subtaskGeneration;
    m_watching.clear();
    if (m_subtasks.isEmpty())
        return;
    m_subtasks.clear();
    emit subtasksChanged();
}
