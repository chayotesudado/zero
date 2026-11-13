#pragma once

#include <QString>

// The Todoist API token, kept in $XDG_CONFIG_HOME/zero/token (0600) so the
// app, its command line, and the bar plugin all sign in the same way.
// TODOIST_API_TOKEN overrides the file, for agents and scripts.
class Credentials {
public:
    explicit Credentials(const QString &path = {});

    QString path() const { return m_path; }
    QString read() const;
    bool fromEnvironment() const;
    // Returns an error message, or an empty string on success.
    QString write(const QString &token) const;
    QString clear() const;

    static QString defaultPath();

private:
    QString m_path;
};
