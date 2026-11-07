#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <functional>

// A small client for Todoist's API v1. Every call is asynchronous and reports
// back once, with either a result or a message saying what went wrong.
// ZERO_API_URL points it somewhere else, which the tests use.
class Todoist : public QObject {
    Q_OBJECT

public:
    using ListCallback = std::function<void(const QJsonArray &results, const QString &error)>;
    using ObjectCallback = std::function<void(const QJsonObject &result, const QString &error)>;

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
    // Fields as Todoist's update takes them, e.g. {"due_string": "tomorrow"}.
    void update(const QString &taskId, const QJsonObject &fields, ObjectCallback done);

    // Collect every page of a paginated list endpoint.
    void list(const QString &path, QUrlQuery query, ListCallback done);

private:
    void send(const QByteArray &verb, const QString &path, const QUrlQuery &query, const QByteArray &body,
              ObjectCallback done);

    QNetworkAccessManager m_network;
    QString m_token;
    QString m_base;
};
