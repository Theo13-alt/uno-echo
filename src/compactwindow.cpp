/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compactwindow.h"

#include "qhotkey.h"
#include "selection.h"

#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QListView>
#include <QStyledItemDelegate>
#include <QPainterPath>
#include <QRegion>
#include <dwmapi.h>

#include <algorithm>

namespace {
class SpacedComboBox final : public QComboBox
{
public:
    using QComboBox::QComboBox;

    void showPopup() override
    {
        QComboBox::showPopup();
        auto *popup = view()->window();
        if (!popup->isVisible()) return;
        const QRect available = popup->screen()->availableGeometry();
        const int below = mapToGlobal(QPoint(0, height())).y() + 4;
        const int above = mapToGlobal(QPoint(0, 0)).y() - popup->height() - 4;
        const int y = below + popup->height() <= available.bottom() + 1 ? below : above;
        const int targetTop = qBound(available.top(), y,
                    qMax(available.top(), available.bottom() + 1 - popup->height()));
        // move() positions the native frame; align the painted client area instead.
        popup->move(popup->pos() + QPoint(0, targetTop - popup->mapToGlobal(QPoint()).y()));
    }
};

class RoundedPopupView final : public QListView
{
public:
    using QListView::QListView;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == window() &&
            (event->type() == QEvent::Show || event->type() == QEvent::Resize)) {
            // The popup is a separate native window; QSS alone leaves square corners.
            QPainterPath outline;
            outline.addRoundedRect(QRectF(window()->rect()), 6, 6);
            window()->setMask(QRegion(outline.toFillPolygon().toPolygon()));
        }
        return QListView::eventFilter(watched, event);
    }
};
}

#ifdef Q_OS_WIN
#include <windows.h>
#endif

CompactWindow::CompactWindow(QWidget *parent)
    : QDialog(parent)
    , m_languageCombo(new SpacedComboBox(this))
    , m_engineCombo(new SpacedComboBox(this))
    , m_translationEdit(new QTextEdit(this))
    , m_insertButton(new QPushButton(tr("Вставить / заменить"), this))
    , m_hotkey(new QHotkey(this))
    , m_trayIcon(new QSystemTrayIcon(QGuiApplication::windowIcon(), this))
    , m_trayMenu(new QMenu(this))
    , m_status(new QLabel(this))
    , m_copyButton(new QPushButton(tr("Копировать"), this))
    , m_translator(new OnlineTranslator(this))
{
    setWindowTitle(QStringLiteral("UNO echo"));
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
    resize(540, 400);
    setMinimumSize(420, 320);
    const COLORREF caption = RGB(28, 25, 33);
    const COLORREF captionText = RGB(212, 207, 216);
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 36, &captionText, sizeof(captionText));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(16);
    auto *heading = new QHBoxLayout;
    auto *brand = new QLabel(QStringLiteral("UNO echo"), this);
    brand->setObjectName(QStringLiteral("brand"));
    heading->addWidget(brand);
    heading->addStretch();
    auto *shortcut = new QLabel(QStringLiteral("Ctrl + Alt + E"), this);
    shortcut->setObjectName(QStringLiteral("shortcut"));
    heading->addWidget(shortcut);
    layout->addLayout(heading);

    auto *languageRow = new QHBoxLayout;
    m_languageCombo->setToolTip(tr("Направление перевода"));
    m_languageCombo->addItem(tr("Авто · RU ↔ EN"), QString());
    m_languageCombo->addItem(tr("На английский"), QStringLiteral("en"));
    m_languageCombo->addItem(tr("На русский"), QStringLiteral("ru"));
    languageRow->addWidget(m_languageCombo, 1);
    languageRow->setSpacing(12);
    m_engineCombo->setToolTip(tr("Сервис перевода. Выделенный текст отправляется выбранному сервису через Mozhi."));
    const auto engineMeta = QMetaEnum::fromType<OnlineTranslator::Engine>();
    for (int i = 0; i < engineMeta.keyCount(); ++i) {
        const auto engine = static_cast<OnlineTranslator::Engine>(engineMeta.value(i));
        if (engine != OnlineTranslator::Google && engine != OnlineTranslator::Yandex && engine != OnlineTranslator::Duckduckgo)
            continue;
        QString name = QString::fromLatin1(engineMeta.key(i));
        if (engine == OnlineTranslator::Duckduckgo)
            name = QStringLiteral("DuckDuckGo / Bing");
        m_engineCombo->addItem(name, static_cast<int>(engine));
    }
    languageRow->addWidget(m_engineCombo, 1);
    layout->addLayout(languageRow);
    for (auto *combo : {m_languageCombo, m_engineCombo}) {
        auto *view = new RoundedPopupView(combo);
        view->setMouseTracking(true);
        view->setItemDelegate(new QStyledItemDelegate(view));
        combo->setView(view);
        view->window()->installEventFilter(view);
    }

    m_translationEdit->setReadOnly(true);
    m_translationEdit->setPlaceholderText(tr("Выдели текст в любом приложении\nи нажми Ctrl + Alt + E"));
    layout->addWidget(m_translationEdit, 1);

    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    buttons->addWidget(m_copyButton);
    buttons->addWidget(m_insertButton, 1);
    m_insertButton->setObjectName(QStringLiteral("insert"));
    layout->addLayout(buttons);
    m_copyButton->setEnabled(false);
    m_insertButton->setEnabled(false);
    auto *signature = new QHBoxLayout;
    signature->addStretch();
    signature->setSpacing(8);
    auto *spider = new QPushButton(this);
    spider->setObjectName(QStringLiteral("aboutButton"));
    spider->setIcon(QIcon(QStringLiteral(":/echo/arachna.svg")));
    spider->setIconSize(QSize(24, 20));
    spider->setFixedSize(36, 28);
    spider->setAccessibleName(tr("О программе UNO echo"));
    spider->setToolTip(tr("О программе, авторы и лицензии"));
    spider->setCursor(Qt::PointingHandCursor);
    connect(spider, &QPushButton::clicked, this, &CompactWindow::showAbout);
    signature->addWidget(spider);
    signature->addStretch();
    layout->addLayout(signature);

    QSettings settings;
    m_languageCombo->setCurrentIndex(qBound(0, settings.value(QStringLiteral("Compact/Language"), 0).toInt(), m_languageCombo->count() - 1));
    const int savedEngine = settings.value(QStringLiteral("Compact/Engine"), static_cast<int>(OnlineTranslator::Google)).toInt();
    m_engineCombo->setCurrentIndex(qMax(0, m_engineCombo->findData(savedEngine)));
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    move(available.center() - rect().center());

    const bool validServer = m_translator->setInstance(settings.value(QStringLiteral("Compact/MozhiUrl"), OnlineTranslator::defaultInstance()).toString());

    connect(m_hotkey, &QHotkey::activated, this, &CompactWindow::requestSelection);
    connect(&Selection::instance(), &Selection::requestedSelectionAvailable, this, &CompactWindow::selectionAvailable);
    connect(m_translator, &OnlineTranslator::finished, this, &CompactWindow::translationFinished);
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this] { requestTranslation(); });
    connect(m_engineCombo, &QComboBox::currentIndexChanged, this, [this] { requestTranslation(); });
    connect(m_insertButton, &QPushButton::clicked, this, &CompactWindow::insertTranslation);
    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_translationText);
        setStatus(tr("Перевод скопирован"));
    });

    auto *showAction = m_trayMenu->addAction(tr("Показать UNO echo"));
    auto *aboutAction = m_trayMenu->addAction(tr("О программе"));
    auto *serverAction = m_trayMenu->addAction(tr("Сервер перевода…"));
    auto *exitAction = m_trayMenu->addAction(tr("Выйти"));
    connect(showAction, &QAction::triggered, this, &CompactWindow::restoreFromTray);
    connect(aboutAction, &QAction::triggered, this, &CompactWindow::showAbout);
    connect(serverAction, &QAction::triggered, this, &CompactWindow::showServerSettings);
    connect(exitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            restoreFromTray();
    });
    if (!qApp->arguments().contains(QStringLiteral("--preview"))) m_trayIcon->show();
    qApp->setQuitOnLastWindowClosed(false);
    if (!qApp->arguments().contains(QStringLiteral("--preview")))
        QTimer::singleShot(0, this, &CompactWindow::registerHotkey);
    setStatus(validServer ? tr("Готов к переводу выделенного текста") : tr("Сохранённый адрес Mozhi некорректен. Откройте «Сервер перевода» в меню трея."));
}

CompactWindow::~CompactWindow()
{
    saveUiState();
}

void CompactWindow::showAbout()
{
    if (auto *existing = findChild<QDialog *>(QStringLiteral("aboutDialog"))) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("aboutDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("О программе UNO echo"));
    dialog->setWindowFlag(Qt::WindowStaysOnTopHint);
    dialog->resize(500, 450);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    auto *text = new QTextBrowser(dialog);
    text->setOpenExternalLinks(true);
    text->document()->setDefaultStyleSheet(QStringLiteral("a { color: #b49af0; }"));
    text->setHtml(tr(
        "<h2>UNO echo %1</h2>"
        "<p>Компактный переводчик для Windows.<br>Разработка и адаптация — <b>Arachna</b>.</p>"
        "<p>Создан на основе <a href=\"https://invent.kde.org/office/crow-translate\">Crow Translate</a>. "
        "Используется код Hennadii Chernyshchyk и Volk Milit. Спасибо авторам исходного проекта.</p>"
        "<p>Лицензия приложения — <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL-3.0-or-later</a>. "
        "Исходники и инструкции сборки включены в <b>source.zip</b> portable-пакета.</p>"
        "<p>Горячие клавиши — <b>QHotkey</b>, Skycoder42 (BSD-3-Clause).<br>"
        "Интерфейс — <b>Qt</b>, динамическая линковка.<br>"
        "Шрифты — <b>Inter</b> и <b>Jura</b> (SIL OFL 1.1).</p>"
        "<p>Полные уведомления — в <b>CREDITS.md</b> и папке <b>licenses</b> рядом с приложением.</p>")
        .arg(QCoreApplication::applicationVersion().toHtmlEscaped()));
    layout->addWidget(text);
    auto *server = new QLabel(tr("Сервер перевода: %1").arg(m_translator->instance().isEmpty() ? tr("не задан") : m_translator->instance()), dialog);
    server->setObjectName(QStringLiteral("aboutServer"));
    server->setTextFormat(Qt::PlainText);
    server->setWordWrap(true);
    server->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(server);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Закрыть"));
    auto *licenses = buttons->addButton(tr("Лицензии"), QDialogButtonBox::ActionRole);
    auto *serverButton = buttons->addButton(tr("Сервер…"), QDialogButtonBox::ActionRole);
    serverButton->setObjectName(QStringLiteral("serverSettingsButton"));
    connect(serverButton, &QPushButton::clicked, this, &CompactWindow::showServerSettings);
    connect(licenses, &QPushButton::clicked, dialog, [] {
        QDir folder(QCoreApplication::applicationDirPath());
        if (!folder.cd(QStringLiteral("licenses"))) {
            folder.cdUp();
            folder.cd(QStringLiteral("LICENSES"));
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder.absolutePath()));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}

void CompactWindow::showServerSettings()
{
    if (auto *existing = findChild<QDialog *>(QStringLiteral("serverDialog"))) {
        existing->show(); existing->raise(); existing->activateWindow();
        return;
    }
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("serverDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Сервер перевода"));
    dialog->setWindowFlag(Qt::WindowStaysOnTopHint);
    dialog->resize(480, 230);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    auto *description = new QLabel(tr("Адрес сайта Mozhi, без /api/translate.\nВыделенный текст передаётся этому серверу. Новый адрес используется со следующего перевода."), dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *input = new QLineEdit(dialog);
    input->setObjectName(QStringLiteral("serverUrlInput"));
    input->setPlaceholderText(OnlineTranslator::defaultInstance());
    QSettings settings;
    input->setText(settings.value(QStringLiteral("Compact/MozhiUrl"), OnlineTranslator::defaultInstance()).toString());
    layout->addWidget(input);
    auto *error = new QLabel(dialog);
    error->setObjectName(QStringLiteral("serverError"));
    error->setTextFormat(Qt::PlainText);
    error->setWordWrap(true);
    error->hide();
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Сохранить"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("saveServerButton"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    auto *reset = buttons->addButton(tr("По умолчанию"), QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, input, [input] { input->setText(OnlineTranslator::defaultInstance()); });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, input, error] {
        if (m_waitingForSelection) {
            error->setText(tr("Дождись завершения захвата текста и повтори сохранение."));
            error->show(); return;
        }
        const QString url = OnlineTranslator::normalizedInstanceUrl(input->text());
        if (url.isEmpty()) {
            error->setText(tr("Нужен адрес http:// или https:// без логина, пароля, параметров и /api/translate."));
            error->show(); return;
        }
        if (!qApp->arguments().contains(QStringLiteral("--preview"))) {
            QSettings settings;
            settings.setValue(QStringLiteral("Compact/MozhiUrl"), url);
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                error->setText(tr("Не удалось сохранить адрес сервера."));
                error->show(); return;
            }
        }
        // Do not resend the previous selection to a newly chosen server.
        ++m_revision;
        m_pending.valid = false;
        m_activeRevision = 0;
        m_waitingForSelection = false;
        m_translator->abort();
        m_translator->setInstance(url);
        m_sourceText.clear(); m_translationText.clear(); m_targetWindow = 0;
        m_translationEdit->clear();
        m_copyButton->setEnabled(false); m_insertButton->setEnabled(false);
        if (auto *server = findChild<QLabel *>(QStringLiteral("aboutServer")))
            server->setText(tr("Сервер перевода: %1").arg(url));
        setStatus(tr("Сервер сохранён. Выдели текст для нового перевода."));
        dialog->close();
    });
    layout->addWidget(buttons);
    dialog->show();
    input->setFocus();
}

void CompactWindow::closeEvent(QCloseEvent *event)
{
    saveUiState();
    m_hotkey->setRegistered(false);
    m_trayIcon->hide();
    event->accept();
    qApp->quit();
}

void CompactWindow::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && !m_restoringFromTray)
        minimizeToTray();
}

void CompactWindow::requestSelection()
{
    if (m_waitingForSelection) return;
#ifdef Q_OS_WIN
    const HWND target = GetForegroundWindow();
    DWORD process = 0;
    GetWindowThreadProcessId(target, &process);
    if (process == GetCurrentProcessId()) return;
    m_targetWindow = reinterpret_cast<quintptr>(target);
#endif
    ++m_revision;
    m_pending.valid = false;
    m_sourceText.clear();
    m_translationText.clear();
    m_insertButton->setEnabled(false);
    m_copyButton->setEnabled(false);
    m_translationEdit->clear();
    setStatus(tr("Получаю выделенный текст…"));
    m_waitingForSelection = true;
    Selection::instance().requestSelection();
}

void CompactWindow::selectionAvailable(const QString &text)
{
    if (!m_waitingForSelection)
        return;
    m_waitingForSelection = false;
    if (text.trimmed().isEmpty()) {
        setStatus(tr("Не удалось получить выделение. Выдели текст и повтори сочетание."));
        restoreFromTray();
        return;
    }
    m_sourceText = text;
    requestTranslation();
    restoreFromTray();
}

OnlineTranslator::Language CompactWindow::destinationForText(const QString &text) const
{
    const QString code = m_languageCombo->currentData().toString();
    if (code == QStringLiteral("en"))
        return OnlineTranslator::English;
    if (code == QStringLiteral("ru"))
        return OnlineTranslator::Russian;
    const bool containsCyrillic = std::any_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.script() == QChar::Script_Cyrillic;
    });
    return containsCyrillic ? OnlineTranslator::English : OnlineTranslator::Russian;
}

void CompactWindow::requestTranslation()
{
    saveUiState();
    if (m_sourceText.isEmpty())
        return;
    ++m_revision;
    m_translationText.clear();
    m_copyButton->setEnabled(false);
    m_insertButton->setEnabled(false);
    setStatus(tr("Перевожу…"));
    PendingRequest request{m_sourceText,
                           destinationForText(m_sourceText),
                           static_cast<OnlineTranslator::Engine>(m_engineCombo->currentData().toInt()),
                           m_revision,
                           true};
    if (!m_translator->isRunning())
        startRequest(request);
    else
        m_pending = request;
}

void CompactWindow::startRequest(const PendingRequest &request)
{
    m_pending.valid = false;
    m_activeRevision = request.revision;
    m_translationText.clear();
    m_translationEdit->clear();
    m_insertButton->setEnabled(false);
    m_translator->translate(request.text, request.engine, request.destination, OnlineTranslator::Auto);
}

void CompactWindow::translationFinished()
{
    if (m_activeRevision == m_revision && m_translator->error() == OnlineTranslator::NoError) {
        m_translationText = m_translator->translation();
        m_translationEdit->setPlainText(m_translationText);
        m_insertButton->setEnabled(!m_translationText.isEmpty() && m_targetWindow != 0);
        m_copyButton->setEnabled(!m_translationText.isEmpty());
        setStatus(m_translationText.isEmpty() ? tr("Сервис вернул пустой ответ") : tr("Перевод готов"));
    } else if (m_activeRevision == m_revision && m_translator->error() != OnlineTranslator::NoError) {
        m_translationText.clear();
        m_translationEdit->clear();
        setStatus(tr("Ошибка: %1").arg(m_translator->errorString()));
        m_insertButton->setEnabled(false);
    }

    if (m_pending.valid) {
        const PendingRequest request = m_pending;
        if (request.revision == m_revision && request.text == m_sourceText)
            startRequest(request);
        else
            m_pending.valid = false;
    }
}

void CompactWindow::insertTranslation()
{
#ifdef Q_OS_WIN
    const auto target = reinterpret_cast<HWND>(m_targetWindow);
    if (target == nullptr || !IsWindow(target) || m_translationText.isEmpty())
        return;
    if (!SetForegroundWindow(target)) {
        setStatus(tr("Не удалось вернуть фокус. Скопируй перевод вручную."));
        return;
    }
    const QString translation = m_translationText;
    QTimer::singleShot(100, this, [this, target, translation] {
        if (!IsWindow(target) || GetForegroundWindow() != target) {
            setStatus(tr("Окно переключилось — вставка отменена."));
            return;
        }
        QApplication::clipboard()->setText(translation);
        INPUT input[4]{};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_CONTROL;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = 'V';
        input[2].type = INPUT_KEYBOARD;
        input[2].ki.wVk = 'V';
        input[2].ki.dwFlags = KEYEVENTF_KEYUP;
        input[3].type = INPUT_KEYBOARD;
        input[3].ki.wVk = VK_CONTROL;
        input[3].ki.dwFlags = KEYEVENTF_KEYUP;
        const bool inserted = SendInput(static_cast<UINT>(std::size(input)), input, sizeof(INPUT)) == std::size(input);
        setStatus(inserted ? tr("Перевод вставлен") : tr("Вставка недоступна. Используй «Копировать»."));
    });
#endif
}

void CompactWindow::minimizeToTray()
{
    hide();
    m_trayIcon->show();
}

void CompactWindow::restoreFromTray()
{
    m_restoringFromTray = true;
    setWindowState(Qt::WindowNoState);
    const QRect available = screen() ? screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
    if (!available.contains(frameGeometry()))
        move(available.center() - rect().center());
    setVisible(true);
    raise();
    activateWindow();
    QTimer::singleShot(0, this, [this] {
        setWindowState(Qt::WindowActive);
        setVisible(true);
        raise();
        activateWindow();
        m_restoringFromTray = false;
        registerHotkey();
    });
}

void CompactWindow::registerHotkey()
{
    const QKeySequence shortcut(QStringLiteral("Ctrl+Alt+E"));
    if (m_hotkey->shortcut() != shortcut)
        m_hotkey->setShortcut(shortcut, false);
    if (!m_hotkey->isRegistered())
        m_hotkey->setRegistered(true);

    m_trayIcon->setToolTip(m_hotkey->isRegistered()
                               ? tr("UNO echo · Ctrl+Alt+E")
                               : tr("UNO echo · Ctrl+Alt+E занято"));
    if (!m_hotkey->isRegistered()) setStatus(tr("Ctrl+Alt+E занято другой программой. Освободи сочетание и открой UNO echo снова."));
}

void CompactWindow::setStatus(const QString &text)
{
    m_status->setText(text);
}

void CompactWindow::saveUiState() const
{
    if (qApp->arguments().contains(QStringLiteral("--preview"))) return;
    QSettings settings;
    settings.setValue(QStringLiteral("Compact/Language"), m_languageCombo->currentIndex());
    settings.setValue(QStringLiteral("Compact/Engine"), m_engineCombo->currentData());
}
