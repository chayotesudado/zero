#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "credentials.h"
#include "fakeserver.h"
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

} // namespace

class ZeroTests : public QObject {
    Q_OBJECT

private slots:
    void init() {
        qunsetenv("TODOIST_API_TOKEN");
        qunsetenv("ZERO_API_URL");
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
        QString error;
        bool done = false;
        client.tasksMatching("today | overdue", [&](const QJsonArray &r, const QString &e) { results = r; error = e; done = true; });
        QTRY_VERIFY(done);
        QCOMPARE(error, QString());
        QCOMPARE(results.size(), 2);
        QVERIFY(server.requests.first().target.startsWith("/tasks/filter?query=today%20%7C%20overdue"));

        client.setToken("bad");
        done = false;
        client.projects([&](const QJsonArray &, const QString &e) { error = e; done = true; });
        QTRY_VERIFY(done);
        QVERIFY(error.contains("token"));
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
        QString error;
        client.projects([&](const QJsonArray &, const QString &e) { error = e; done = true; });
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
        client.projects([&](const QJsonArray &, const QString &) { done = true; });
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

    void backendFilesAddsIntoTheListOnScreen() {
        QTemporaryDir dir;
        Credentials credentials(dir.path() + "/token");
        QCOMPARE(credentials.write("good"), QString());
        FakeServer server([](const FakeServer::Request &request) -> FakeServer::Response {
            if (request.path() == "/projects")
                return {200, page(projects)};
            if (request.path() == "/tasks/quick") {
                QJsonObject made = task("new", "Milk");
                if (request.body.contains("#Work"))
                    made.insert("project_id", "work");
                if (request.body.contains("friday"))
                    made.insert("due", QJsonObject{{"date", iso(3)}, {"string", "friday"}});
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
            QList<QByteArray> list;
            for (const auto &r : server.requests)
                if (r.method != "GET")
                    list << r.path() + " " + r.body;
            server.requests.clear();
            return list;
        };
        auto settle = [&] { QTRY_VERIFY(!backend.busy()); };

        // A project view moves the new task into its project.
        backend.showView("project:work");
        settle();
        posts();
        QSignalSpy added(&backend, &Backend::added);
        backend.add("Milk");
        QTRY_COMPARE(added.size(), 1);
        settle();
        QList<QByteArray> sent = posts();
        QCOMPARE(sent.size(), 2);
        QVERIFY(sent.at(1).startsWith("/tasks/new/move"));
        QVERIFY(sent.at(1).contains("\"project_id\":\"work\""));

        // Unless the text names its own project.
        backend.showView("project:inbox1");
        settle();
        posts();
        backend.add("Milk #Work");
        settle();
        QCOMPARE(posts().size(), 1);

        // Today dates an undated task today, but keeps a typed date.
        backend.showView("today");
        settle();
        posts();
        backend.add("Milk");
        settle();
        sent = posts();
        QCOMPARE(sent.size(), 2);
        QVERIFY(sent.at(1).startsWith("/tasks/new {"));
        QVERIFY(sent.at(1).contains("today"));
        backend.add("Milk friday");
        settle();
        QCOMPARE(posts().size(), 1);
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
        QCOMPARE(backend.destinations().first().toMap().value("id").toString(), QString("inbox1"));

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
        backend.add("Milk");
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
