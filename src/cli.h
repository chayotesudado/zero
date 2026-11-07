#pragma once

#include <QStringList>

namespace Cli {

// True when the arguments name a command that runs without a window.
bool handles(const QStringList &arguments);

// Runs that command against a QCoreApplication and returns the exit code.
int run(const QStringList &arguments);

} // namespace Cli
