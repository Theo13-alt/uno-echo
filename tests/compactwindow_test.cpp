/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "compactwindow.h"
#include "inputlayout.h"
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>
#include <QTextEdit>
#include <QProcess>
#include <QScopeGuard>
#include <QMimeData>
#include <QClipboard>
#include <QTimer>
#include <cstdio>
#include <memory>
#include <windows.h>

class CompactWindowTest final : public QObject {
    Q_OBJECT
private slots:
    void correctionUpdatesInputLanguage_data() {
        QTest::addColumn<int>("action"); QTest::addColumn<QString>("text");
        QTest::addColumn<int>("startLanguage"); QTest::addColumn<int>("endLanguage");
        QTest::newRow("selected-en-ru") << int(CompactWindow::SelectionAction::Layout) << QStringLiteral("ghbdtn") << 0x0409 << 0x0419;
        QTest::newRow("selected-ru-en") << int(CompactWindow::SelectionAction::Layout) << QStringLiteral("руддщ") << 0x0419 << 0x0409;
        QTest::newRow("word-en-ru") << int(CompactWindow::SelectionAction::WordLayout) << QStringLiteral("ghbdtn") << 0x0409 << 0x0419;
        QTest::newRow("word-ru-en") << int(CompactWindow::SelectionAction::WordLayout) << QStringLiteral("руддщ") << 0x0419 << 0x0409;
        QTest::newRow("already-destination") << int(CompactWindow::SelectionAction::Layout) << QStringLiteral("ghbdtn") << 0x0419 << 0x0419;
        QTest::newRow("case-keeps-language") << int(CompactWindow::SelectionAction::Case) << QStringLiteral("ABC") << 0x0419 << 0x0419;
    }
    void correctionUpdatesInputLanguage() {
        QFETCH(int, action); QFETCH(QString, text); QFETCH(int, startLanguage); QFETCH(int, endLanguage);
        auto clipboard = std::make_shared<QMimeData>();
        const auto *before = QApplication::clipboard()->mimeData();
        if (before) for (const auto &format : before->formats()) clipboard->setData(format, before->data(format));
        const auto restore = qScopeGuard([&] {
            auto *data = new QMimeData;
            for (const auto &format : clipboard->formats()) data->setData(format, clipboard->data(format));
            QApplication::clipboard()->setMimeData(data);
        });
        CompactWindow window;
        QProcess fixture;
        fixture.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--native-fixture"),
            QString::fromLatin1(text.toUtf8().toBase64()), QString::number(startLanguage)});
        QVERIFY(fixture.waitForStarted());
        const auto closeFixture = qScopeGuard([&] {fixture.kill(); fixture.waitForFinished();});
        QVERIFY(fixture.waitForReadyRead());
        const auto handles = fixture.readLine().trimmed().split(' ');
        QCOMPARE(handles.size(), 2);
        const HWND source = reinterpret_cast<HWND>(handles[0].toULongLong());
        const HWND editor = reinterpret_cast<HWND>(handles[1].toULongLong());
        QVERIFY(IsWindow(source)); SetForegroundWindow(source);
        QTRY_VERIFY_WITH_TIMEOUT(GetForegroundWindow() == source, 45000);
        const DWORD thread = GetWindowThreadProcessId(source, nullptr);
        HKL installed[64]{}; const int count = GetKeyboardLayoutList(64, installed);
        HKL initialLayout = nullptr;
        for (int i = 0; i < count; ++i) if (LOWORD(reinterpret_cast<quintptr>(installed[i])) == startLanguage) initialLayout = installed[i];
        QVERIFY(initialLayout != nullptr);
        QVERIFY(PostMessageW(source, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(initialLayout)));
        QTRY_COMPARE_WITH_TIMEOUT(LOWORD(reinterpret_cast<quintptr>(GetKeyboardLayout(thread))), WORD(startLanguage), 3000);
        QCOMPARE(LOWORD(reinterpret_cast<quintptr>(GetKeyboardLayout(thread))), WORD(startLanguage));
        const auto content = [editor] {
            DWORD_PTR length = 0, copied = 0;
            if (!SendMessageTimeoutW(editor, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, 1000, &length)) return QString();
            QString result(int(length) + 1, QChar());
            if (!SendMessageTimeoutW(editor, WM_GETTEXT, result.size(), reinterpret_cast<LPARAM>(result.data()),
                                     SMTO_ABORTIFHUNG, 1000, &copied)) return QString();
            result.truncate(int(copied)); return result;
        };
        QApplication::clipboard()->setText(QStringLiteral("layout clipboard sentinel"));
        SendMessageW(editor, EM_SETSEL, 0, -1);
        window.prepareCapture(static_cast<CompactWindow::SelectionAction>(action), reinterpret_cast<quintptr>(source));
        window.selectionAvailable(text);
        const QString corrected = action == int(CompactWindow::SelectionAction::Case)
            ? TextTools::invertCase(text) : TextTools::switchLayout(text);
        QTRY_COMPARE_WITH_TIMEOUT(content(), corrected, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(LOWORD(reinterpret_cast<quintptr>(GetKeyboardLayout(thread))), WORD(endLanguage), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!window.m_inserting, 2000);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("layout clipboard sentinel"));
        QVERIFY(GetForegroundWindow() == source);
        // A physical Q key must now type in the destination layout, without Unicode injection.
        INPUT key[2]{};
        key[0].type = key[1].type = INPUT_KEYBOARD;
        key[0].ki.wScan = key[1].ki.wScan = 0x10;
        key[0].ki.dwFlags = KEYEVENTF_SCANCODE;
        key[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        QCOMPARE(SendInput(2, key, sizeof(INPUT)), UINT(2));
        QString suffix = endLanguage == 0x0419 ? QStringLiteral("й") : QStringLiteral("q");
        if (GetKeyState(VK_CAPITAL) & 1) suffix = suffix.toUpper();
        QTRY_COMPARE_WITH_TIMEOUT(content(), corrected + suffix, 2000);
    }
    void invalidLayoutTargetDoesNothing() {
        QVERIFY(!InputLayout::requestForForegroundWindow(0, TextTools::LayoutLanguage::Russian));
        QVERIFY(!InputLayout::requestForForegroundWindow(0, TextTools::LayoutLanguage::Unchanged));
    }
    void backgroundCorrection_data() {
        QTest::addColumn<int>("action"); QTest::addColumn<QString>("input");
        QTest::addColumn<bool>("inFlight");
        QTest::newRow("selected-layout") << int(CompactWindow::SelectionAction::Layout) << QStringLiteral("ghbdtn") << false;
        QTest::newRow("word-layout") << int(CompactWindow::SelectionAction::WordLayout) << QStringLiteral("prefix ghbdtn") << false;
        QTest::newRow("case") << int(CompactWindow::SelectionAction::Case) << QStringLiteral("ABC") << false;
        QTest::newRow("empty-capture") << int(CompactWindow::SelectionAction::WordLayout) << QString() << false;
        QTest::newRow("during-translation") << int(CompactWindow::SelectionAction::Layout) << QStringLiteral("ghbdtn") << true;
    }
    void backgroundCorrection() {
        QFETCH(int, action); QFETCH(QString, input); QFETCH(bool, inFlight);
        QTcpServer holdServer;
        QVERIFY(holdServer.listen(QHostAddress::LocalHost));
        CompactWindow window;
        // Any accidental translation request stays local to this test.
        QVERIFY(window.m_translator->setInstance(QStringLiteral("http://127.0.0.1:%1").arg(holdServer.serverPort())));
        window.m_sourceText = QStringLiteral("original source");
        window.m_translationText = QStringLiteral("original translation");
        window.m_translationEdit->setPlainText(window.m_translationText);
        window.m_status->setText(QStringLiteral("original status"));
        window.m_targetWindow = 0x1234;
        window.m_revision = 42; window.m_activeRevision = 41;
        window.m_copyButton->setEnabled(true); window.m_insertButton->setEnabled(true);
        if (inFlight) {
            window.requestTranslation();
            QTRY_VERIFY(window.m_translator->isRunning());
        }
        const QString sourceBefore = window.m_sourceText, translationBefore = window.m_translationText;
        const QString editBefore = window.m_translationEdit->toPlainText(), statusBefore = window.m_status->text();
        const auto revisionBefore = window.m_revision, activeBefore = window.m_activeRevision;
        const bool copyBefore = window.m_copyButton->isEnabled(), insertBefore = window.m_insertButton->isEnabled();
        QSignalSpy finished(window.m_translator, &OnlineTranslator::finished);
        // Exercise capture setup and completion without foreground or synthetic input.
        // An invalid correction target prevents OS pasting; UI state is still observable.
        window.prepareCapture(static_cast<CompactWindow::SelectionAction>(action), 0);
        window.selectionAvailable(input);
        QVERIFY(!window.m_waitingForSelection);
        QVERIFY(!window.m_inserting);
        QCOMPARE(window.m_sourceText, sourceBefore);
        QCOMPARE(window.m_translationText, translationBefore);
        QCOMPARE(window.m_translationEdit->toPlainText(), editBefore);
        QCOMPARE(window.m_status->text(), statusBefore);
        QCOMPARE(window.m_targetWindow, quintptr(0x1234));
        QCOMPARE(window.m_revision, revisionBefore);
        QCOMPARE(window.m_activeRevision, activeBefore);
        QCOMPARE(window.m_copyButton->isEnabled(), copyBefore);
        QCOMPARE(window.m_insertButton->isEnabled(), insertBefore);
        QVERIFY(!window.isVisible());
        QCOMPARE(finished.size(), 0);
        QCOMPARE(window.m_translator->isRunning(), inFlight);
    }
    void translationCaptureClearsOldResult() {
        CompactWindow window;
        window.m_sourceText = QStringLiteral("old source");
        window.m_translationText = QStringLiteral("old result");
        window.m_translationEdit->setPlainText(window.m_translationText);
        window.m_copyButton->setEnabled(true); window.m_insertButton->setEnabled(true);
        window.prepareCapture(CompactWindow::SelectionAction::Translate, 0x4321);
        QVERIFY(window.m_translationText.isEmpty()); QVERIFY(window.m_sourceText.isEmpty());
        QVERIFY(window.m_translationEdit->toPlainText().isEmpty());
        QVERIFY(!window.m_copyButton->isEnabled()); QVERIFY(!window.m_insertButton->isEnabled());
        QCOMPARE(window.m_targetWindow, quintptr(0x4321));
    }

};
int main(int argc, char **argv) {
    if (argc >= 4 && QByteArray(argv[1]) == "--native-fixture") {
        const QString text = QString::fromUtf8(QByteArray::fromBase64(argv[2]));
        const WORD language = QString::fromLatin1(argv[3]).toUShort();
        HKL layouts[64]{}; const int count = GetKeyboardLayoutList(64, layouts);
        HKL selected = nullptr;
        for (int i = 0; i < count; ++i) if (LOWORD(reinterpret_cast<quintptr>(layouts[i])) == language) selected = layouts[i];
        if (!selected) return 3;
        ActivateKeyboardLayout(selected, 0);
        WNDCLASSW cls{};
        cls.lpfnWndProc = [](HWND handle, UINT message, WPARAM w, LPARAM l) -> LRESULT {
            if (message == WM_SETFOCUS || (message == WM_ACTIVATE && LOWORD(w) != WA_INACTIVE)) {
                if (const HWND child = GetWindow(handle, GW_CHILD)) SetFocus(child);
            }
            if (message == WM_DESTROY) {PostQuitMessage(0); return 0;}
            return DefWindowProcW(handle, message, w, l);
        };
        cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"EchoInputLayoutFixture";
        RegisterClassW(&cls);
        const HWND host = CreateWindowExW(0, cls.lpszClassName, L"Echo input layout fixture",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 480, 160, nullptr, nullptr, cls.hInstance, nullptr);
        const HWND edit = CreateWindowExW(0, L"EDIT", reinterpret_cast<LPCWSTR>(text.utf16()),
            WS_CHILD | WS_VISIBLE | ES_MULTILINE, 0, 0, 460, 120, host, nullptr, cls.hInstance, nullptr);
        if (!host || !edit) return 2;
        SetForegroundWindow(host); SetFocus(edit); ActivateKeyboardLayout(selected, 0);
        std::printf("%llu %llu\n", static_cast<unsigned long long>(reinterpret_cast<quintptr>(host)),
                    static_cast<unsigned long long>(reinterpret_cast<quintptr>(edit))); std::fflush(stdout);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {TranslateMessage(&message); DispatchMessageW(&message);}
        return 0;
    }
    char preview[] = "--preview";
    char *appArgs[] = {argv[0], preview, nullptr}; int appArgc = 2;
    QApplication app(appArgc, appArgs);
    CompactWindowTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "compactwindow_test.moc"
