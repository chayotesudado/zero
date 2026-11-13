#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "credentials.h"
#include "fakeserver.h"
#include "pixelart.h"
#include "tasks.h"
#include "theme.h"
#include "todoist.h"
#include "translator.h"

namespace {

QJsonObject task(const QString &id, const QString &content, const QJsonValue &due = QJsonValue(),
                 int priority = 1, const QString &parent = {}, int order = 0) {
    QJsonObject object{{"id", id}, {"content", content}, {"project_id", "inbox1"}, {"priority", priority},
                       {"child_order", order}, {"labels", QJsonArray()}};
    object.insert("parent_id", parent.isEmpty() ? QJsonValue() : QJsonValue(parent));
    object.insert("due", due.isString() ? QJsonValue(QJsonObject{{"date", due}, {"string", due}, {"is_recurring", false}}) : due);
    return object;
}

QByteArray page(const QJsonArray &results, const QString &next = {}) {
    return QJsonDocument(QJsonObject{{"results", results}, {"next_cursor", next.isEmpty() ? QJsonValue() : QJsonValue(next)}})
        .toJson(QJsonDocument::Compact);
}

const QJsonArray projects{
    QJsonObject{{"id", "inbox1"}, {"name", "Inbox"}, {"inbox_project", true}, {"color", "grey"}, {"child_order", 0}},
    QJsonObject{{"id", "work"}, {"name", "Work"}, {"color", "blue"}, {"child_order", 1}},
};

QString iso(int days) { return QDate::currentDate().addDays(days).toString(Qt::ISODate); }

// Todoist's answer to a Sync API request: every command went through.
FakeServer::Response syncOk(const FakeServer::Request &request) {
    QJsonObject status;
    for (const auto &command : QJsonDocument::fromJson(request.body).object().value("commands").toArray())
        status.insert(command.toObject().value("uuid").toString(), "ok");
    return {200, QJsonDocument(QJsonObject{{"sync_status", status}}).toJson()};
}

// The Sync commands sent since the last call, as {type, args}, clearing the log.
QList<QJsonObject> takeCommands(FakeServer &server) {
    QList<QJsonObject> found;
    for (const auto &r : server.requests)
        if (r.path() == "/sync")
            for (const auto &command : QJsonDocument::fromJson(r.body).object().value("commands").toArray())
                found << command.toObject();
    server.requests.clear();
    return found;
}

QJsonObject body(const FakeServer::Request &request) { return QJsonDocument::fromJson(request.body).object(); }

QJsonArray orders(std::initializer_list<std::pair<const char *, int>> items, const char *field = "child_order") {
    QJsonArray list;
    for (const auto &[id, order] : items)
        list << QJsonObject{{"id", id}, {field, order}};
    return list;
}

} // namespace

class ZeroTests : public QObject {
    Q_OBJECT

private slots:
    // The drawn logo is the brand art: its faces, plus two pixels of depth.
    void pixelArtIsTheLogo() {
        const auto lockup = PixelArt::layout("lockup");
        QCOMPARE(lockup.width, 55);
        QCOMPARE(lockup.height, 10);
        QCOMPARE(lockup.face.size(), 154);
        const auto check = PixelArt::layout("check");
        QCOMPARE(check.width, 14);
        QCOMPARE(check.height, 10);
        QVERIFY(std::all_of(check.face.cbegin(), check.face.cend(), [](QPoint p) { return p.x() < 12; }));
        QVERIFY(PixelArt::layout("nope").face.isEmpty());
    }

    void parsesDueDates() {
        const Task plain = Task::fromJson(task("1", "a", "2026-11-06"));
        QCOMPARE(plain.dueDate, QDate(2026, 11, 6));
        QVERIFY(!plain.hasTime());

        const Task floating = Task::fromJson(task("2", "b", "2026-11-06T17:30:00"));
        QCOMPARE(floating.dueDate, QDate(2026, 11, 6));
        QCOMPARE(floating.dueTime, QTime(17, 30));

        const Task utc = Task::fromJson(task("3", "c", "2026-11-06T17:30:00Z"));
        const QDateTime local = QDateTime(QDate(2026, 11, 6), QTime(17, 30), QTimeZone::UTC).toLocalTime();
        QCOMPARE(utc.dueDate, local.date());
        QCOMPARE(utc.dueTime, local.time());

        QVERIFY(!Task::fromJson(task("4", "d")).hasDue());
        // Todoist's 4 is the user's p1.
        QCOMPARE(Tasks::toVariant(Task::fromJson(task("5", "e", QJsonValue(), 4)), QDateTime::currentDateTime(), {})
                     .value("priority").toInt(), 1);
    }

    void knowsWhatIsOverdue() {
        const QDateTime now(QDate(2026, 11, 6), QTime(12, 0));
        QVERIFY(Task::fromJson(task("1", "a", "2026-11-05")).isOverdue(now));
        QVERIFY(!Task::fromJson(task("2", "b", "2026-11-06")).isOverdue(now));
        QVERIFY(Task::fromJson(task("3", "c", "2026-11-06T09:00:00")).isOverdue(now));
        QVERIFY(!Task::fromJson(task("4", "d", "2026-11-06T15:00:00")).isOverdue(now));
        QVERIFY(!Task::fromJson(task("5", "e")).isOverdue(now));
    }

    void sortsByDue() {
        QList<Task> tasks = Tasks::parse({
            task("none", "no date"),
            task("late", "untimed today", "2026-11-06", 1),
            task("urgent", "untimed today p1", "2026-11-06", 4),
            task("timed", "timed today", "2026-11-06T09:00:00"),
            task("old", "yesterday", "2026-11-05"),
        });
        Tasks::sortByDue(tasks);
        QStringList ids;
        for (const auto &t : tasks)
            ids << t.id;
        QCOMPARE(ids, QStringList({"old", "timed", "urgent", "late", "none"}));
    }

    void ordersSubtasksUnderParents() {
        QHash<QString, int> depths;
        const QList<Task> tasks = Tasks::sortAsTree(Tasks::parse({
            task("child", "child", QJsonValue(), 1, "parent", 0),
            task("second", "second", QJsonValue(), 1, {}, 2),
            task("parent", "parent", QJsonValue(), 1, {}, 1),
            task("orphan", "orphan", QJsonValue(), 1, "elsewhere", 3),
        }), &depths);
        QStringList ids;
        for (const auto &t : tasks)
            ids << t.id;
        QCOMPARE(ids, QStringList({"parent", "child", "second", "orphan"}));
        QCOMPARE(depths.value("child"), 1);
        QCOMPARE(depths.value("orphan"), 0);
    }

    void ordersSubProjectsUnderParents() {
        // child_order only ranks siblings, so a sub-project's 0 mustn't put
        // it above its parent.
        const QList<Project> list = Tasks::parseProjects({
            QJsonObject{{"id", "home"}, {"name", "Home"}, {"child_order", 2}},
            QJsonObject{{"id", "deep"}, {"name", "Deep"}, {"parent_id", "sub"}, {"child_order", 0}},
            QJsonObject{{"id", "sub"}, {"name", "Sub"}, {"parent_id", "work"}, {"child_order", 0}},
            QJsonObject{{"id", "orphan"}, {"name", "Orphan"}, {"parent_id", "archived"}, {"child_order", 3}},
            projects.at(1), projects.at(0),
        });
        QStringList ids;
        for (const auto &p : list)
            ids << p.id;
        QCOMPARE(ids, QStringList({"inbox1", "work", "sub", "deep", "home", "orphan"}));
    }

    void backendNestsSubProjects() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "sub"}, {"name", "Sub"}, {"parent_id", "work"}, {"child_order", 0}}
             << QJsonObject{{"id", "orphan"}, {"name", "Orphan"}, {"parent_id", "archived"}, {"child_order", 5}};
        FakeServer server([&list](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(list)};
            return {200, page({})};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        QStringList shown;
        for (const auto &entry : backend.projects()) {
            const QVariantMap map = entry.toMap();
            shown << map.value("id").toString() + ":" + QString::number(map.value("depth").toInt());
        }
        QCOMPARE(shown, QStringList({"work:0", "sub:1", "orphan:0"}));
    }

    void skipsCheckedAndArchived() {
        QJsonObject done = task("1", "done");
        done.insert("checked", true);
        QCOMPARE(Tasks::parse({done, task("2", "open")}).size(), 1);
        QJsonObject archived = projects.at(1).toObject();
        archived.insert("is_archived", true);
        const QList<Project> list = Tasks::parseProjects({archived, projects.at(0)});
        QCOMPARE(list.size(), 1);
        QVERIFY(list.first().inbox);
    }

    void keepsTheTokenPrivate() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/zero/token");
        QCOMPARE(credentials.read(), QString());
        QCOMPARE(credentials.write("  secret\n"), QString());
        QCOMPARE(credentials.read(), QString("secret"));
        const auto permissions = QFile::permissions(credentials.path());
        QVERIFY(permissions & QFile::ReadOwner);
        QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)));

        qputenv("TODOIST_API_TOKEN", "from-env");
        QCOMPARE(credentials.read(), QString("from-env"));
        qunsetenv("TODOIST_API_TOKEN");

        QCOMPARE(credentials.clear(), QString());
        QVERIFY(!QFile::exists(credentials.path()));
    }

    void movesTheOldTickedToken() {
        QTemporaryDir dir;
        const QString legacy = dir.path() + "/ticked/token", current = dir.path() + "/zero/token";
        QDir().mkpath(dir.path() + "/ticked");
        write(legacy, "old-token\n");
        Credentials::migrateLegacy(legacy, current);
        QVERIFY(!QFile::exists(legacy));
        QVERIFY(!QDir(dir.path() + "/ticked").exists());
        QCOMPARE(Credentials(current).read(), QString("old-token"));

        // An existing token is never overwritten.
        QDir().mkpath(dir.path() + "/ticked");
        write(legacy, "older\n");
        Credentials::migrateLegacy(legacy, current);
        QCOMPARE(Credentials(current).read(), QString("old-token"));
    }

    void clientFollowsPages() {
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.headers.value("authorization") != "Bearer good")
                return {401, R"({"error":"Unauthorized"})"};
            if (request.target.contains("cursor=next"))
                return {200, page({task("2", "second")})};
            return {200, page({task("1", "first")}, "next")};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());

        Todoist client;
        client.setToken("good");
        QJsonArray results;
        ApiError error;
        bool done = false;
        client.tasksMatching("today | overdue", [&](const QJsonArray &r, const ApiError &e) { results = r; error = e; done = true; });
        QTRY_VERIFY(done);
        QVERIFY(error.isEmpty());
        QCOMPARE(results.size(), 2);
        QVERIFY(server.requests.first().target.startsWith("/tasks/filter?query=today%20%7C%20overdue"));

        client.setToken("bad");
        done = false;
        client.projects([&](const QJsonArray &, const ApiError &e) { error = e; done = true; });
        QTRY_VERIFY(done);
        QVERIFY(error.isAuth());
        QVERIFY(error.message.contains("token"));
    }

    void tokenOnlyGoesToSafePlaces() {
        const QString todoist = "https://api.todoist.com/api/v1";
        QCOMPARE(Todoist::resolveBase({}), todoist);
        QCOMPARE(Todoist::resolveBase("http://evil.example/api"), todoist);
        QCOMPARE(Todoist::resolveBase("ftp://127.0.0.1"), todoist);
        QCOMPARE(Todoist::resolveBase("http://127.0.0.1:8765/"), QString("http://127.0.0.1:8765"));
        QCOMPARE(Todoist::resolveBase("https://proxy.example/v1"), QString("https://proxy.example/v1"));
    }

    void tokenDoesNotFollowRedirectsElsewhere() {
        FakeServer elsewhere([](const FakeServer::Request &) -> FakeServer::Response { return {200, "{}"}; });
        const QByteArray location = "Location: " + elsewhere.url().toUtf8().replace("127.0.0.1", "localhost") + "/steal\r\n";
        FakeServer server([location](const FakeServer::Request &) -> FakeServer::Response {
            return {302, {}, location};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Todoist client;
        client.setToken("secret");
        bool done = false;
        ApiError error;
        client.projects([&](const QJsonArray &, const ApiError &e) { error = e; done = true; });
        QTRY_VERIFY(done);
        QTest::qWait(100);
        QVERIFY(elsewhere.requests.isEmpty());
    }

    void stopsPagingForever() {
        int calls = 0;
        FakeServer server([&calls](const FakeServer::Request &) -> FakeServer::Response {
            ++calls;
            return {200, page({}, "again")};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Todoist client;
        client.setToken("good");
        bool done = false;
        client.projects([&](const QJsonArray &, const ApiError &) { done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 30000);
        QCOMPARE(calls, 500);
    }

    void backendCompletesAndUndoes() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/filter")
                return {200, page({task("a", "Water plants", iso(0)), task("b", "Call back", iso(-1))})};
            if (request.path() == "/tasks")
                return {200, page({})};
            if (request.method == "POST")
                return {204, {}};
            return {404, R"({"error":"nope"})"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());

        Backend backend(credentials);
        QVERIFY(backend.signedIn());
        QTRY_VERIFY(backend.loaded());
        QTRY_COMPARE(backend.todayCount(), 2);
        QCOMPARE(backend.tasks().size(), 2);
        // Overdue first.
        QCOMPARE(backend.tasks().first().toMap().value("id").toString(), QString("b"));

        backend.complete("a");
        QCOMPARE(backend.tasks().size(), 1);
        QCOMPARE(backend.todayCount(), 1);
        QTRY_VERIFY(backend.canUndo());
        QCOMPARE(server.requests.last().path(), QByteArray("/tasks/a/close"));

        backend.undo();
        QTRY_VERIFY(backend.canRedo());
        bool reopened = false;
        for (const auto &request : server.requests)
            reopened |= request.path() == "/tasks/a/reopen";
        QVERIFY(reopened);
    }

    void backendFilesAddsByTheirPrefix() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "stuff"}, {"name", "My Stuff"}, {"child_order", 2}};
        const QJsonArray sections{
            QJsonObject{{"id", "s1"}, {"name", "Next week"}, {"project_id", "stuff"}, {"section_order", 1}},
            QJsonObject{{"id", "s2"}, {"name", "Later"}, {"project_id", "inbox1"}, {"section_order", 1}},
        };
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(list)};
            if (request.path() == "/sections" && request.method == "GET")
                return {200, page(sections)};
            if (request.path() == "/tasks/quick") {
                QJsonObject made = task("new", "Milk");
                if (request.body.contains("#Work"))
                    made.insert("project_id", "work");
                return {200, QJsonDocument(made).toJson()};
            }
            if (request.method == "GET")
                return {200, page({})};
            return {200, QJsonDocument(task("new", "Milk")).toJson()};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto posts = [&] {
            QList<QByteArray> sent;
            for (const auto &r : server.requests)
                if (r.method != "GET")
                    sent << r.path() + " " + r.body;
            server.requests.clear();
            return sent;
        };
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        auto quickText = [](const QByteArray &post) {
            return QJsonDocument::fromJson(post.mid(post.indexOf('{'))).object().value("text").toString();
        };

        // What the box starts with, list by list.
        QCOMPARE(backend.addPrefix(), QString("today "));
        backend.showView("upcoming");
        settle();
        QCOMPARE(backend.addPrefix(), QString());
        backend.showView("inbox");
        settle();
        QCOMPARE(backend.addPrefix(), QString());
        QCOMPARE(backend.addPrefix("s2"), QString("#Inbox /Later "));
        backend.showView("project:stuff");
        settle();
        QCOMPARE(backend.addPrefix(), QString("#My Stuff "));
        QCOMPARE(backend.addPrefix("s1"), QString("#My Stuff /Next week "));
        QCOMPARE(backend.addPrefix("elsewhere"), QString("#My Stuff "));

        // With the prefix, Zero files it there itself, spaces and all, and
        // quick add only sees the rest.
        posts();
        QSignalSpy added(&backend, &Backend::added);
        backend.add("#My Stuff Milk");
        QTRY_COMPARE(added.size(), 1);
        settle();
        QList<QByteArray> sent = posts();
        QCOMPARE(sent.size(), 2);
        QCOMPARE(quickText(sent.at(0)), QString("Milk"));
        QVERIFY(sent.at(1).startsWith("/tasks/new/move"));
        QVERIFY(sent.at(1).contains("\"project_id\":\"stuff\""));

        // And into a section.
        backend.add("#My Stuff /Next week Milk tomorrow");
        settle();
        sent = posts();
        QCOMPARE(quickText(sent.at(0)), QString("Milk tomorrow"));
        QVERIFY(sent.at(1).contains("\"section_id\":\"s1\""));

        // Cleared, it's a plain quick add: the Inbox.
        backend.add("Milk");
        settle();
        sent = posts();
        QCOMPARE(sent.size(), 1);
        QCOMPARE(quickText(sent.at(0)), QString("Milk"));

        // A #project typed after the prefix wins.
        backend.add("#My Stuff Milk #Work");
        settle();
        QCOMPARE(posts().size(), 1);

        // A "#" that names no project ("PR #42") still files it here.
        backend.add("#My Stuff Review PR #42");
        settle();
        sent = posts();
        QCOMPARE(sent.size(), 2);
        QVERIFY(sent.at(1).contains("\"project_id\":\"stuff\""));

        // The prefix alone adds nothing.
        backend.add("#My Stuff ");
        QTest::qWait(100);
        QVERIFY(posts().isEmpty());

        // Today's "today" is Todoist's to read, so it stays in the text, and
        // nothing more is sent.
        backend.showView("today");
        settle();
        posts();
        backend.add("today Milk");
        settle();
        sent = posts();
        QCOMPARE(sent.size(), 1);
        QCOMPARE(quickText(sent.at(0)), QString("today Milk"));
    }

    void backendAsksBeforeUndoingAnAdd() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/filter")
                return {200, page({task("a", "Water plants", iso(0))})};
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Milk")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {200, QJsonDocument(task("new", "Milk")).toJson()};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };

        // A completion undoes without asking.
        backend.complete("a");
        settle();
        QVERIFY(backend.undoDeletes().isEmpty());

        // An add says which task undoing it would delete.
        backend.add("Milk");
        settle();
        QCOMPARE(backend.undoDeletes().value("id").toString(), QString("new"));
        QCOMPARE(backend.undoDeletes().value("text").toString(), QString("Milk"));
    }

    void backendRedoesAnAddWhereItWent() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Milk")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {200, QJsonDocument(task("new", "Milk")).toJson()};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        auto sentSince = [&](const QByteArray &path, const QByteArray &body) {
            for (const auto &r : server.requests)
                if (r.path() == path && r.body.contains(body))
                    return true;
            return false;
        };

        // Into a project: redo moves the new copy there too.
        backend.showView("project:work");
        settle();
        backend.add("#Work Milk");
        settle();
        backend.undo();
        settle();
        QVERIFY(backend.canRedo());
        server.requests.clear();
        backend.redo();
        settle();
        QVERIFY(sentSince("/tasks/quick", "Milk"));
        QVERIFY(sentSince("/tasks/new/move", "\"project_id\":\"work\""));
        QVERIFY(backend.canUndo());

        // In Today: redo adds it with "today" again.
        backend.showView("today");
        settle();
        backend.add("today Milk");
        settle();
        backend.undo();
        settle();
        server.requests.clear();
        backend.redo();
        settle();
        QVERIFY(sentSince("/tasks/quick", "today Milk"));
    }

    void errorsKeepTheirStatus() {
        // No token is refused like a bad one, so it reads as auth, not network.
        Todoist client;
        ApiError error;
        client.close("x", [&](const QJsonObject &, const ApiError &e) { error = e; });
        QVERIFY(error.isAuth());
        QVERIFY(!error.isNetwork());
        QVERIFY(ApiError("Couldn't reach Todoist", 0).isNetwork());
        QVERIFY(!ApiError().isNetwork());
        QVERIFY(!ApiError("Todoist said 400: bad token format", 400).isAuth());
    }

    void backendMovesAndReschedulesWithUndo() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks")
                return {200, page({task("a", "Eggs", "2026-11-06")})};
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        backend.showView("inbox");
        QTRY_COMPARE(backend.tasks().size(), 1);
        QCOMPARE(backend.destinations().first().toMap().value("projectId").toString(), QString("inbox1"));

        server.requests.clear();
        backend.moveTask("a", "work");
        // It leaves the Inbox at once.
        QCOMPARE(backend.tasks().size(), 0);
        QTRY_VERIFY(backend.canUndo());
        QTRY_VERIFY(!backend.busy());
        QCOMPARE(server.requests.first().path(), QByteArray("/tasks/a/move"));

        server.requests.clear();
        backend.undo();
        QTRY_VERIFY(backend.canRedo());
        QVERIFY(server.requests.first().body.contains("inbox1"));

        QTRY_COMPARE(backend.tasks().size(), 1);
        server.requests.clear();
        backend.schedule("a", "tomorrow");
        QTRY_VERIFY(!server.requests.isEmpty());
        QVERIFY(server.requests.first().body.contains("tomorrow"));
        QTRY_VERIFY(backend.canUndo());
        QTRY_VERIFY(!backend.busy());
        server.requests.clear();
        backend.undo();
        QTRY_VERIFY(!server.requests.isEmpty());
        // Back to the wording Todoist had for it.
        QVERIFY(server.requests.first().body.contains("2026-11-06"));
    }

    void backendCreatesAndDeletesProjects() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "aisle"}, {"name", "Aisle 5"}, {"parent_id", "work"}, {"child_order", 2}};
        FakeServer server([&list](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.method == "POST" && request.path() == "/projects") {
                // Like Todoist, a created project is listed from then on.
                const QJsonObject shop{{"id", "shop"}, {"name", "Supermarket"}, {"color", "green"}, {"child_order", 9}};
                list << shop;
                return {200, QJsonDocument(shop).toJson()};
            }
            if (request.path() == "/projects")
                return {200, page(list)};
            if (request.path() == "/tasks") {
                if (request.target.contains("project_id=work"))
                    return {200, page({task("1", "a"), task("2", "b")})};
                if (request.target.contains("project_id=aisle"))
                    return {200, page({task("3", "c")})};
                return {200, page({})};
            }
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());

        // A new project opens straight away.
        QSignalSpy created(&backend, &Backend::projectCreated);
        backend.createProject("  Supermarket ");
        QTRY_COMPARE(created.size(), 1);
        QCOMPARE(backend.view(), QString("project:shop"));
        bool sentName = false;
        for (const auto &r : server.requests)
            sentName |= r.method == "POST" && r.body.contains("\"name\":\"Supermarket\"");
        QVERIFY(sentName);

        // Deleting counts what goes with it, sub-projects included.
        QSignalSpy contents(&backend, &Backend::projectContents);
        backend.inspectProject("work");
        QTRY_COMPARE(contents.size(), 1);
        QCOMPARE(contents.first().at(1).toInt(), 3);
        QCOMPARE(contents.first().at(2).toStringList(), QStringList({"Aisle 5"}));

        // Deleting the project on screen falls back to the Inbox.
        backend.showView("project:aisle");
        QSignalSpy deleted(&backend, &Backend::projectDeleted);
        backend.deleteProject("work");
        QCOMPARE(backend.view(), QString("inbox"));
        QTRY_COMPARE(deleted.size(), 1);
        bool sentDelete = false;
        for (const auto &r : server.requests)
            sentDelete |= r.method == "DELETE" && r.path() == "/projects/work";
        QVERIFY(sentDelete);

        // The Inbox can't be deleted.
        server.requests.clear();
        backend.deleteProject("inbox1");
        QTest::qWait(100);
        for (const auto &r : server.requests)
            QVERIFY(r.method != "DELETE");
    }

    void backendLoadsAViewPickedWhileStarting() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.target.contains("project_id=inbox1"))
                return {200, page({task("a", "In the inbox")})};
            return {200, page({})};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        // The Inbox is picked while the first list of projects is on its way.
        bool picked = false;
        connect(&server, &QTcpServer::newConnection, &backend, [&] {
            if (!picked) {
                picked = true;
                backend.showView("inbox");
            }
        });
        QTRY_VERIFY(backend.loaded());
        QCOMPARE(backend.view(), QString("inbox"));
        QCOMPARE(backend.tasks().size(), 1);
    }

    void backendDropsUndoStepsThatCantWork() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        int reopenStatus = 404;
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/filter")
                return {200, page({task("a", "Gone elsewhere", iso(0)), task("b", "Still here", iso(0))})};
            if (request.path().endsWith("/reopen"))
                return {reopenStatus, R"({"error":"x"})"};
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };

        backend.complete("b");
        settle();
        backend.complete("a");
        settle();

        // Todoist busy: the step stays, to try again.
        reopenStatus = 503;
        backend.undo();
        settle();
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(backend.canUndo());

        // Deleted elsewhere: the step goes, and the one under it still undoes.
        reopenStatus = 404;
        backend.undo();
        settle();
        QVERIFY(backend.error().contains("Gone elsewhere"));
        QVERIFY(backend.canUndo());
        reopenStatus = 200;
        backend.undo();
        settle();
        QVERIFY(!backend.canUndo());
        QVERIFY(backend.canRedo());
        bool reopenedB = false;
        for (const auto &r : server.requests)
            reopenedB |= r.path() == "/tasks/b/reopen";
        QVERIFY(reopenedB);
    }

    void backendForgetsAddsIntoADeletedProject() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Milk")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };

        // Quick add makes it in the Inbox, then it's moved into Work.
        backend.showView("project:work");
        settle();
        backend.add("#Work Milk");
        settle();
        QVERIFY(backend.canUndo());

        backend.deleteProject("work");
        QVERIFY(!backend.canUndo());
    }

    void backendColorsProjectsWithTodoistsNames() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        bool refuse = false;
        FakeServer server([&refuse](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.method == "POST" && request.path() == "/projects")
                return {200, R"({"id":"shop","name":"Supermarket","color":"lime_green"})"};
            if (request.method == "POST" && request.path().startsWith("/projects/"))
                return refuse ? FakeServer::Response{400, R"({"error":"nope"})"} : FakeServer::Response{200, "{}"};
            if (request.path() == "/projects")
                return {200, page(projects)};
            return {200, page({})};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());

        QCOMPARE(backend.colors().size(), 20);
        QCOMPARE(backend.colors().first().toMap().value("name").toString(), QString("berry_red"));

        QSignalSpy created(&backend, &Backend::projectCreated);
        backend.createProject("Supermarket", "lime_green");
        QTRY_COMPARE(created.size(), 1);
        bool sentColor = false;
        for (const auto &r : server.requests)
            sentColor |= r.path() == "/projects" && r.body.contains("\"color\":\"lime_green\"");
        QVERIFY(sentColor);

        auto colorOf = [&](const QString &id) {
            for (const auto &p : backend.projects())
                if (p.toMap().value("id") == id)
                    return p.toMap().value("colorName").toString();
            return QString();
        };
        QTRY_VERIFY(!backend.busy());
        server.requests.clear();
        backend.setProjectColor("work", "grape");
        QCOMPARE(colorOf("work"), QString("grape"));
        QTRY_VERIFY(!backend.busy());
        QCOMPARE(server.requests.first().path(), QByteArray("/projects/work"));
        QVERIFY(server.requests.first().body.contains("grape"));

        // Names Todoist doesn't know are never sent.
        server.requests.clear();
        backend.setProjectColor("work", "#ff0000");
        QTest::qWait(50);
        QVERIFY(server.requests.isEmpty());

        // A refusal puts the old color back and says so.
        refuse = true;
        backend.setProjectColor("work", "teal");
        QTRY_VERIFY(!backend.busy());
        QCOMPARE(colorOf("work"), QString("grape"));
        QVERIFY(!backend.error().isEmpty());
    }

    void backendReportsFailures() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("bad"), QString());
        FakeServer server([](const FakeServer::Request &) -> FakeServer::Response { return {401, {}}; });
        qputenv("ZERO_API_URL", server.url().toUtf8());

        Backend backend(credentials);
        QTRY_VERIFY(!backend.error().isEmpty());
        QVERIFY(backend.error().contains("token"));
    }

    void groupsTasksBySection() {
        const QList<Section> sections = Tasks::parseSections({
            QJsonObject{{"id", "later"}, {"name", "Later"}, {"project_id", "work"}, {"section_order", 2}},
            QJsonObject{{"id", "gone"}, {"name", "Gone"}, {"project_id", "work"}, {"is_archived", true}},
            QJsonObject{{"id", "now"}, {"name", "Now"}, {"project_id", "work"}, {"section_order", 1}},
        });
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.first().id, QString("now"));

        QJsonObject inLater = task("a", "a"), inNow = task("b", "b"), inGone = task("c", "c"), sub = task("d", "d", QJsonValue(), 1, "b");
        inLater.insert("section_id", "later");
        inNow.insert("section_id", "now");
        inGone.insert("section_id", "gone");
        sub.insert("section_id", "now");
        QHash<QString, int> depths;
        QStringList ids;
        for (const auto &t : Tasks::sortBySection(Tasks::parse({inLater, inNow, inGone, sub, task("e", "e")}), sections, &depths))
            ids << t.id;
        // Loose tasks (and one in an archived section) first, then by section.
        QCOMPARE(ids, QStringList({"c", "e", "b", "d", "a"}));
        QCOMPARE(depths.value("d"), 1);
    }

    void backendShowsAndEditsSections() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray sections{
            QJsonObject{{"id", "s1"}, {"name", "To do"}, {"project_id", "work"}, {"section_order", 1}},
            QJsonObject{{"id", "s2"}, {"name", "Empty"}, {"project_id", "work"}, {"section_order", 2}},
        };
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/sections" && request.method == "GET")
                return {200, page(sections)};
            if (request.path() == "/sections") {
                const QJsonObject made{{"id", "s3"}, {"name", "New"}, {"project_id", "work"}, {"section_order", 3}};
                sections << made;
                return {200, QJsonDocument(made).toJson()};
            }
            if (request.target.contains("project_id=work")) {
                QJsonObject a = task("a", "Loose"), b = task("b", "In to do");
                a.insert("project_id", "work");
                b.insert("project_id", "work");
                b.insert("section_id", "s1");
                return {200, page({a, b})};
            }
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Milk")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        backend.showView("project:work");
        QTRY_VERIFY(backend.loaded());
        auto keys = [&] {
            QStringList list;
            for (const auto &row : backend.rows())
                list << row.toMap().value("key").toString();
            return list;
        };
        // Empty sections show too, so they can be used and deleted.
        QCOMPARE(keys(), QStringList({"task:a", "section:s1", "task:b", "section:s2"}));
        QCOMPARE(backend.rows().at(1).toMap().value("count").toInt(), 1);

        // The move picker offers each section under its project.
        QStringList destinations;
        for (const auto &entry : backend.destinations())
            destinations << entry.toMap().value("key").toString();
        QCOMPARE(destinations, QStringList({"project:inbox1", "project:work", "section:s1", "section:s2"}));

        // A new task goes into the section it's added to.
        server.requests.clear();
        backend.add(backend.addPrefix("s2") + "Milk");
        settle();
        bool filed = false;
        for (const auto &r : server.requests)
            filed |= r.path() == "/tasks/new/move" && r.body.contains("\"section_id\":\"s2\"");
        QVERIFY(filed);

        // Moving between sections, and back out of one with undo.
        server.requests.clear();
        backend.moveTask("a", "work", "s1");
        settle();
        QVERIFY(server.requests.first().body.contains("\"section_id\":\"s1\""));
        backend.undo();
        settle();
        bool backOut = false;
        for (const auto &r : server.requests)
            backOut |= r.path() == "/tasks/a/move" && r.body.contains("\"project_id\":\"work\"");
        QVERIFY(backOut);

        // A section is made in the project on screen.
        QSignalSpy created(&backend, &Backend::sectionCreated);
        backend.createSection("  New ");
        QTRY_COMPARE(created.size(), 1);
        bool sent = false;
        for (const auto &r : server.requests)
            sent |= r.method == "POST" && r.path() == "/sections" && r.body.contains("\"name\":\"New\"") &&
                    r.body.contains("\"project_id\":\"work\"");
        QVERIFY(sent);
        QTRY_VERIFY(keys().contains("section:s3"));

        // Deleting one takes its tasks, and steps about them, at once.
        backend.complete("b");
        QTRY_VERIFY(backend.canUndo());
        backend.deleteSection("s1");
        QVERIFY(!keys().contains("section:s1"));
        QVERIFY(!keys().contains("task:b"));
        bool onlyAddLeft = true;
        // The add into s2 is still undoable; the completion of b isn't.
        backend.undo();
        settle();
        for (const auto &r : server.requests)
            onlyAddLeft &= r.path() != "/tasks/b/reopen";
        QVERIFY(onlyAddLeft);
        bool deleted = false;
        for (const auto &r : server.requests)
            deleted |= r.method == "DELETE" && r.path() == "/sections/s1";
        QVERIFY(deleted);
    }

    void backendDeletesTasks() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.target.contains("project_id=inbox1"))
                return {200, page({task("p", "Parent"), task("c", "Child", QJsonValue(), 1, "p"), task("o", "Other")})};
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        backend.showView("inbox");
        QTRY_COMPARE(backend.tasks().size(), 3);
        QCOMPARE(backend.subtaskCount("p"), 1);
        backend.schedule("c", "tomorrow");
        QTRY_VERIFY(backend.canUndo());

        QSignalSpy deleted(&backend, &Backend::taskDeleted);
        backend.deleteTask("p");
        // The subtask goes with it, and so does undoing its date change.
        QCOMPARE(backend.tasks().size(), 1);
        QVERIFY(!backend.canUndo());
        QTRY_COMPARE(deleted.size(), 1);
        bool sent = false;
        for (const auto &r : server.requests)
            sent |= r.method == "DELETE" && r.path() == "/tasks/p";
        QVERIFY(sent);
    }

    void backendNestsProjects() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "home"}, {"name", "Home"}, {"child_order", 2}}
             << QJsonObject{{"id", "sub"}, {"name", "Sub"}, {"parent_id", "work"}, {"child_order", 0}};
        bool refuse = false;
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(list)};
            if (request.path() == "/sync") {
                const QJsonObject command = QJsonDocument::fromJson(request.body).object().value("commands").toArray().first().toObject();
                const QString uuid = command.value("uuid").toString();
                if (refuse)
                    return {200, QJsonDocument(QJsonObject{{"sync_status", QJsonObject{{uuid, QJsonObject{
                        {"error", "Too many levels"}, {"http_code", 400}}}}}}).toJson()};
                return {200, QJsonDocument(QJsonObject{{"sync_status", QJsonObject{{uuid, "ok"}}}}).toJson()};
            }
            if (request.method == "GET")
                return {200, page({})};
            return {204, {}};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        auto syncs = [&] {
            QList<QJsonObject> found;
            for (const auto &r : server.requests)
                if (r.path() == "/sync")
                    found << QJsonDocument::fromJson(r.body).object().value("commands").toArray().first().toObject();
            server.requests.clear();
            return found;
        };

        // A project can't go under itself, its own sub-project, or the Inbox.
        QStringList choices;
        for (const auto &entry : backend.parentChoices("work"))
            choices << entry.toMap().value("key").toString();
        QCOMPARE(choices, QStringList({"top", "project:home"}));
        backend.moveProject("work", "sub");
        backend.moveProject("work", "inbox1");
        QTest::qWait(100);
        QVERIFY(syncs().isEmpty());

        QSignalSpy moved(&backend, &Backend::projectMoved);
        backend.moveProject("home", "work");
        QTRY_COMPARE(moved.size(), 1);
        settle();
        QList<QJsonObject> sent = syncs();
        QCOMPARE(sent.size(), 1);
        QCOMPARE(sent.first().value("type").toString(), QString("project_move"));
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"id", "home"}, {"parent_id", "work"}}));

        // Undo puts it back at the top level.
        backend.undo();
        settle();
        sent = syncs();
        QCOMPARE(sent.size(), 1);
        QVERIFY(sent.first().value("args").toObject().value("parent_id").isNull());

        // A command Todoist turns down is an error, not a success.
        refuse = true;
        backend.moveProject("home", "sub");
        settle();
        QVERIFY(backend.error().contains("Too many levels"));
        QCOMPARE(moved.size(), 1);
    }

    void backendEditsTasks() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        bool refuse = false;
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.target.contains("project_id=inbox1"))
                return {200, page({task("a", "Call mum"), task("b", "Other")})};
            if (request.method == "GET")
                return {200, page({})};
            if (refuse)
                return {400, R"({"error":"Nope"})"};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        backend.showView("inbox");
        QTRY_COMPARE(backend.tasks().size(), 2);
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        auto shown = [&] { return backend.taskById("a"); };

        // Changed at once, and sent as Todoist counts priority.
        server.requests.clear();
        backend.editTask("a", {{"content", " Call dad "}, {"description", "About the trip\n\n"}, {"priority", 1}});
        QCOMPARE(shown().value("content").toString(), QString("Call dad"));
        QCOMPARE(shown().value("priority").toInt(), 1);
        settle();
        QCOMPARE(server.requests.first().path(), QByteArray("/tasks/a"));
        QCOMPARE(body(server.requests.first()),
                 QJsonObject({{"content", "Call dad"}, {"description", "About the trip"}, {"priority", 4}}));

        // Undo sends the old fields back.
        server.requests.clear();
        backend.undo();
        QCOMPARE(shown().value("content").toString(), QString("Call mum"));
        settle();
        QCOMPARE(body(server.requests.first()), QJsonObject({{"content", "Call mum"}, {"description", ""}, {"priority", 1}}));
        QVERIFY(backend.canRedo());

        // Nothing changed, or an empty title: nothing sent.
        server.requests.clear();
        backend.editTask("a", {{"content", "  "}, {"priority", 4}, {"description", ""}});
        backend.setPriority("a", 9);
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());

        // Turned down: the old title comes back, with the reason.
        refuse = true;
        backend.setPriority("b", 2);
        backend.editTask("a", {{"content", "Lost"}});
        settle();
        QCOMPARE(shown().value("content").toString(), QString("Call mum"));
        QCOMPARE(backend.taskById("b").value("priority").toInt(), 4);
        QVERIFY(backend.error().contains("Nope"));
    }

    void backendNestsAndOrdersTasks() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/sync")
                return syncOk(request);
            if (request.target.contains("project_id=work")) {
                QJsonArray list{task("a", "A", QJsonValue(), 1, {}, 1), task("b", "B", QJsonValue(), 1, {}, 2),
                                task("c", "C", QJsonValue(), 1, {}, 3), task("c1", "C1", QJsonValue(), 1, "c", 1)};
                for (auto &&value : list) {
                    QJsonObject object = value.toObject();
                    object.insert("project_id", "work");
                    value = object;
                }
                return {200, page(list)};
            }
            if (request.path() == "/tasks/filter")
                return {200, page({task("t", "Today", iso(0)), task("u", "Also today", iso(0))})};
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };

        // The dated lists don't nest or reorder.
        settle();
        server.requests.clear();
        backend.indentTask("u");
        backend.moveTaskBy("t", 1);
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());

        backend.showView("project:work");
        QTRY_COMPARE(backend.tasks().size(), 4);
        settle();
        auto order = [&] {
            QStringList list;
            for (const auto &entry : backend.tasks()) {
                const QVariantMap map = entry.toMap();
                list << map.value("id").toString() + ":" + QString::number(map.value("depth").toInt());
            }
            return list;
        };
        auto moveBody = [&] {
            for (const auto &r : server.requests)
                if (r.path().endsWith("/move"))
                    return body(r);
            return QJsonObject();
        };

        // Indent: under the task above, last among its subtasks.
        server.requests.clear();
        backend.indentTask("b");
        QCOMPARE(order(), QStringList({"a:0", "b:1", "c:0", "c1:1"}));
        settle();
        QCOMPARE(moveBody(), QJsonObject({{"parent_id", "a"}}));
        QList<QJsonObject> sent = takeCommands(server);
        QCOMPARE(sent.size(), 1);
        QCOMPARE(sent.first().value("type").toString(), QString("item_reorder"));
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"items", orders({{"b", 1}})}}));

        // Undo takes it back out, to where it was.
        backend.undo();
        settle();
        QCOMPARE(moveBody(), QJsonObject({{"project_id", "work"}}));
        sent = takeCommands(server);
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"items", orders({{"a", 1}, {"b", 2}, {"c", 3}})}}));

        // The first task has nothing to go under.
        backend.indentTask("a");
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());

        // Outdent: just after its old parent.
        backend.outdentTask("c1");
        QCOMPARE(order(), QStringList({"a:0", "b:0", "c:0", "c1:0"}));
        settle();
        QCOMPARE(moveBody(), QJsonObject({{"project_id", "work"}}));
        sent = takeCommands(server);
        QCOMPARE(sent.first().value("args").toObject(),
                 QJsonObject({{"items", orders({{"a", 1}, {"b", 2}, {"c", 3}, {"c1", 4}})}}));
        backend.undo();
        settle();
        QCOMPARE(moveBody(), QJsonObject({{"parent_id", "c"}}));
        server.requests.clear();

        // Moving down a place, past a task and its subtasks.
        backend.moveTaskBy("b", 1);
        QCOMPARE(order(), QStringList({"a:0", "c:0", "c1:1", "b:0"}));
        settle();
        sent = takeCommands(server);
        QCOMPARE(sent.size(), 1);
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"items", orders({{"a", 1}, {"c", 2}, {"b", 3}})}}));
        backend.undo();
        QCOMPARE(order(), QStringList({"a:0", "b:0", "c:0", "c1:1"}));
        settle();
        sent = takeCommands(server);
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"items", orders({{"a", 1}, {"b", 2}, {"c", 3}})}}));

        // Not past the ends of its level.
        backend.moveTaskBy("a", -1);
        backend.moveTaskBy("c1", 1);
        QTest::qWait(100);
        QVERIFY(takeCommands(server).isEmpty());
    }

    void backendRenamesAndReordersSectionsAndProjects() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "home"}, {"name", "Home"}, {"child_order", 2}};
        const QJsonArray sections{
            QJsonObject{{"id", "s1"}, {"name", "To do"}, {"project_id", "work"}, {"section_order", 1}},
            QJsonObject{{"id", "s2"}, {"name", "Done"}, {"project_id", "work"}, {"section_order", 2}},
        };
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects" && request.method == "GET")
                return {200, page(list)};
            if (request.path() == "/sections" && request.method == "GET")
                return {200, page(sections)};
            if (request.path() == "/sync")
                return syncOk(request);
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        backend.showView("project:work");
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        settle();
        auto headings = [&] {
            QStringList names;
            for (const auto &row : backend.rows())
                names << row.toMap().value("name").toString();
            return names;
        };
        auto sidebar = [&] {
            QStringList names;
            for (const auto &entry : backend.projects())
                names << entry.toMap().value("name").toString();
            return names;
        };
        QCOMPARE(headings(), QStringList({"To do", "Done"}));

        // A section's new name shows at once, and undo puts the old one back.
        server.requests.clear();
        backend.renameSection("s1", " Doing ");
        QCOMPARE(headings(), QStringList({"Doing", "Done"}));
        settle();
        QCOMPARE(server.requests.first().path(), QByteArray("/sections/s1"));
        QCOMPARE(body(server.requests.first()), QJsonObject({{"name", "Doing"}}));
        backend.undo();
        QCOMPARE(headings(), QStringList({"To do", "Done"}));
        settle();

        // So does a project's, in the title too; the Inbox keeps its name.
        server.requests.clear();
        backend.renameProject("work", "Job");
        QCOMPARE(backend.viewName(), QString("Job"));
        QCOMPARE(sidebar(), QStringList({"Job", "Home"}));
        settle();
        QCOMPARE(server.requests.first().path(), QByteArray("/projects/work"));
        QCOMPARE(body(server.requests.first()), QJsonObject({{"name", "Job"}}));
        server.requests.clear();
        backend.renameProject("inbox1", "Mine");
        backend.renameProject("work", "Job");
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());

        // Sections reorder within their project.
        backend.moveSectionBy("s1", 1);
        QCOMPARE(headings(), QStringList({"Done", "To do"}));
        settle();
        QList<QJsonObject> sent = takeCommands(server);
        QCOMPARE(sent.size(), 1);
        QCOMPARE(sent.first().value("type").toString(), QString("section_reorder"));
        QCOMPARE(sent.first().value("args").toObject(),
                 QJsonObject({{"sections", orders({{"s2", 1}, {"s1", 2}}, "section_order")}}));
        backend.moveSectionBy("s2", -1);
        QTest::qWait(100);
        QVERIFY(takeCommands(server).isEmpty());

        // Projects among their siblings, with undo.
        backend.moveProjectBy("work", 1);
        QCOMPARE(sidebar(), QStringList({"Home", "Job"}));
        settle();
        sent = takeCommands(server);
        QCOMPARE(sent.first().value("type").toString(), QString("project_reorder"));
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"projects", orders({{"home", 1}, {"work", 2}})}}));
        backend.undo();
        QCOMPARE(sidebar(), QStringList({"Job", "Home"}));
        settle();
        sent = takeCommands(server);
        QCOMPARE(sent.first().value("args").toObject(), QJsonObject({{"projects", orders({{"work", 1}, {"home", 2}})}}));
    }

    void backendAddsAndLoadsSubtasks() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/filter")
                return {200, page({task("p", "Plan trip", iso(0))})};
            if (request.target.contains("parent_id=p"))
                return {200, page({task("c", "Book flights", QJsonValue(), 1, "p", 2),
                                   task("d", "Pack", QJsonValue(), 1, "p", 1)})};
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Find a sitter")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        settle();

        // A subtask goes under its parent, even from Today.
        server.requests.clear();
        backend.add("Find a sitter", "p");
        settle();
        bool under = false;
        for (const auto &r : server.requests)
            under |= r.path() == "/tasks/new/move" && body(r) == QJsonObject({{"parent_id", "p"}});
        QVERIFY(under);

        // Today doesn't list them all, so they're fetched, in Todoist's order.
        QSignalSpy changed(&backend, &Backend::subtasksChanged);
        backend.loadSubtasks("p");
        QTRY_VERIFY(changed.size() > 0);
        QStringList ids;
        for (const auto &entry : backend.subtasksOf("p"))
            ids << entry.toMap().value("id").toString();
        QCOMPARE(ids, QStringList({"d", "c"}));
        QCOMPARE(backend.taskById("c").value("parentId").toString(), QString("p"));

        // They can be completed and edited like any task.
        backend.complete("d");
        QCOMPARE(backend.subtasksOf("p").size(), 1);
        settle();
        server.requests.clear();
        backend.editTask("c", {{"content", "Book trains"}});
        QCOMPARE(backend.subtasksOf("p").first().toMap().value("content").toString(), QString("Book trains"));
        settle();
        QCOMPARE(server.requests.first().path(), QByteArray("/tasks/c"));

        backend.forgetSubtasks();
        QVERIFY(backend.taskById("c").isEmpty());
        QVERIFY(backend.subtasksOf("p").isEmpty());
    }

    void backendDropsDraggedRows() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        QJsonArray list = projects;
        list << QJsonObject{{"id", "home"}, {"name", "Home"}, {"child_order", 2}}
             << QJsonObject{{"id", "sub"}, {"name", "Sub"}, {"parent_id", "work"}, {"child_order", 1}};
        const QJsonArray sections{
            QJsonObject{{"id", "s1"}, {"name", "Later"}, {"project_id", "work"}, {"section_order", 1}},
            QJsonObject{{"id", "s2"}, {"name", "Empty"}, {"project_id", "work"}, {"section_order", 2}},
        };
        FakeServer server([&](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(list)};
            if (request.path() == "/sections" && request.method == "GET")
                return {200, page(sections)};
            if (request.path() == "/sync")
                return syncOk(request);
            if (request.target.contains("project_id=work")) {
                QJsonArray found{task("a", "A", QJsonValue(), 1, {}, 1), task("b", "B", QJsonValue(), 1, {}, 2),
                                 task("c", "C", QJsonValue(), 1, {}, 3), task("c1", "C1", QJsonValue(), 1, "c", 1),
                                 task("d", "D", QJsonValue(), 1, {}, 1)};
                for (auto &&value : found) {
                    QJsonObject object = value.toObject();
                    object.insert("project_id", "work");
                    if (object.value("id") == "d")
                        object.insert("section_id", "s1");
                    value = object;
                }
                return {200, page(found)};
            }
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        backend.showView("project:work");
        QTRY_COMPARE(backend.tasks().size(), 5);
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        settle();
        server.requests.clear();
        // Rows: a, b, c, c1 (under c), Later, d, Empty.
        auto shown = [&] {
            QStringList keys;
            for (const auto &entry : backend.rows()) {
                const QVariantMap row = entry.toMap();
                keys << (row.value("kind") == "section" ? "#" + row.value("id").toString()
                                                        : row.value("id").toString() + ":" + row.value("depth").toString());
            }
            return keys;
        };
        QCOMPARE(shown(), QStringList({"a:0", "b:0", "c:0", "c1:1", "#s1", "d:0", "#s2"}));
        auto moved = [&] {
            for (const auto &r : server.requests)
                if (r.path().endsWith("/move"))
                    return body(r);
            return QJsonObject();
        };
        auto reorder = [&] {
            const QList<QJsonObject> sent = takeCommands(server);
            return sent.isEmpty() ? QJsonObject() : sent.last().value("args").toObject();
        };

        // In front of a subtask: a subtask too, of the same parent.
        backend.dropTask("a", 3);
        QCOMPARE(shown(), QStringList({"b:0", "c:0", "a:1", "c1:1", "#s1", "d:0", "#s2"}));
        settle();
        QCOMPARE(moved(), QJsonObject({{"parent_id", "c"}}));
        QCOMPARE(reorder(), QJsonObject({{"items", orders({{"a", 1}, {"c1", 2}})}}));
        // Undo puts it back where it was.
        backend.undo();
        settle();
        QCOMPARE(moved(), QJsonObject({{"project_id", "work"}}));
        QCOMPARE(reorder(), QJsonObject({{"items", orders({{"a", 1}, {"b", 2}, {"c", 3}})}}));

        // To the top, with its subtask: just a reorder.
        backend.dropTask("c", 0);
        QCOMPARE(shown(), QStringList({"c:0", "c1:1", "a:0", "b:0", "#s1", "d:0", "#s2"}));
        settle();
        QCOMPARE(moved(), QJsonObject());
        QCOMPARE(reorder(), QJsonObject({{"items", orders({{"c", 1}, {"a", 2}, {"b", 3}})}}));
        backend.undo();
        settle();
        server.requests.clear();

        // Into a section, in front of its task.
        backend.dropTask("b", 5);
        settle();
        QCOMPARE(moved(), QJsonObject({{"section_id", "s1"}}));
        QCOMPARE(reorder(), QJsonObject({{"items", orders({{"b", 1}, {"d", 2}})}}));
        backend.undo();
        settle();
        server.requests.clear();

        // Just above a heading: last in the section above, at the top level.
        backend.dropTask("a", 4);
        settle();
        QCOMPARE(moved(), QJsonObject());
        QCOMPARE(reorder(), QJsonObject({{"items", orders({{"b", 1}, {"c", 2}, {"a", 3}})}}));
        backend.undo();
        settle();
        server.requests.clear();

        // Under an empty section's heading: into it.
        backend.dropTask("b", 7);
        settle();
        QCOMPARE(moved(), QJsonObject({{"section_id", "s2"}}));
        backend.undo();
        settle();
        server.requests.clear();

        // Onto itself, or among its own subtasks: nothing.
        backend.dropTask("c", 3);
        backend.dropTask("c", 2);
        backend.dropTask("b", 1);
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());

        // A section goes among the headings above where it's dropped.
        backend.dropSection("s2", 4);
        QCOMPARE(shown().mid(4), QStringList({"#s2", "#s1", "d:0"}));
        settle();
        QCOMPARE(reorder(), QJsonObject({{"sections", orders({{"s2", 1}, {"s1", 2}}, "section_order")}}));
        backend.undo();
        settle();
        server.requests.clear();
        // In front of the next heading is where it already is; the end isn't.
        backend.dropSection("s1", 6);
        QTest::qWait(100);
        QVERIFY(takeCommands(server).isEmpty());
        backend.dropSection("s1", 7);
        settle();
        QCOMPARE(reorder(), QJsonObject({{"sections", orders({{"s2", 1}, {"s1", 2}}, "section_order")}}));

        // A project, among its siblings: the sidebar is work, sub, home.
        backend.dropProject("home", 0);
        settle();
        QCOMPARE(reorder(), QJsonObject({{"projects", orders({{"home", 1}, {"work", 2}})}}));
        backend.undo();
        settle();
        server.requests.clear();
        backend.dropProject("work", 2);
        backend.dropProject("sub", 3);
        QTest::qWait(100);
        QVERIFY(takeCommands(server).isEmpty());
        backend.dropProject("work", 3);
        settle();
        QCOMPARE(reorder(), QJsonObject({{"projects", orders({{"home", 1}, {"work", 2}})}}));
    }

    void backendReviewFixes() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/sync")
                return syncOk(request);
            if (request.path() == "/tasks/filter") {
                QJsonObject p = task("p", "Plan trip", iso(0));
                p.insert("description", "Notes from the web\n");
                return {200, page({p})};
            }
            if (request.target.contains("parent_id=p"))
                return {200, page({task("c", "Book flights", QJsonValue(), 1, "p", 1),
                                   task("d", "Pack", QJsonValue(), 1, "p", 2)})};
            if (request.target.contains("parent_id="))
                return {200, page({})};
            if (request.target.contains("project_id=inbox1"))
                return {200, page({task("a", "A", QJsonValue(), 1, {}, 1), task("b", "B", QJsonValue(), 1, {}, 2),
                                   task("x", "X", QJsonValue(), 1, {}, 3)})};
            if (request.path() == "/tasks/quick")
                return {200, QJsonDocument(task("new", "Buy screws")).toJson()};
            if (request.method == "GET")
                return {200, page({})};
            return {200, "{}"};
        });
        qputenv("ZERO_API_URL", server.url().toUtf8());
        Backend backend(credentials);
        QTRY_VERIFY(backend.loaded());
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };
        settle();

        // Opening and closing a task whose description ends in a newline,
        // as Todoist's other apps leave them, changes nothing.
        server.requests.clear();
        backend.editTask("p", {{"content", "Plan trip"}, {"description", "Notes from the web"}});
        QTest::qWait(100);
        QVERIFY(server.requests.isEmpty());
        QVERIFY(!backend.canUndo());

        // Subtasks the details loaded count when deleting from a dated list.
        QCOMPARE(backend.subtaskCount("p"), 0);
        backend.loadSubtasks("p");
        QTRY_COMPARE(backend.subtasksOf("p").size(), 2);
        QCOMPARE(backend.subtaskCount("p"), 2);

        // A subtask added while Today is on screen isn't dated today.
        server.requests.clear();
        backend.add("Buy screws", "p");
        settle();
        for (const auto &r : server.requests)
            QVERIFY(!r.body.contains("due_string"));

        // A subtask opened from the details (whose copy is its parent's)
        // reloads with its parent's subtasks after a change.
        backend.loadSubtasks("c");
        settle();
        server.requests.clear();
        backend.schedule("c", "tomorrow");
        settle();
        bool parentReloaded = false;
        for (const auto &r : server.requests)
            parentReloaded |= r.target.contains("parent_id=p");
        QVERIFY(parentReloaded);
        backend.forgetSubtasks();

        // Deleting a task drops the reorders that put it in order, which
        // would fail once it's gone; others stay.
        backend.showView("inbox");
        QTRY_COMPARE(backend.tasks().size(), 3);
        settle();
        while (backend.canUndo()) {
            backend.undo();
            settle();
        }
        backend.moveTaskBy("a", 1);
        settle();
        QVERIFY(backend.canUndo());
        backend.deleteTask("b");
        QVERIFY(!backend.canUndo());
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeReadsThePalette() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#7daea3\"\nbackground = \"#282828\"\nred = \"#ea6962\"\nmuted = 'nonsense'\n");
        Theme theme(dir.path());
        QVERIFY(theme.light());
        QCOMPARE(theme.background(), QColor("#282828"));
        QCOMPARE(theme.red(), QColor("#ea6962"));
        // Missing or broken names fall back to the built-in dark look.
        QCOMPARE(theme.foreground(), QColor("#ececef"));
        QCOMPARE(theme.muted(), QColor("#6e6e74"));
        QVERIFY(qAbs(theme.tint(0.5).alphaF() - 0.5) < 0.01);

        // A theme switch rewrites the file; the palette follows.
        write(dir.path() + "/theme/colors.toml", "background = \"#ffffff\"\n");
        QTRY_COMPARE(theme.background(), QColor("#ffffff"));
        QVERIFY(!theme.light());
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void shippedCatalogsParse() {
        // Every catalog compiled into the app must be valid JSON, or the
        // language silently falls back to English.
        const QDir catalogs(QFINDTESTDATA("../src/i18n"));
        QVERIFY(catalogs.exists());
        for (const auto &name : catalogs.entryList({"*.json"})) {
            QFile file(catalogs.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name));
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(ZeroTests)
#include "zero_tests.moc"
