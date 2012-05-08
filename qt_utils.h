#pragma once

#include <QString>

int parseInt(const QString &str, bool allow_empty = false);
float parseFloat(const QString &str, bool allow_empty = false);
bool parseBool(const QString &str, bool allow_empty = false);
