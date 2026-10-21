/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COMPACTWINDOW_H
#define COMPACTWINDOW_H

#include "onlinetranslator.h"

#include <QDialog>

class QCloseEvent;
class QComboBox;
class QHotkey;
class QMenu;
class QPushButton;
class QSystemTrayIcon;
class QTextEdit;
class QLabel;

class CompactWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit CompactWindow(QWidget *parent = nullptr);
    ~CompactWindow() override;
    void showWindow() { restoreFromTray(); }

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct PendingRequest {
        QString text;
        OnlineTranslator::Language destination = OnlineTranslator::English;
        OnlineTranslator::Engine engine = OnlineTranslator::Google;
        quint64 revision = 0;
        bool valid = false;
    };

    void requestSelection();
    void selectionAvailable(const QString &text);
    void requestTranslation();
    void startRequest(const PendingRequest &request);
    void translationFinished();
    OnlineTranslator::Language destinationForText(const QString &text) const;
    void insertTranslation();
    void minimizeToTray();
    void restoreFromTray();
    void registerHotkey();
    void saveUiState() const;
    void setStatus(const QString &text);
    void showAbout();
    void showServerSettings();

    QComboBox *m_languageCombo;
    QComboBox *m_engineCombo;
    QTextEdit *m_translationEdit;
    QPushButton *m_insertButton;
    QHotkey *m_hotkey;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QLabel *m_status;
    QPushButton *m_copyButton;
    OnlineTranslator *m_translator;
    QString m_sourceText;
    QString m_translationText;
    PendingRequest m_pending;
    quint64 m_revision = 0;
    quint64 m_activeRevision = 0;
    quintptr m_targetWindow = 0;
    bool m_waitingForSelection = false;
    bool m_restoringFromTray = false;
};

#endif // COMPACTWINDOW_H
