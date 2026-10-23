/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "texttools.h"
#include <QTest>
class TextToolsTest final : public QObject {
    Q_OBJECT
private slots:
    void layout_data() {
        QTest::addColumn<QString>("source"); QTest::addColumn<QString>("expected");
        QTest::newRow("en-ru") << QStringLiteral("Ghbdtn? vbh!") << QStringLiteral("Привет, мир!");
        QTest::newRow("ru-en") << QStringLiteral("Руддщб цщкдв!") << QStringLiteral("Hello, world!");
        QTest::newRow("upper") << QStringLiteral("GHBDTN") << QStringLiteral("ПРИВЕТ");
        QTest::newRow("yo") << QStringLiteral("`~") + QStringLiteral("ghbdtn") << QStringLiteral("ёЁпривет");
        QTest::newRow("symbols") << QStringLiteral("@#$^& ghbdtn") << QStringLiteral("\"№;:? привет");
        QTest::newRow("unicode") << QStringLiteral("ghbdtn\n😀 中文") << QStringLiteral("привет\n😀 中文");
        QTest::newRow("no-letters") << QStringLiteral("123 + 😀") << QStringLiteral("123 + 😀");
        QTest::newRow("empty") << QString() << QString();
    }
    void layout() { QFETCH(QString, source); QFETCH(QString, expected); QCOMPARE(TextTools::switchLayout(source), expected); }
    void destination() {
        QCOMPARE(TextTools::layoutDestination(QStringLiteral("JKGGGKjljkjh")), TextTools::LayoutLanguage::Russian);
        QCOMPARE(TextTools::layoutDestination(QStringLiteral("руддщ")), TextTools::LayoutLanguage::English);
        QCOMPARE(TextTools::layoutDestination(QStringLiteral("123 😀")), TextTools::LayoutLanguage::Unchanged);
        QCOMPARE(TextTools::layoutDestination(QString()), TextTools::LayoutLanguage::Unchanged);
        QCOMPARE(TextTools::layoutDestination(QStringLiteral("aф")), TextTools::LayoutLanguage::Russian);
    }
    void casing_data() {
        QTest::addColumn<QString>("source"); QTest::addColumn<QString>("expected");
        QTest::newRow("caps") << QStringLiteral("ПРИВЕТ WORLD!") << QStringLiteral("привет world!");
        QTest::newRow("mixed") << QStringLiteral("пРиВеТ cAPS 100% 😀\nЁё") << QStringLiteral("ПрИвЕт Caps 100% 😀\nёЁ");
        QTest::newRow("symbols") << QStringLiteral("+ 123 & 😀") << QStringLiteral("+ 123 & 😀");
    }
    void reversibleLayout() {
        const QString source = QStringLiteral("`qwertyuiop[]asdfghjkl;'zxcvbnm,./\\~QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>?|!@#$%^&*()_+" );
        QCOMPARE(TextTools::switchLayout(TextTools::switchLayout(source)), source);
    }
    void casing() { QFETCH(QString, source); QFETCH(QString, expected); QCOMPARE(TextTools::invertCase(source), expected); }
};
QTEST_GUILESS_MAIN(TextToolsTest)
#include "texttools_test.moc"
