/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "texttools.h"

namespace {
    // Standard US QWERTY <-> Russian ЙЦУКЕН, including shifted punctuation.
    const QString english = QStringLiteral(R"(`qwertyuiop[]asdfghjkl;'zxcvbnm,./\~QWERTYUIOP{}ASDFGHJKL:"ZXCVBNM<>?|!@#$%^&*()_+)" );
    const QString russian = QStringLiteral(R"(ёйцукенгшщзхъфывапролджэячсмитьбю.\ЁЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ,/!"№;%:?*()_+)" );
}

TextTools::LayoutLanguage TextTools::layoutDestination(const QString &text)
{
    Q_ASSERT(english.size() == russian.size());
    int latin = 0, cyrillic = 0;
    for (const auto ch : text) {
        if (ch.isLetter() && english.contains(ch)) ++latin;
        if (ch.isLetter() && russian.contains(ch)) ++cyrillic;
    }
    if (latin == 0 && cyrillic == 0) return LayoutLanguage::Unchanged;
    return cyrillic > latin ? LayoutLanguage::English : LayoutLanguage::Russian;
}

QString TextTools::switchLayout(const QString &text)
{
    const auto destination = layoutDestination(text);
    if (destination == LayoutLanguage::Unchanged) return text;
    const bool toEnglish = destination == LayoutLanguage::English;
    const QString &from = toEnglish ? russian : english;
    const QString &to = toEnglish ? english : russian;
    QString result = text;
    for (auto &ch : result) {
        const auto index = from.indexOf(ch);
        if (index >= 0) ch = to.at(index);
    }
    return result;
}

QString TextTools::invertCase(const QString &text)
{
    QString result;
    result.reserve(text.size());
    // QString casing retains Unicode mappings that expand into several letters.
    for (auto ch : text) {
        const QString letter(ch);
        result += ch.isUpper() ? letter.toLower() : ch.isLower() ? letter.toUpper() : letter;
    }
    return result;
}
