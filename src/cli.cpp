// Commands that need no display, for the bar plugin, scripts, and agents.
// Each takes --json, exits 0 or 1, and writes errors to stderr.

#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio> // IWYU pragma: keep (stdin, stdout, stderr)

#include "backend.h"
#include "credentials.h"
#include "tasks.h"
#include "todoist.h"

namespace {

const QStringList commands{"today", "list", "add", "done", "login", "logout", "status", "skill", "help", "--help", "-h", "--version"};

const char *skillText = R"(---
name: zero
description: Read and change the user's Todoist tasks from the command line with `zero` — what's due today, adding to the Inbox, completing tasks. Use when asked about their todos, tasks, Todoist, or what's due.
---

# zero

`zero` is the user's Todoist app on Omarchy. Every command below takes
`--json`, exits 0 on success and 1 on failure, and prints errors on stderr.
Run `zero status --json` first; if it says signed out, ask the user to open
Zero and paste their API token (never ask for the token in chat).

- `zero today --json` — overdue and due-today tasks.
- `zero list --filter "<todoist filter>" --json` — any Todoist filter
  (`#Work`, `p1`, `7 days`, `@errand & today`).
- `zero add "<text>" --json` — Todoist quick add: dates, `#Project`,
  `@label`, `p1`–`p4` are parsed from the text. No `#Project` means Inbox.
- `zero done <task id> --json` — completes a task (recurring tasks move to
  their next date).

Confirm with the user before completing tasks they didn't name.
)";

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

QString tr(const char *text) { return QCoreApplication::translate("Cli", text); }

int failure(bool json, const QString &code, const QString &message) {
    err() << "zero: " << message << Qt::endl;
    // The plugin reads stdout, so --json also says what failed there.
    if (json)
        out() << QJsonDocument(QJsonObject{{"ok", false}, {"error", code}, {"message", message}}).toJson(QJsonDocument::Compact)
              << Qt::endl;
    return 1;
}

void printJson(const QJsonObject &object) {
    out() << QJsonDocument(object).toJson(QJsonDocument::Compact) << Qt::endl;
}

// Runs a callback-style client call to completion.
template <typename Call>
void wait(Call call) {
    QEventLoop loop;
    bool finished = false;
    call([&] {
        finished = true;
        loop.quit();
    });
    if (!finished)
        loop.exec();
}

struct Listing {
    QList<Task> tasks;
    QHash<QString, Project> projects;
    ApiError error;
};

Listing fetch(Todoist &client, const QString &filter) {
    Listing listing;
    QJsonArray tasks, projects;
    ApiError taskError, projectError;
    int pending = 2;
    QEventLoop loop;
    client.tasksMatching(filter, [&](const QJsonArray &results, const ApiError &error) {
        tasks = results;
        taskError = error;
        if (--pending == 0)
            loop.quit();
    });
    client.projects([&](const QJsonArray &results, const ApiError &error) {
        projects = results;
        projectError = error;
        if (--pending == 0)
            loop.quit();
    });
    if (pending > 0)
        loop.exec();
    listing.error = !taskError.isEmpty() ? taskError : projectError;
    listing.tasks = Tasks::parse(tasks);
    Tasks::sortByDue(listing.tasks);
    for (const auto &project : Tasks::parseProjects(projects))
        listing.projects.insert(project.id, project);
    return listing;
}

// What the bar plugin and scripts branch on, from the HTTP status, never the
// (translatable) message.
QString errorCode(const ApiError &error) {
    if (error.isAuth())
        return "auth";
    if (error.isNetwork())
        return "network";
    return "api";
}

int failure(bool json, const ApiError &error) { return failure(json, errorCode(error), error.message); }

// Text from Todoist, for a terminal: control characters (escape sequences a
// task's name could carry, from a shared project say) and the bidi controls
// that reorder what's shown become spaces. Emoji joiners stay.
QString printable(QString text) {
    for (QChar &c : text) {
        const char16_t u = c.unicode();
        if (c.category() == QChar::Other_Control || (u >= 0x202A && u <= 0x202E) || (u >= 0x2066 && u <= 0x2069))
            c = ' ';
    }
    return text;
}

int printListing(const Listing &listing, bool json) {
    if (!listing.error.isEmpty())
        return failure(json, listing.error);
    const QDateTime now = QDateTime::currentDateTime();
    if (json) {
        QJsonArray tasks;
        int overdue = 0;
        for (const auto &task : listing.tasks) {
            tasks << Tasks::toJson(task, now, listing.projects);
            overdue += task.isOverdue(now) ? 1 : 0;
        }
        printJson({{"ok", true}, {"count", tasks.size()}, {"overdue", overdue},
                   {"today", now.date().toString(Qt::ISODate)}, {"tasks", tasks}});
        return 0;
    }
    if (listing.tasks.isEmpty())
        out() << tr("Nothing due.") << Qt::endl;
    for (const auto &task : listing.tasks) {
        QString when = task.hasDue() ? task.dueDate.toString(Qt::ISODate) : QString();
        if (task.hasTime())
            when += " " + task.dueTime.toString("HH:mm");
        const Project project = listing.projects.value(task.projectId);
        out() << (task.isOverdue(now) ? "! " : "  ") << printable(task.content);
        if (!when.isEmpty())
            out() << "  [" << when << "]";
        if (!project.name.isEmpty() && !project.inbox)
            out() << "  #" << printable(project.name);
        out() << "  (" << printable(task.id) << ")" << Qt::endl;
    }
    return 0;
}

QString installSkill() {
    const QString directory = QDir::homePath() + "/.claude/skills/zero";
    if (!QDir().mkpath(directory))
        return tr("Couldn't create %1.").arg(directory);
    QSaveFile file(directory + "/SKILL.md");
    if (!file.open(QIODevice::WriteOnly) || file.write(skillText) < 0 || !file.commit())
        return tr("Couldn't write %1.").arg(file.fileName());
    return {};
}

void usage() {
    out() << "Usage: zero [open]                      open the app\n"
             "       zero today [--json]              overdue and due-today tasks\n"
             "       zero list --filter <query> [--json]\n"
             "       zero add <text> [--json]         quick add (Inbox unless #Project)\n"
             "       zero done <task id> [--json]     complete a task\n"
             "       zero status [--json]             signed in or not\n"
             "       zero login                       read an API token from stdin\n"
             "       zero logout\n"
             "       zero skill install               install the agent skill\n";
    out().flush();
}

} // namespace

namespace Cli {

bool handles(const QStringList &arguments) {
    return arguments.size() > 1 && commands.contains(arguments.at(1));
}

int run(const QStringList &all) {
    QStringList arguments = all.mid(1);
    const QString command = arguments.takeFirst();
    const bool json = arguments.removeAll("--json") > 0;

    if (command == "help" || command == "--help" || command == "-h") {
        usage();
        return 0;
    }
    if (command == "--version") {
        out() << "zero " << QCoreApplication::applicationVersion() << Qt::endl;
        return 0;
    }

    Credentials credentials;
    if (command == "skill") {
        if (arguments.value(0) != "install")
            return failure(json, "usage", tr("Usage: zero skill install"));
        const QString error = installSkill();
        if (!error.isEmpty())
            return failure(json, "io", error);
        if (json)
            printJson({{"ok", true}, {"path", QDir::homePath() + "/.claude/skills/zero/SKILL.md"}});
        else
            out() << tr("Installed ~/.claude/skills/zero/SKILL.md") << Qt::endl;
        return 0;
    }
    if (command == "logout") {
        const QString error = credentials.clear();
        if (!error.isEmpty())
            return failure(json, "io", error);
        if (json)
            printJson({{"ok", true}});
        return 0;
    }

    Todoist client;
    if (command == "login") {
        // From stdin, so the token stays out of the shell history and ps.
        QFile input;
        if (!input.open(stdin, QIODevice::ReadOnly))
            return failure(json, "io", tr("Couldn't read stdin."));
        const QString pasted = QString::fromUtf8(input.readAll()).trimmed();
        if (pasted.isEmpty())
            return failure(json, "usage", tr("Pipe the API token in: zero login < token.txt"));
        client.setToken(pasted);
        ApiError error;
        wait([&](auto finish) { client.projects([&, finish](const QJsonArray &, const ApiError &e) { error = e; finish(); }); });
        if (!error.isEmpty())
            return failure(json, error);
        const QString saveError = credentials.write(pasted);
        if (!saveError.isEmpty())
            return failure(json, "io", saveError);
        if (json)
            printJson({{"ok", true}});
        else
            out() << tr("Signed in.") << Qt::endl;
        return 0;
    }

    client.setToken(credentials.read());
    if (command == "status") {
        const bool signedIn = !client.token().isEmpty();
        if (json)
            printJson({{"ok", true}, {"signedIn", signedIn}, {"tokenPath", credentials.path()}});
        else
            out() << (signedIn ? tr("Signed in.") : tr("Signed out. Open Zero to connect Todoist.")) << Qt::endl;
        return 0;
    }
    if (client.token().isEmpty())
        return failure(json, "auth", tr("Not signed in. Open Zero and paste your Todoist API token."));

    if (command == "today")
        return printListing(fetch(client, Backend::todayQuery()), json);

    if (command == "list") {
        const int at = arguments.indexOf("--filter");
        const QString filter = at >= 0 ? arguments.value(at + 1) : QString();
        if (filter.isEmpty())
            return failure(json, "usage", tr("Usage: zero list --filter <query>"));
        return printListing(fetch(client, filter), json);
    }

    if (command == "add") {
        const QString text = arguments.join(' ').trimmed();
        if (text.isEmpty())
            return failure(json, "usage", tr("Usage: zero add <text>"));
        QJsonObject result;
        ApiError error;
        wait([&](auto finish) {
            client.quickAdd(text, [&, finish](const QJsonObject &r, const ApiError &e) { result = r; error = e; finish(); });
        });
        if (!error.isEmpty())
            return failure(json, error);
        const Task task = Task::fromJson(result);
        if (json)
            printJson({{"ok", true}, {"task", Tasks::toJson(task, QDateTime::currentDateTime(), {})}});
        else
            out() << tr("Added “%1”.").arg(printable(task.content)) << Qt::endl;
        return 0;
    }

    if (command == "done") {
        const QString id = arguments.value(0);
        if (id.isEmpty())
            return failure(json, "usage", tr("Usage: zero done <task id>"));
        ApiError error;
        wait([&](auto finish) { client.close(id, [&, finish](const QJsonObject &, const ApiError &e) { error = e; finish(); }); });
        if (!error.isEmpty())
            return failure(json, error);
        if (json)
            printJson({{"ok", true}, {"id", id}});
        return 0;
    }

    usage();
    return 1;
}

} // namespace Cli
