/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TEXTTOOLS_H
#define TEXTTOOLS_H
#include <QString>
namespace TextTools {
enum class LayoutLanguage { Unchanged, English, Russian };
LayoutLanguage layoutDestination(const QString &text);
QString switchLayout(const QString &text);
QString invertCase(const QString &text);
}
#endif
