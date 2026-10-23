/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compactwindow.h"

#include "qhotkey.h"
#include "selection.h"
#include "texttools.h"
#include "inputlayout.h"

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
#include <QKeySequenceEdit>
#include <QFormLayout>
#include <QMimeData>
#include <memory>
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
    , m_layoutHotkey(new QHotkey(this))
    , m_wordHotkey(new QHotkey(this))
    , m_caseHotkey(new QHotkey(this))
    , m_caseBreakHotkey(new QHotkey(this))
    , m_shortcutLabel(new QLabel(this))
    , m_toolsLabel(new QLabel(this))
    , m_caseLabel(new QLabel(this))
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
    auto *shortcut = m_shortcutLabel;
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
    signature->setSpacing(12);
    for (auto *hint : {m_toolsLabel, m_caseLabel}) {
        hint->setObjectName(QStringLiteral("toolsHint"));
        hint->setWordWrap(true);
        hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    m_caseLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    signature->addWidget(m_toolsLabel, 1);
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
    signature->addWidget(m_caseLabel, 1);
    layout->addLayout(signature);

    QSettings settings;
    m_translationShortcut = QKeySequence(settings.value(QStringLiteral("Compact/ShortcutTranslate"), QStringLiteral("Ctrl+Alt+E")).toString());
    m_layoutShortcut = QKeySequence(settings.value(QStringLiteral("Compact/ShortcutLayout"), QStringLiteral("Shift+Pause")).toString());
    m_wordShortcut = QKeySequence(settings.value(QStringLiteral("Compact/ShortcutWord"), QStringLiteral("Pause")).toString());
    m_caseShortcut = QKeySequence(settings.value(QStringLiteral("Compact/ShortcutCase"), QStringLiteral("Ctrl+Pause")).toString());
    updateShortcutLabels();
    m_languageCombo->setCurrentIndex(qBound(0, settings.value(QStringLiteral("Compact/Language"), 0).toInt(), m_languageCombo->count() - 1));
    const int savedEngine = settings.value(QStringLiteral("Compact/Engine"), static_cast<int>(OnlineTranslator::Google)).toInt();
    m_engineCombo->setCurrentIndex(qMax(0, m_engineCombo->findData(savedEngine)));
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    move(available.center() - rect().center());

    const bool validServer = m_translator->setInstance(settings.value(QStringLiteral("Compact/MozhiUrl"), OnlineTranslator::defaultInstance()).toString());

    connect(m_hotkey, &QHotkey::activated, this, [this] { requestSelection(); });
    connect(m_layoutHotkey, &QHotkey::activated, this, [this] { requestSelection(SelectionAction::Layout); });
    connect(m_wordHotkey, &QHotkey::activated, this, [this] { requestSelection(SelectionAction::WordLayout); });
    connect(m_caseHotkey, &QHotkey::activated, this, [this] { requestSelection(SelectionAction::Case); });
    connect(m_caseBreakHotkey, &QHotkey::activated, this, [this] { requestSelection(SelectionAction::Case); });
    connect(&Selection::instance(), &Selection::requestedSelectionAvailable, this, &CompactWindow::selectionAvailable);
    connect(m_translator, &OnlineTranslator::finished, this, &CompactWindow::translationFinished);
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this] { requestTranslation(); });
    connect(m_engineCombo, &QComboBox::currentIndexChanged, this, [this] { requestTranslation(); });
    connect(m_insertButton, &QPushButton::clicked, this, [this] { insertTranslation(); });
    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_translationText);
        setStatus(tr("Результат скопирован"));
    });

    auto *showAction = m_trayMenu->addAction(tr("Показать UNO echo"));
    auto *aboutAction = m_trayMenu->addAction(tr("О программе"));
    auto *serverAction = m_trayMenu->addAction(tr("Сервер перевода…"));
    auto *shortcutsAction = m_trayMenu->addAction(tr("Сочетания клавиш…"));
    connect(shortcutsAction, &QAction::triggered, this, &CompactWindow::showShortcutSettings);
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
    dialog->resize(540, 470);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    auto *text = new QTextBrowser(dialog);
    text->setOpenExternalLinks(true);
    text->document()->setDefaultStyleSheet(QStringLiteral("a { color: #b49af0; }"));
    text->setHtml(tr(
        "<h2>UNO echo %1</h2>"
        "<p>Перевод, раскладка и регистр для Windows.<br>Разработка и адаптация — <b>Arachna</b>.</p>"
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
    auto *shortcuts = buttons->addButton(tr("Клавиши…"), QDialogButtonBox::ActionRole);
    shortcuts->setObjectName(QStringLiteral("shortcutSettingsButton"));
    connect(shortcuts, &QPushButton::clicked, this, &CompactWindow::showShortcutSettings);
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
        if (m_waitingForSelection || m_inserting) {
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
    for (auto *key : {m_hotkey, m_layoutHotkey, m_caseHotkey, m_caseBreakHotkey, m_wordHotkey}) key->setRegistered(false);
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

void CompactWindow::requestSelection(SelectionAction action)
{
    if (m_waitingForSelection || m_inserting) return;
#ifdef Q_OS_WIN
    const HWND target = GetForegroundWindow();
    DWORD process = 0;
    GetWindowThreadProcessId(target, &process);
    if (process == GetCurrentProcessId()) return;
    prepareCapture(action, reinterpret_cast<quintptr>(target));
#else
    prepareCapture(action, 0);
#endif
    Selection::instance().requestSelection(action == SelectionAction::WordLayout);
}

void CompactWindow::prepareCapture(SelectionAction action, quintptr targetWindow)
{
    m_captureTarget = targetWindow;
    m_captureAction = action;
    m_waitingForSelection = true;
    if (action != SelectionAction::Translate) return;
    m_targetWindow = m_captureTarget;
    ++m_revision;
    m_pending.valid = false;
    if (m_translator->isRunning()) m_translator->abort();
    m_sourceText.clear();
    m_translationText.clear();
    m_insertButton->setEnabled(false);
    m_copyButton->setEnabled(false);
    m_translationEdit->clear();
    setStatus(tr("Получаю выделенный текст…"));
}

void CompactWindow::selectionAvailable(const QString &text)
{
    if (!m_waitingForSelection)
        return;
    m_waitingForSelection = false;
    if (m_captureAction != SelectionAction::Translate) {
        // Background corrections must never replace the open translation or its target.
        if (text.isEmpty()) return;
        const QString corrected = m_captureAction == SelectionAction::Case
            ? TextTools::invertCase(text) : TextTools::switchLayout(text);
        if (corrected != text) pasteText(corrected, m_captureTarget, true,
            m_captureAction == SelectionAction::Case ? TextTools::LayoutLanguage::Unchanged
                                                    : TextTools::layoutDestination(text));
        return;
    }
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
    pasteText(m_translationText, m_targetWindow, false);
}

void CompactWindow::pasteText(const QString &text, quintptr targetWindow, bool correction,
                             TextTools::LayoutLanguage destination)
{
#ifdef Q_OS_WIN
    const auto target = reinterpret_cast<HWND>(targetWindow);
    if (target == nullptr || !IsWindow(target) || text.isEmpty())
        return;
    if (m_inserting) return;
    if (correction && GetForegroundWindow() != target) return;
    if (!correction && !SetForegroundWindow(target)) {
        setStatus(tr("Не удалось вернуть фокус. Скопируй перевод вручную."));
        return;
    }
    const QString translation = text;
    const quint64 revision = m_revision;
    m_inserting = true;
    QTimer::singleShot(100, this, [this, target, translation, revision, correction, destination] {
        if (!correction && revision != m_revision) { m_inserting = false; return; }
        if (!IsWindow(target) || GetForegroundWindow() != target) {
            m_inserting = false;
            if (!correction) setStatus(tr("Окно переключилось — вставка отменена."));
            return;
        }
        auto original = std::make_shared<QMimeData>();
        if (correction) {
            const auto *data = QApplication::clipboard()->mimeData();
            if (data) for (const auto &format : data->formats()) original->setData(format, data->data(format));
        }
        QApplication::clipboard()->setText(translation);
        const DWORD pasteSequence = GetClipboardSequenceNumber();
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
        if (inserted && correction && destination != TextTools::LayoutLanguage::Unchanged)
            InputLayout::requestForForegroundWindow(reinterpret_cast<quintptr>(target), destination);
        if (!correction) setStatus(inserted ? tr("Результат вставлен") : tr("Вставка недоступна. Используй «Копировать»."));
        if (correction) {
            QTimer::singleShot(500, this, [this, original, pasteSequence] {
                m_inserting = false;
                if (GetClipboardSequenceNumber() != pasteSequence) return;
                auto *restored = new QMimeData;
                for (const auto &format : original->formats()) restored->setData(format, original->data(format));
                QApplication::clipboard()->setMimeData(restored);
            });
        } else {
            m_inserting = false;
        }
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

void CompactWindow::updateShortcutLabels()
{
    m_shortcutLabel->setText(m_translationShortcut.toString(QKeySequence::NativeText));
    m_toolsLabel->setText(tr("Раскладка: %1\nСлово: %2").arg(m_layoutShortcut.toString(QKeySequence::NativeText), m_wordShortcut.toString(QKeySequence::NativeText)));
    m_caseLabel->setText(tr("Регистр: %1").arg(m_caseShortcut.toString(QKeySequence::NativeText)));
    m_translationEdit->setPlaceholderText(tr("Выдели текст в любом приложении\nи нажми %1 для перевода")
        .arg(m_translationShortcut.toString(QKeySequence::NativeText)));
}

void CompactWindow::registerHotkey()
{
    if (qApp->arguments().contains(QStringLiteral("--preview"))) return;
    const auto apply = [](QHotkey *key, const QKeySequence &shortcut) {
        if (key->shortcut() != shortcut) key->setShortcut(shortcut, false);
        if (!key->isRegistered()) key->setRegistered(true);
    };
    apply(m_hotkey, m_translationShortcut);
    apply(m_layoutHotkey, m_layoutShortcut);
    apply(m_wordHotkey, m_wordShortcut);
    apply(m_caseHotkey, m_caseShortcut);
    // On Windows Ctrl+Pause can arrive as Ctrl+Break (VK_CANCEL).
    if (m_caseShortcut == QKeySequence(QStringLiteral("Ctrl+Pause")))
        apply(m_caseBreakHotkey, QKeySequence(QStringLiteral("Ctrl+Cancel")));
    else
        m_caseBreakHotkey->setRegistered(false);
    QStringList unavailable;
    if (!m_hotkey->isRegistered()) unavailable << m_translationShortcut.toString();
    if (!m_layoutHotkey->isRegistered()) unavailable << m_layoutShortcut.toString();
    if (!m_wordHotkey->isRegistered()) unavailable << m_wordShortcut.toString();
    if (!m_caseHotkey->isRegistered() || (m_caseShortcut == QKeySequence(QStringLiteral("Ctrl+Pause")) && !m_caseBreakHotkey->isRegistered())) unavailable << m_caseShortcut.toString();
    m_trayIcon->setToolTip(tr("UNO echo · %1").arg(m_translationShortcut.toString()));
    if (!unavailable.isEmpty())
        setStatus(tr("Не удалось включить сочетания: %1. Проверь «Клавиши…» и другие переключатели раскладки.").arg(unavailable.join(QStringLiteral(", "))));
}

void CompactWindow::showShortcutSettings()
{
    if (auto *existing = findChild<QDialog *>(QStringLiteral("shortcutDialog"))) {
        existing->show(); existing->raise(); existing->activateWindow(); return;
    }
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("shortcutDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Сочетания клавиш"));
    dialog->setWindowFlag(Qt::WindowStaysOnTopHint);
    dialog->resize(510, 390);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    auto *hint = new QLabel(tr("Для фрагмента выдели текст. Для одного слова оставь курсор сразу после него, до пробела. Раскладка и регистр исправляются на месте, без интернета. Перевод открывается в echo."), dialog);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *form = new QFormLayout;
    auto *translation = new QKeySequenceEdit(m_translationShortcut, dialog);
    auto *layoutKey = new QKeySequenceEdit(m_layoutShortcut, dialog);
    auto *caseKey = new QKeySequenceEdit(m_caseShortcut, dialog);
    auto *wordKey = new QKeySequenceEdit(m_wordShortcut, dialog);
    for (auto *editor : {translation, layoutKey, caseKey, wordKey}) editor->setMaximumSequenceLength(1);
    translation->setObjectName(QStringLiteral("translateShortcut"));
    layoutKey->setObjectName(QStringLiteral("layoutShortcut"));
    caseKey->setObjectName(QStringLiteral("caseShortcut"));
    wordKey->setObjectName(QStringLiteral("wordShortcut"));
    form->addRow(tr("Перевести"), translation); form->addRow(tr("Исправить раскладку"), layoutKey); form->addRow(tr("Инвертировать регистр"), caseKey);
    form->addRow(tr("Раскладка одного слова"), wordKey);
    layout->addLayout(form);
    auto *error = new QLabel(dialog); error->setObjectName(QStringLiteral("serverError")); error->setWordWrap(true); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Сохранить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    auto *reset = buttons->addButton(tr("По умолчанию"), QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, dialog, [=] {
        translation->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Alt+E")));
        layoutKey->setKeySequence(QKeySequence(QStringLiteral("Shift+Pause")));
        caseKey->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Pause")));
        wordKey->setKeySequence(QKeySequence(QStringLiteral("Pause")));
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [=] {
        if (m_waitingForSelection || m_inserting) { error->setText(tr("Дождись завершения исправления текста.")); return; }
        const auto normalize = [](QKeySequence key) {
            return key == QKeySequence(QStringLiteral("Ctrl+Cancel")) ? QKeySequence(QStringLiteral("Ctrl+Pause")) : key;
        };
        const QKeySequence keys[] = {normalize(translation->keySequence()), normalize(layoutKey->keySequence()), normalize(caseKey->keySequence()), normalize(wordKey->keySequence())};
        for (const auto &key : keys) {
            if (key.count() != 1 || (key != QKeySequence(QStringLiteral("Pause")) && !(key[0].keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier)))) {
                error->setText(tr("Нужно одно сочетание с Ctrl, Alt, Shift или Win; отдельно можно Pause.")); return;
            }
        }
        for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j) {
            if (keys[i] == keys[j]) { error->setText(tr("Для каждого действия нужно отдельное сочетание.")); return; }
        }
        const QKeySequence previous[] = {m_translationShortcut, m_layoutShortcut, m_caseShortcut, m_wordShortcut};
        const auto releaseKeys = [this] { for (auto *key : {m_hotkey, m_layoutHotkey, m_caseHotkey, m_caseBreakHotkey, m_wordHotkey}) key->setRegistered(false); };
        releaseKeys();
        m_translationShortcut = keys[0]; m_layoutShortcut = keys[1]; m_caseShortcut = keys[2]; m_wordShortcut = keys[3];
        registerHotkey();
        const bool preview = qApp->arguments().contains(QStringLiteral("--preview"));
        if (!preview && (!m_hotkey->isRegistered() || !m_layoutHotkey->isRegistered() || !m_wordHotkey->isRegistered() || (!m_caseHotkey->isRegistered() || (m_caseShortcut == QKeySequence(QStringLiteral("Ctrl+Pause")) && !m_caseBreakHotkey->isRegistered())))) {
            releaseKeys();
            m_translationShortcut = previous[0]; m_layoutShortcut = previous[1]; m_caseShortcut = previous[2]; m_wordShortcut = previous[3];
            registerHotkey(); error->setText(tr("Сочетание занято или недоступно. Выбери другое.")); return;
        }
        if (!preview) {
            QSettings settings;
            const QString names[] = {QStringLiteral("Compact/ShortcutTranslate"), QStringLiteral("Compact/ShortcutLayout"), QStringLiteral("Compact/ShortcutCase"), QStringLiteral("Compact/ShortcutWord")};
            for (int i = 0; i < 4; ++i) settings.setValue(names[i], keys[i].toString(QKeySequence::PortableText));
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                releaseKeys();
                m_translationShortcut = previous[0]; m_layoutShortcut = previous[1]; m_caseShortcut = previous[2]; m_wordShortcut = previous[3];
                for (int i = 0; i < 4; ++i) settings.setValue(names[i], previous[i].toString(QKeySequence::PortableText));
                settings.sync(); registerHotkey();
                error->setText(tr("Не удалось сохранить настройки.")); return;
            }
        }
        updateShortcutLabels(); setStatus(tr("Сочетания сохранены")); dialog->close();
    });
    layout->addWidget(buttons);
    dialog->show();
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
