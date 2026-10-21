/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "selection.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTextEdit>
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
QTEST_MAIN(SelectionTest)
#include "selection_test.moc"
