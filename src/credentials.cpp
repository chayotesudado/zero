#include "credentials.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

Credentials::Credentials(const QString &path) : m_path(path.isEmpty() ? defaultPath() : path) {}

QString Credentials::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/zero/token";
}

bool Credentials::fromEnvironment() const {
    return !qEnvironmentVariableIsEmpty("TODOIST_API_TOKEN");
}

QString Credentials::read() const {
    if (fromEnvironment())
        return qEnvironmentVariable("TODOIST_API_TOKEN").trimmed();
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString Credentials::write(const QString &token) const {
    const QString directory = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(directory))
        return QCoreApplication::translate("Credentials", "Couldn't create %1.").arg(directory);
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    // Written beside the target and renamed into place, readable only by the
    // user, since the token gives full access to their Todoist account.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return QCoreApplication::translate("Credentials", "Couldn't write %1: %2").arg(m_path, file.errorString());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(token.trimmed().toUtf8() + "\n");
    if (!file.commit())
        return QCoreApplication::translate("Credentials", "Couldn't write %1: %2").arg(m_path, file.errorString());
    return {};
}

QString Credentials::clear() const {
    if (QFile::exists(m_path) && !QFile::remove(m_path))
        return QCoreApplication::translate("Credentials", "Couldn't remove %1.").arg(m_path);
    return {};
}
