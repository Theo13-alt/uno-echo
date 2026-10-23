/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "selection.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>
#include <windows.h>
Selection::~Selection() = default;
Selection &Selection::instance() { static Selection value; return value; }
Selection::Selection() : m_timeout(new QTimer(this)) {
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { finish(QString()); });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &Selection::clipboardChanged);
}
void Selection::requestSelection(bool previousWord) {
    if (m_busy) return;
    m_busy = true; m_copySent = false;
    m_previousWord = previousWord; m_wordSelected = false;
    m_target = reinterpret_cast<quintptr>(GetForegroundWindow());
    m_original.reset(new QMimeData);
    const auto *data = QApplication::clipboard()->mimeData();
    if (data) for (const QString &format : data->formats()) m_original->setData(format, data->data(format));
    m_elapsed.start();
    sendCopyShortcutWhenReady();
}
void Selection::sendCopyShortcutWhenReady() {
    if (!m_busy) return;
    if (reinterpret_cast<quintptr>(GetForegroundWindow()) != m_target || m_elapsed.elapsed() > 2000) {
        finish(QString()); return;
    }
    if ((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_SHIFT)
        | GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) {
        QTimer::singleShot(10, this, &Selection::sendCopyShortcutWhenReady); return;
    }
    if (m_previousWord && !m_wordSelected) {
        // Use the source editor's own word boundary, at the caret. No text is deleted.
        INPUT select[6]{};
        for (auto &event : select) event.type = INPUT_KEYBOARD;
        select[0].ki.wVk = VK_CONTROL; select[1].ki.wVk = VK_SHIFT;
        // Extended arrow avoids NumLock keypad emulation releasing Shift.
        select[2].ki.wVk = VK_LEFT; select[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
        select[3].ki.wVk = VK_LEFT; select[3].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
        select[4].ki.wVk = VK_SHIFT; select[4].ki.dwFlags = KEYEVENTF_KEYUP;
        select[5].ki.wVk = VK_CONTROL; select[5].ki.dwFlags = KEYEVENTF_KEYUP;
        if (SendInput(6, select, sizeof(INPUT)) != 6) { finish(QString()); return; }
        m_wordSelected = true;
        QTimer::singleShot(40, this, &Selection::sendCopyShortcutWhenReady);
        return;
    }
    m_sequence = GetClipboardSequenceNumber();
    m_copySent = true;
    m_timeout->start(1200);
    INPUT input[4]{};
    for (auto &event : input) event.type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_CONTROL; input[1].ki.wVk = 'C';
    input[2].ki.wVk = 'C'; input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].ki.wVk = VK_CONTROL; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(4, input, sizeof(INPUT)) != 4) finish(QString());
}
void Selection::clipboardChanged() {
    if (!m_busy || !m_copySent || GetClipboardSequenceNumber() == m_sequence) return;
    // Clipboard notifications may arrive before the source releases its lock.
    QTimer::singleShot(30, this, [this] {
        if (!m_busy || !m_copySent || GetClipboardSequenceNumber() == m_sequence) return;
        DWORD ownerProcess = 0, targetProcess = 0;
        GetWindowThreadProcessId(GetClipboardOwner(), &ownerProcess);
        GetWindowThreadProcessId(reinterpret_cast<HWND>(m_target), &targetProcess);
        if (GetForegroundWindow() != reinterpret_cast<HWND>(m_target)
            || !ownerProcess || ownerProcess != targetProcess) return;
        const auto text = QApplication::clipboard()->text();
        if (text.isEmpty()) return;
        m_captureSequence = GetClipboardSequenceNumber();
        finish(text);
    });
}
void Selection::finish(const QString &text) {
    if (!m_busy) return;
    m_timeout->stop();
    // Restore only the clipboard change produced by this capture, never a later user copy.
    if (!text.isEmpty()) {
        m_copySent = false;
        QTimer::singleShot(0, this, [this, text] {
            if (GetClipboardSequenceNumber() == m_captureSequence)
                QApplication::clipboard()->setMimeData(m_original.take());
            m_original.reset(); m_busy = false;
            emit requestedSelectionAvailable(text);
        });
    } else {
        m_original.reset(); m_busy = false;
        emit requestedSelectionAvailable(text);
    }
}
