#include "todoist.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUuid>
#include <memory>


static const char *todoistBase = "https://api.todoist.com/api/v1";

QString Todoist::resolveBase(const QString &override) {
    QString base = override.trimmed();
    const QUrl url(base);
    const bool local = url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    if (base.isEmpty() || !url.isValid() || !(url.scheme() == "https" || (url.scheme() == "http" && local)))
        base = todoistBase;
    while (base.endsWith('/'))
        base.chop(1);
    return base;
}

Todoist::Todoist(QObject *parent)
    : QObject(parent), m_base(resolveBase(qEnvironmentVariable("ZERO_API_URL"))) {
    // The token rides in a header, so it must never follow a redirect to
    // another host.
    m_network.setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
}

void Todoist::send(const QByteArray &verb, const QString &path, const QUrlQuery &query, const QByteArray &body,
                   ObjectCallback done) {
    if (m_token.isEmpty()) {
        // Counts as Todoist refusing it: there's no token to accept.
        done({}, ApiError(tr("Not signed in to Todoist."), 401));
        return;
    }
    QUrl url(m_base + path);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, "zero/" + QCoreApplication::applicationVersion());
    request.setTransferTimeout(20000);
    if (!body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_network.sendCustomRequest(request, verb, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (status == 401 || status == 403) {
            done({}, ApiError(tr("Todoist didn't accept the API token. Check it under Settings → Integrations → Developer."), status));
            return;
        }
        if (status == 0 || reply->error() == QNetworkReply::OperationCanceledError ||
            reply->error() == QNetworkReply::TimeoutError) {
            done({}, ApiError(tr("Couldn't reach Todoist: %1").arg(reply->errorString()), 0));
            return;
        }
        if (status >= 400) {
            // Todoist explains most failures in a JSON "error" field.
            const QJsonObject object = QJsonDocument::fromJson(data).object();
            QString message = object.value("error").toString();
            if (message.isEmpty())
                message = QString::fromUtf8(data.left(200)).trimmed();
            if (message.isEmpty())
                message = reply->errorString();
            done({}, ApiError(tr("Todoist said %1: %2").arg(status).arg(message), status));
            return;
        }
        if (data.trimmed().isEmpty()) {
            done({}, {});
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            // Reached, so not a network error: it's Todoist's answer that's off.
            done({}, ApiError(tr("Todoist sent a reply that isn't JSON."), status));
            return;
        }
        // A bare array is wrapped, so list callers see the same shape as the
        // paginated {"results": [...]} replies.
        done(document.isArray() ? QJsonObject{{"results", document.array()}} : document.object(), {});
    });
}

void Todoist::list(const QString &path, QUrlQuery query, ListCallback done) {
    auto collected = std::make_shared<QJsonArray>();
    auto pages = std::make_shared<int>(0);
    // Each page's request holds the pager; the pager only a weak handle on
    // itself, so a request that never answers (the app quits) frees it.
    using Pager = std::function<void(const QString &)>;
    auto page = std::make_shared<Pager>();
    const std::weak_ptr<Pager> weak = page;
    const QPointer<Todoist> self(this);
    *page = [self, path, query, collected, pages, weak, done](const QString &cursor) {
        const std::shared_ptr<Pager> pager = weak.lock();
        if (!self || !pager)
            return;
        QUrlQuery pageQuery = query;
        pageQuery.addQueryItem("limit", "200");
        if (!cursor.isEmpty())
            pageQuery.addQueryItem("cursor", cursor);
        ++*pages;
        self->send("GET", path, pageQuery, {}, [collected, pages, pager, done](const QJsonObject &result, const ApiError &error) {
            if (!error.isEmpty()) {
                done({}, error);
                return;
            }
            for (const auto &item : result.value("results").toArray())
                collected->append(item);
            const QString next = result.value("next_cursor").toString();
            // 500 pages of 200 is far more than any account holds; stop
            // rather than loop forever if a server keeps handing back cursors,
            // empty pages included.
            if (!next.isEmpty() && *pages < 500)
                (*pager)(next);
            else
                done(*collected, {});
        });
    };
    (*page)({});
}

void Todoist::projects(ListCallback done) { list("/projects", {}, std::move(done)); }

void Todoist::tasksInProject(const QString &projectId, ListCallback done) {
    QUrlQuery query;
    query.addQueryItem("project_id", projectId);
    list("/tasks", query, std::move(done));
}

void Todoist::tasksMatching(const QString &filter, ListCallback done) {
    QUrlQuery query;
    query.addQueryItem("query", QString::fromUtf8(QUrl::toPercentEncoding(filter)));
    list("/tasks/filter", query, std::move(done));
}

void Todoist::quickAdd(const QString &text, ObjectCallback done) {
    const QByteArray body = QJsonDocument(QJsonObject{{"text", text}}).toJson(QJsonDocument::Compact);
    send("POST", "/tasks/quick", {}, body, std::move(done));
}

void Todoist::close(const QString &taskId, ObjectCallback done) {
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId) + "/close", {}, {}, std::move(done));
}

void Todoist::reopen(const QString &taskId, ObjectCallback done) {
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId) + "/reopen", {}, {}, std::move(done));
}

void Todoist::remove(const QString &taskId, ObjectCallback done) {
    send("DELETE", "/tasks/" + QUrl::toPercentEncoding(taskId), {}, {}, std::move(done));
}

void Todoist::move(const QString &taskId, const QString &projectId, ObjectCallback done) {
    const QByteArray body = QJsonDocument(QJsonObject{{"project_id", projectId}}).toJson(QJsonDocument::Compact);
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId) + "/move", {}, body, std::move(done));
}

void Todoist::moveUnder(const QString &taskId, const QString &parentId, ObjectCallback done) {
    const QByteArray body = QJsonDocument(QJsonObject{{"parent_id", parentId}}).toJson(QJsonDocument::Compact);
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId) + "/move", {}, body, std::move(done));
}

void Todoist::subtasks(const QString &parentId, ListCallback done) {
    QUrlQuery query;
    query.addQueryItem("parent_id", parentId);
    list("/tasks", query, std::move(done));
}

void Todoist::update(const QString &taskId, const QJsonObject &fields, ObjectCallback done) {
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId), {}, QJsonDocument(fields).toJson(QJsonDocument::Compact),
         std::move(done));
}

void Todoist::createProject(const QString &name, const QString &color, ObjectCallback done) {
    QJsonObject fields{{"name", name}};
    if (!color.isEmpty())
        fields.insert("color", color);
    const QByteArray body = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    send("POST", "/projects", {}, body, std::move(done));
}

void Todoist::deleteProject(const QString &projectId, ObjectCallback done) {
    send("DELETE", "/projects/" + QUrl::toPercentEncoding(projectId), {}, {}, std::move(done));
}

void Todoist::updateProject(const QString &projectId, const QJsonObject &fields, ObjectCallback done) {
    send("POST", "/projects/" + QUrl::toPercentEncoding(projectId), {},
         QJsonDocument(fields).toJson(QJsonDocument::Compact), std::move(done));
}

void Todoist::moveToSection(const QString &taskId, const QString &sectionId, ObjectCallback done) {
    const QByteArray body = QJsonDocument(QJsonObject{{"section_id", sectionId}}).toJson(QJsonDocument::Compact);
    send("POST", "/tasks/" + QUrl::toPercentEncoding(taskId) + "/move", {}, body, std::move(done));
}

void Todoist::sections(const QString &projectId, ListCallback done) {
    QUrlQuery query;
    if (!projectId.isEmpty())
        query.addQueryItem("project_id", projectId);
    list("/sections", query, std::move(done));
}

void Todoist::createSection(const QString &name, const QString &projectId, ObjectCallback done) {
    const QByteArray body =
        QJsonDocument(QJsonObject{{"name", name}, {"project_id", projectId}}).toJson(QJsonDocument::Compact);
    send("POST", "/sections", {}, body, std::move(done));
}

void Todoist::deleteSection(const QString &sectionId, ObjectCallback done) {
    send("DELETE", "/sections/" + QUrl::toPercentEncoding(sectionId), {}, {}, std::move(done));
}

void Todoist::updateSection(const QString &sectionId, const QJsonObject &fields, ObjectCallback done) {
    send("POST", "/sections/" + QUrl::toPercentEncoding(sectionId), {},
         QJsonDocument(fields).toJson(QJsonDocument::Compact), std::move(done));
}

void Todoist::moveProject(const QString &projectId, const QString &parentId, ObjectCallback done) {
    command("project_move",
            QJsonObject{{"id", projectId}, {"parent_id", parentId.isEmpty() ? QJsonValue() : QJsonValue(parentId)}},
            std::move(done));
}

void Todoist::command(const QString &type, const QJsonObject &args, ObjectCallback done) {
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonArray commands{QJsonObject{{"type", type}, {"uuid", uuid}, {"args", args}}};
    const QByteArray body = QJsonDocument(QJsonObject{{"commands", commands}}).toJson(QJsonDocument::Compact);
    send("POST", "/sync", {}, body, [uuid, done](const QJsonObject &result, const ApiError &error) {
        if (!error.isEmpty()) {
            done({}, error);
            return;
        }
        const QJsonValue status = result.value("sync_status").toObject().value(uuid);
        if (status.toString() == "ok") {
            done(result, {});
            return;
        }
        // {"error": "...", "http_code": 400, ...}, or nothing back at all.
        const QJsonObject failure = status.toObject();
        QString message = failure.value("error").toString();
        if (message.isEmpty())
            message = tr("no answer for the change");
        const int code = failure.value("http_code").toInt(400);
        done({}, ApiError(tr("Todoist said %1: %2").arg(code).arg(message), code));
    });
}
