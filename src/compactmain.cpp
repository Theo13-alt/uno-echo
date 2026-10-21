/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compactwindow.h"
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QPainter>
int main(int argc, char *argv[]) {
    QApplication::setApplicationName(QStringLiteral("crow-compact"));
    QApplication::setApplicationVersion(QStringLiteral(UNO_ECHO_VERSION));
    QApplication::setOrganizationName(QStringLiteral("crow-translate"));
    QApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral("UNO echo"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setEffectEnabled(Qt::UI_AnimateCombo, false);
    app.setWindowIcon(QIcon(QStringLiteral(":/echo/echo.svg")));
    QFontDatabase::addApplicationFont(QStringLiteral(":/echo/inter-400.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/echo/jura-600.ttf"));
    const bool preview = app.arguments().contains(QStringLiteral("--preview"));
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data);
    QLockFile lock(data + QStringLiteral("/uno-echo.lock"));
    QLocalServer server;
    const QString serverName = QStringLiteral("uno-echo-") + qEnvironmentVariable("USERNAME");
    if (!preview) {
        if (!lock.tryLock(0)) {
            QLocalSocket socket; socket.connectToServer(serverName);
            if (socket.waitForConnected(1000)) { socket.write("show"); socket.waitForBytesWritten(1000); }
            return 0;
        }
        QLocalServer::removeServer(serverName);
        server.listen(serverName);
    }
    QFile theme(QStringLiteral(":/echo/echo.qss"));
    if (theme.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(theme.readAll()));
    CompactWindow window;
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        auto *socket = server.nextPendingConnection();
        window.showWindow(); socket->disconnectFromServer(); socket->deleteLater();
    });
    window.show();
    if (preview) {
        const int index = app.arguments().indexOf(QStringLiteral("--preview"));
        if (app.arguments().contains(QStringLiteral("--about")))
            window.findChild<QPushButton *>(QStringLiteral("aboutButton"))->click();
        if (app.arguments().contains(QStringLiteral("--server"))) {
            window.findChild<QPushButton *>(QStringLiteral("aboutButton"))->click();
            window.findChild<QPushButton *>(QStringLiteral("serverSettingsButton"))->click();
        }
        QComboBox *popupCombo = nullptr;
        if (app.arguments().contains(QStringLiteral("--engine-popup")))
            popupCombo = window.findChildren<QComboBox *>().at(1);
        else if (app.arguments().contains(QStringLiteral("--language-popup")))
            popupCombo = window.findChildren<QComboBox *>().at(0);
        if (popupCombo) {
            popupCombo->setCurrentIndex(2);
            popupCombo->showPopup();
        }
        QTimer::singleShot(400, &window, [&] {
            QWidget *capture = &window;
            if (auto *about = window.findChild<QDialog *>(QStringLiteral("aboutDialog"))) capture = about;
            if (auto *server = window.findChild<QDialog *>(QStringLiteral("serverDialog"))) capture = server;
            if (QWidget *popup = QApplication::activePopupWidget()) capture = popup;
            if (index + 1 < app.arguments().size()) {
                QPixmap snapshot = capture->grab();
                if (capture != &window && app.arguments().contains(QStringLiteral("--popup-context"))) {
                    const QPoint offset = window.mapFromGlobal(capture->mapToGlobal(QPoint()));
                    snapshot = window.grab();
                    QPainter painter(&snapshot);
                    painter.translate(offset);
                    painter.setClipRegion(capture->mask());
                    painter.drawPixmap(0, 0, capture->grab());
                }
                snapshot.save(app.arguments().at(index + 1));
            }
            app.quit();
        });
    }
    return app.exec();
}
