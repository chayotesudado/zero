#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <functional>

// What went wrong with a request: a message to show, and the HTTP status
// (0 when Todoist couldn't be reached; 401 when there's no token to send).
// It doesn't turn back into a QString on its own, so the status can't be
// dropped by accident: callers that only show it take .message.
struct ApiError {
    QString message;
    int status = 0;

    ApiError(const QString &message = {}, int status = 0) : message(message), status(status) {}
    bool isEmpty() const { return message.isEmpty(); }
    bool isAuth() const { return status == 401 || status == 403; }
    bool isNetwork() const { return !isEmpty() && status == 0; }
    // Worth trying again later: Todoist was unreachable, busy, or didn't take
    // the token. Anything else (a task deleted elsewhere: 404) fails the same
    // way every time.
    bool isTransient() const { return status == 0 || status == 401 || status == 403 || status == 429 || status >= 500; }
};

// A small client for Todoist's API v1. Every call is asynchronous and reports
// back once, with either a result or a message saying what went wrong.
// ZERO_API_URL points it somewhere else, which the tests use.
class Todoist : public QObject {
    Q_OBJECT

public:
    using ListCallback = std::function<void(const QJsonArray &results, const ApiError &error)>;
    using ObjectCallback = std::function<void(const QJsonObject &result, const ApiError &error)>;

    explicit Todoist(QObject *parent = nullptr);

    void setToken(const QString &token) { m_token = token; }
    QString token() const { return m_token; }
    QString baseUrl() const { return m_base; }

    // Where requests go: Todoist, or ZERO_API_URL when it's https or this
    // machine (the tests' fake server). Anything else falls back to Todoist,
    // so the token is never sent in the clear to another host.
    static QString resolveBase(const QString &override);

    void projects(ListCallback done);
    void tasksInProject(const QString &projectId, ListCallback done);
    // Todoist's own filter syntax, as typed in its search bar.
    void tasksMatching(const QString &query, ListCallback done);
    // Natural language, like the Todoist quick-add box: dates, #project,
    // @label, and p1–p4 are parsed out. Without a #project it goes to the Inbox.
    void quickAdd(const QString &text, ObjectCallback done);
    void close(const QString &taskId, ObjectCallback done);
    void reopen(const QString &taskId, ObjectCallback done);
    void remove(const QString &taskId, ObjectCallback done);
    // color is one of Todoist's names (Tasks::colorNames()), or empty.
    void createProject(const QString &name, const QString &color, ObjectCallback done);
    void updateProject(const QString &projectId, const QJsonObject &fields, ObjectCallback done);
    // Todoist deletes the project's tasks and sub-projects with it.
    void deleteProject(const QString &projectId, ObjectCallback done);
    void move(const QString &taskId, const QString &projectId, ObjectCallback done);
    // Into a section, and so into the section's project.
    void moveToSection(const QString &taskId, const QString &sectionId, ObjectCallback done);
    // Every section, or a project's.
    void sections(const QString &projectId, ListCallback done);
    void createSection(const QString &name, const QString &projectId, ObjectCallback done);
    // Todoist deletes the section's tasks with it.
    void deleteSection(const QString &sectionId, ObjectCallback done);
    // Makes a project a sub-project of another, or top-level when parentId
    // is empty. REST can't, so this is a Sync API command.
    void moveProject(const QString &projectId, const QString &parentId, ObjectCallback done);
    // Fields as Todoist's update takes them, e.g. {"due_string": "tomorrow"}.
    void update(const QString &taskId, const QJsonObject &fields, ObjectCallback done);

    // Collect every page of a paginated list endpoint.
    void list(const QString &path, QUrlQuery query, ListCallback done);

private:
    // One Sync API command; its own failure (reported inside a 200) counts
    // as an error like any other.
    void command(const QString &type, const QJsonObject &args, ObjectCallback done);
    void send(const QByteArray &verb, const QString &path, const QUrlQuery &query, const QByteArray &body,
              ObjectCallback done);

    QNetworkAccessManager m_network;
    QString m_token;
    QString m_base;
};
