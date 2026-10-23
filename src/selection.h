/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SELECTION_H
#define SELECTION_H
#include <QObject>
#include <QElapsedTimer>
#include <QScopedPointer>
class QMimeData;
class QTimer;
class Selection : public QObject {
    Q_OBJECT
public:
    ~Selection() override;
    static Selection &instance();
    void requestSelection(bool previousWord = false);
signals:
    void requestedSelectionAvailable(const QString &text);
private:
    Selection();
    void sendCopyShortcutWhenReady();
    void clipboardChanged();
    void finish(const QString &text);
    QScopedPointer<QMimeData> m_original;
    QTimer *m_timeout;
    QElapsedTimer m_elapsed;
    quintptr m_target = 0;
    unsigned long m_sequence = 0;
    unsigned long m_captureSequence = 0;
    bool m_busy = false;
    bool m_copySent = false;
    bool m_previousWord = false;
    bool m_wordSelected = false;
};
#endif
