/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "selection.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTextEdit>
#include <QScopeGuard>
#include <QProcess>
#include <cstdio>
#include <windows.h>

class SelectionTest : public QObject {
    Q_OBJECT
    QScopedPointer<QMimeData> original;
private slots:
    void initTestCase() {
        original.reset(new QMimeData);
        const auto *data = QApplication::clipboard()->mimeData();
        if (data) for (const auto &format : data->formats()) original->setData(format, data->data(format));
    }
    void cleanupTestCase() { QApplication::clipboard()->setMimeData(original.take()); }
    void capturesSelectionAndRestoresClipboard() {
        QTextEdit source; source.setWindowTitle("UNO echo selection test");
        source.setPlainText("Synthetic selected text"); source.selectAll();
        source.show(); source.activateWindow(); source.setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&source));
        QApplication::clipboard()->setText("previous clipboard");
        QTest::qWait(100);
        source.setFocus(); source.selectAll();
        QVERIFY(source.hasFocus());
        QCOMPARE(source.textCursor().selectedText(), QString("Synthetic selected text"));
        // Qt's active flag can be true while Windows still foregrounds another app.
        // Never send the synthetic Ctrl+C until this test owns the foreground.
        QTRY_VERIFY_WITH_TIMEOUT(GetForegroundWindow() == reinterpret_cast<HWND>(source.winId()), 45000);
        QSignalSpy result(&Selection::instance(), &Selection::requestedSelectionAvailable);
        Selection::instance().requestSelection();
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 3000);
        QCOMPARE(result.at(0).at(0).toString(), QString("Synthetic selected text"));
        QCOMPARE(QApplication::clipboard()->text(), QString("previous clipboard"));
    }
    void capturesPreviousWord_data() {
        QTest::addColumn<QString>("text"); QTest::addColumn<QString>("word");
        QTest::newRow("english") << QStringLiteral("before ghbdtn") << QStringLiteral("ghbdtn");
        QTest::newRow("russian") << QStringLiteral("перед привет") << QStringLiteral("привет");
        QTest::newRow("single") << QStringLiteral("Ghbdtn") << QStringLiteral("Ghbdtn");
        QTest::newRow("empty") << QString() << QString();
    }
    void capturesPreviousWord() {
        QFETCH(QString, text); QFETCH(QString, word);
        QProcess fixture;
        fixture.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--native-fixture"),
            QString::fromLatin1(text.toUtf8().toBase64())});
        QVERIFY(fixture.waitForStarted());
        const auto destroy = qScopeGuard([&fixture] { fixture.kill(); fixture.waitForFinished(); });
        QVERIFY(fixture.waitForReadyRead());
        const HWND source = reinterpret_cast<HWND>(fixture.readLine().trimmed().toULongLong());
        QVERIFY(IsWindow(source));
        SetForegroundWindow(source); SetFocus(source);
        SendMessageW(source, EM_SETSEL, text.size(), text.size());
        QTRY_VERIFY_WITH_TIMEOUT(GetForegroundWindow() == source, 3000);
        QApplication::clipboard()->setText("word-test clipboard sentinel");
        QSignalSpy result(&Selection::instance(), &Selection::requestedSelectionAvailable);
        Selection::instance().requestSelection(true);
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 3000);
        DWORD start = 0, end = 0;
        SendMessageW(source, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        QCOMPARE(result.at(0).at(0).toString(), word);
        SendMessageW(source, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        QCOMPARE(text.mid(start, end - start), word);
        QString actual(text.size() + 1, QChar());
        GetWindowTextW(source, reinterpret_cast<LPWSTR>(actual.data()), actual.size());
        actual.truncate(text.size());
        QCOMPARE(actual, text);
        QCOMPARE(QApplication::clipboard()->text(), QString("word-test clipboard sentinel"));
    }
    void emptySelectionDoesNotTranslateOldClipboard() {
        QTextEdit source; source.show(); source.activateWindow(); source.setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&source));
        QApplication::clipboard()->setText("old text must not become translation");
        QSignalSpy result(&Selection::instance(), &Selection::requestedSelectionAvailable);
        Selection::instance().requestSelection();
        Selection::instance().requestSelection();
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 3000);
        QVERIFY(result.at(0).at(0).toString().isEmpty());
        QCOMPARE(QApplication::clipboard()->text(), QString("old text must not become translation"));
    }
};
int main(int argc, char **argv) {
    if (argc >= 3 && QByteArray(argv[1]) == "--native-fixture") {
        const QString text = QString::fromUtf8(QByteArray::fromBase64(argv[2]));
        const HWND edit = CreateWindowExW(0, L"EDIT", reinterpret_cast<LPCWSTR>(text.utf16()),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | ES_MULTILINE, 100, 100, 480, 160,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!edit) return 2;
        SetForegroundWindow(edit); SetFocus(edit);
        std::printf("%llu\n", static_cast<unsigned long long>(reinterpret_cast<quintptr>(edit)));
        std::fflush(stdout);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        return 0;
    }
    QApplication app(argc, argv);
    SelectionTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "selection_test.moc"
