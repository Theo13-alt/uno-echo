/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "onlinetranslator.h"

#include <QFinalState>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QState>
#include <QStateMachine>
#include <QMetaEnum>
#include <QUrl>

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_genericLanguageCodes = {
    {Auto, QStringLiteral("auto")},
    {Afrikaans, QStringLiteral("af")},
    {Albanian, QStringLiteral("sq")},
    {Amharic, QStringLiteral("am")},
    {Arabic, QStringLiteral("ar")},
    {Armenian, QStringLiteral("hy")},
    {Assamese, QStringLiteral("as")},
    {Aymara, QStringLiteral("ay")},
    {Azerbaijani, QStringLiteral("az")},
    {Bajan, QStringLiteral("bjs")},
    {BalkanGipsy, QStringLiteral("rm")},
    {Bambara, QStringLiteral("bm")},
    {Bangla, QStringLiteral("bn")},
    {Bashkir, QStringLiteral("ba")},
    {Basque, QStringLiteral("eu")},
    {Belarusian, QStringLiteral("be")},
    {Bemba, QStringLiteral("bem")},
    {Bhojpuri, QStringLiteral("bho")},
    {Bislama, QStringLiteral("bi")},
    {Bosnian, QStringLiteral("bs")},
    {Breton, QStringLiteral("br")},
    {Bulgarian, QStringLiteral("bg")},
    {Cantonese, QStringLiteral("yue")},
    {Catalan, QStringLiteral("ca")},
    {Cebuano, QStringLiteral("ceb")},
    {Chamorro, QStringLiteral("ch")},
    {Chichewa, QStringLiteral("ny")},
    {ChineseLiterary, QStringLiteral("lzh")},
    {ChineseSimplified, QStringLiteral("zh")},
    {ChineseTraditional, QStringLiteral("zh-TW")},
    {Chuvash, QStringLiteral("cv")},
    {Comorian, QStringLiteral("zdj")},
    {Coptic, QStringLiteral("cop")},
    {Corsican, QStringLiteral("co")},
    {AntiguanCreole, QStringLiteral("aig")},
    {BahamianCreole, QStringLiteral("bah")},
    {GrenadianCreole, QStringLiteral("gcl")},
    {GuyaneseCreole, QStringLiteral("gyn")},
    {JamaicanCreole, QStringLiteral("jam")},
    {VincentianCreole, QStringLiteral("svc")},
    {VirginIslandsCreole, QStringLiteral("vic")},
    {SaintLucianCreole, QStringLiteral("acf")},
    {SeselwaCreole, QStringLiteral("crs")},
    {UpperGuineaCreole, QStringLiteral("pov")},
    {Croatian, QStringLiteral("hr")},
    {Czech, QStringLiteral("cs")},
    {Danish, QStringLiteral("da")},
    {Dari, QStringLiteral("prs")},
    {Divehi, QStringLiteral("dv")},
    {Dogri, QStringLiteral("doi")},
    {Dutch, QStringLiteral("nl")},
    {Dzongkha, QStringLiteral("dz")},
    {Elvish, QStringLiteral("sjn")},
    {Emoji, QStringLiteral("emj")},
    {English, QStringLiteral("en")},
    {Esperanto, QStringLiteral("eo")},
    {Estonian, QStringLiteral("et")},
    {Ewe, QStringLiteral("ee")},
    {Fanagalo, QStringLiteral("fn")},
    {Faroese, QStringLiteral("fo")},
    {Fijian, QStringLiteral("fj")},
    {Filipino, QStringLiteral("tl")},
    {Finnish, QStringLiteral("fi")},
    {French, QStringLiteral("fr")},
    {FrenchCanada, QStringLiteral("fr-CA")},
    {Frisian, QStringLiteral("fy")},
    {Galician, QStringLiteral("gl")},
    {Ganda, QStringLiteral("lug")},
    {Georgian, QStringLiteral("ka")},
    {German, QStringLiteral("de")},
    {Greek, QStringLiteral("el")},
    {GreekClassical, QStringLiteral("grc")},
    {Guarani, QStringLiteral("gn")},
    {Gujarati, QStringLiteral("gu")},
    {HaitianCreole, QStringLiteral("ht")},
    {Hausa, QStringLiteral("ha")},
    {Hawaiian, QStringLiteral("haw")},
    {Hebrew, QStringLiteral("he")},
    {HillMari, QStringLiteral("mrj")},
    {Hindi, QStringLiteral("hi")},
    {Hmong, QStringLiteral("hmn")},
    {HmongDaw, QStringLiteral("mww")},
    {Hungarian, QStringLiteral("hu")},
    {Icelandic, QStringLiteral("is")},
    {Igbo, QStringLiteral("ig")},
    {Ilocano, QStringLiteral("ilo")},
    {Indonesian, QStringLiteral("id")},
    {Inuinnaqtun, QStringLiteral("ikt")},
    {Inuktitut, QStringLiteral("iu")},
    {InuktitutGreenlandic, QStringLiteral("kl")},
    {InuktitutLatin, QStringLiteral("iu-Latn")},
    {Irish, QStringLiteral("ga")},
    {Italian, QStringLiteral("it")},
    {Japanese, QStringLiteral("ja")},
    {Javanese, QStringLiteral("jw")},
    {Kabuverdianu, QStringLiteral("kea")},
    {Kabylian, QStringLiteral("kab")},
    {Kannada, QStringLiteral("kn")},
    {Kazakh, QStringLiteral("kk")},
    {KazakhLatin, QStringLiteral("kazlat")},
    {Khmer, QStringLiteral("km")},
    {Kinyarwanda, QStringLiteral("rw")},
    {Kirundi, QStringLiteral("rn")},
    {Klingon, QStringLiteral("tlh-Latn")},
    {Konkani, QStringLiteral("gom")},
    {Korean, QStringLiteral("ko")},
    {Krio, QStringLiteral("kri")},
    {KurdishCentral, QStringLiteral("ku")},
    {KurdishNorthern, QStringLiteral("kmr")},
    {KurdishSorani, QStringLiteral("ckb")},
    {Kyrgyz, QStringLiteral("ky")},
    {Lao, QStringLiteral("lo")},
    {Latin, QStringLiteral("la")},
    {Latvian, QStringLiteral("lv")},
    {Lingala, QStringLiteral("ln")},
    {Lithuanian, QStringLiteral("lt")},
    {LowerSorbian, QStringLiteral("dsb")},
    {Luganda, QStringLiteral("lg")},
    {Luxembourgish, QStringLiteral("lb")},
    {Macedonian, QStringLiteral("mk")},
    {Maithili, QStringLiteral("mai")},
    {Malagasy, QStringLiteral("mg")},
    {Malay, QStringLiteral("ms")},
    {Malayalam, QStringLiteral("ml")},
    {Maltese, QStringLiteral("mt")},
    {ManxGaelic, QStringLiteral("gv")},
    {Marathi, QStringLiteral("mr")},
    {Mari, QStringLiteral("mhr")},
    {Marshallese, QStringLiteral("mh")},
    {Meiteilon, QStringLiteral("mni-Mtei")},
    {Mende, QStringLiteral("men")},
    {Mizo, QStringLiteral("lus")},
    {Mongolian, QStringLiteral("mn")},
    {MongolianCyrillic, QStringLiteral("mn-Cyrl")},
    {MongolianTraditional, QStringLiteral("mn-Mong")},
    {Morisyen, QStringLiteral("mfe")},
    {Myanmar, QStringLiteral("my")},
    {Maori, QStringLiteral("mi")},
    {Nepali, QStringLiteral("ne")},
    {Niuean, QStringLiteral("niu")},
    {Norwegian, QStringLiteral("no")},
    {Nyanja, QStringLiteral("nya")},
    {Odia, QStringLiteral("or")},
    {Oromo, QStringLiteral("om")},
    {Palauan, QStringLiteral("pau")},
    {Papiamentu, QStringLiteral("pap")},
    {Pashto, QStringLiteral("ps")},
    {Persian, QStringLiteral("fa")},
    {Pijin, QStringLiteral("pis")},
    {Polish, QStringLiteral("pl")},
    {PortugueseBrazilian, QStringLiteral("pt")},
    {PortuguesePortugal, QStringLiteral("pt-PT")},
    {Potawatomi, QStringLiteral("pot")},
    {Punjabi, QStringLiteral("pa")},
    {Quechua, QStringLiteral("qu")},
    {QueretaroOtomi, QStringLiteral("otq")},
    {Romanian, QStringLiteral("ro")},
    {Rundi, QStringLiteral("run")},
    {Russian, QStringLiteral("ru")},
    {Samoan, QStringLiteral("sm")},
    {Sango, QStringLiteral("sg")},
    {Sanskrit, QStringLiteral("sa")},
    {ScotsGaelic, QStringLiteral("gd")},
    {SerbianCyrillic, QStringLiteral("sr-Cyrl")},
    {SerbianLatin, QStringLiteral("sr")},
    {Sesotho, QStringLiteral("st")},
    {SesothoSaLeboa, QStringLiteral("nso")},
    {Setswana, QStringLiteral("tn")},
    {Shona, QStringLiteral("sn")},
    {Sindhi, QStringLiteral("sd")},
    {Sinhala, QStringLiteral("si")},
    {Slovak, QStringLiteral("sk")},
    {Slovenian, QStringLiteral("sl")},
    {Somali, QStringLiteral("so")},
    {Spanish, QStringLiteral("es")},
    {SrananTongo, QStringLiteral("srn")},
    {Sundanese, QStringLiteral("su")},
    {Swahili, QStringLiteral("sw")},
    {Swedish, QStringLiteral("sv")},
    {Syriac, QStringLiteral("syc")},
    {Tahitian, QStringLiteral("ty")},
    {Tajik, QStringLiteral("tg")},
    {Tamashek, QStringLiteral("tmh")},
    {Tamil, QStringLiteral("ta")},
    {Tatar, QStringLiteral("tt")},
    {Telugu, QStringLiteral("te")},
    {Tetum, QStringLiteral("tet")},
    {Thai, QStringLiteral("th")},
    {Tibetan, QStringLiteral("bo")},
    {Tigrinya, QStringLiteral("ti")},
    {TokPisin, QStringLiteral("tpi")},
    {Tokelauan, QStringLiteral("tkl")},
    {Tongan, QStringLiteral("to")},
    {Tsonga, QStringLiteral("ts")},
    {Turkish, QStringLiteral("tr")},
    {Turkmen, QStringLiteral("tk")},
    {Tuvaluan, QStringLiteral("tvl")},
    {Twi, QStringLiteral("ak")},
    {Udmurt, QStringLiteral("udm")},
    {Ukrainian, QStringLiteral("uk")},
    {Uma, QStringLiteral("ppk")},
    {UpperSorbian, QStringLiteral("hsb")},
    {Urdu, QStringLiteral("ur")},
    {Uyghur, QStringLiteral("ug")},
    {UzbekCyrillic, QStringLiteral("uzbcyr")},
    {UzbekLatin, QStringLiteral("uz")},
    {Vietnamese, QStringLiteral("vi")},
    {Wallisian, QStringLiteral("wls")},
    {Welsh, QStringLiteral("cy")},
    {Wolof, QStringLiteral("wo")},
    {Xhosa, QStringLiteral("xh")},
    {Yakut, QStringLiteral("sah")},
    {Yiddish, QStringLiteral("yi")},
    {Yoruba, QStringLiteral("yo")},
    {YucatecMaya, QStringLiteral("yua")},
    {Zulu, QStringLiteral("zu")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_googleLanguageCodes = {
    {ChineseSimplified, QStringLiteral("zh-CN")},
    {Hebrew, QStringLiteral("iw")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_yandexLanguageCodes = {
    {SerbianCyrillic, QStringLiteral("sr")},
    {SerbianLatin, QStringLiteral("sr-Latn")},
    {PortugueseBrazilian, QStringLiteral("pt-BR")},
    {PortuguesePortugal, QStringLiteral("pt")},
    {Javanese, QStringLiteral("jv")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_ddgLanguageCodes = {
    {SerbianLatin, QStringLiteral("sr-Latn")},
    {Filipino, QStringLiteral("fil")},
    {SerbianCyrillic, QStringLiteral("sr-Cyrl")},
    {Hmong, QStringLiteral("mww")}};

OnlineTranslator::OnlineTranslator(QObject *parent)
    : QObject(parent)
    , m_stateMachine(new QStateMachine(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_stateMachine, &QStateMachine::finished, this, &OnlineTranslator::finished);
    connect(m_stateMachine, &QStateMachine::stopped, this, &OnlineTranslator::finished);
}

void OnlineTranslator::translate(const QString &text, Engine engine, Language translationLang, Language sourceLang)
{
    abort();

    resetData();
    m_source = text;
    m_sourceLang = sourceLang;
    m_translationLang = translationLang;
    m_engine = engine;

    if (m_instance.isEmpty()) {
        resetData(InstanceError, tr("Укажите корректный адрес сервера Mozhi в настройках"));
        emit finished();
        return;
    }

    buildStateMachine();
    m_stateMachine->start();
}

void OnlineTranslator::abort()
{
    if (m_currentReply != nullptr) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // Force stop the state machine
    if (m_stateMachine->isRunning()) {
        m_stateMachine->stop();
    }

    // Reset to ensure clean state
    resetData(NetworkError, tr("Operation aborted by user"));
}

bool OnlineTranslator::isRunning() const
{
    return m_stateMachine->isRunning();
}

const QString &OnlineTranslator::translation() const
{
    return m_translation;
}

OnlineTranslator::TranslationError OnlineTranslator::error() const
{
    return m_error;
}

const QString &OnlineTranslator::errorString() const
{
    return m_errorString;
}

QString OnlineTranslator::defaultInstance()
{
    return QStringLiteral("https://mozhi.adminforge.de");
}

QString OnlineTranslator::normalizedInstanceUrl(const QString &input)
{
    const QUrl url(input.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")) ||
        !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) return {};
    QString normalized = url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
    while (normalized.endsWith('/')) normalized.chop(1);
    if (QUrl(normalized).path().endsWith(QStringLiteral("/api/translate"))) return {};
    return normalized;
}

bool OnlineTranslator::setInstance(const QString &url)
{
    const QString normalized = normalizedInstanceUrl(url);
    if (normalized.isEmpty()) return false;
    m_instance = normalized;
    return true;
}

const QString &OnlineTranslator::instance() const
{
    return m_instance;
}

QByteArray OnlineTranslator::requestBody(const QString &text, Language sourceLang) const
{
    // Encode values individually: a literal '+' must be %2B in a form body.
    const QString engine = QString::fromLatin1(QMetaEnum::fromType<Engine>().valueToKey(m_engine)).toLower();
    return "engine=" + QUrl::toPercentEncoding(engine)
        + "&from=" + QUrl::toPercentEncoding(languageApiCode(m_engine, sourceLang))
        + "&to=" + QUrl::toPercentEncoding(languageApiCode(m_engine, m_translationLang))
        + "&text=" + QUrl::toPercentEncoding(text);
}

int OnlineTranslator::requestTextBudget() const
{
    int longestSourceCode = languageApiCode(m_engine, m_sourceLang).size();
    for (auto lang = s_genericLanguageCodes.cbegin(); lang != s_genericLanguageCodes.cend(); ++lang)
        longestSourceCode = qMax(longestSourceCode, int(languageApiCode(m_engine, lang.key()).size()));
    // Auto detection can replace "auto" with a longer code on subsequent chunks.
    const int extraSourceBytes = longestSourceCode - languageApiCode(m_engine, m_sourceLang).size();
    return s_requestBodyLimit - requestBody(QString(), m_sourceLang).size() - extraSourceBytes;
}

void OnlineTranslator::requestTranslate()
{
    const QString sourceText = sender()->property(s_textProperty).toString();
    const QByteArray body = requestBody(sourceText, m_sourceLang);
    if (body.size() > s_requestBodyLimit) {
        resetData(InstanceError, tr("Текст слишком длинный для одного запроса"));
        return;
    }

    QNetworkRequest request(QUrl(m_instance + '/').resolved(QUrl(QStringLiteral("api/translate"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(15000);
    // Disable HTTP/2 to prevent connection errors with some servers
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    m_currentReply = m_networkManager->post(request, body);
}

void OnlineTranslator::parseTranslate()
{
    m_currentReply->deleteLater();

    // Check for error
    if (m_currentReply->error() != QNetworkReply::NoError) {
        const int status = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = status == 500 && m_currentReply->isReadable()
            ? m_currentReply->read(512).trimmed() : QByteArray();
        if (status == 414 || status == 431)
            resetData(InstanceError, tr("Сервис отклонил слишком длинный запрос (HTTP %1)").arg(status));
        else if (status == 429 || (status == 500 && body.toLower().startsWith("instance has been rate limited")))
            resetData(InstanceError, tr("Сервис временно ограничил частоту запросов. Попробуйте позже."));
        else if (status >= 400)
            resetData(InstanceError, tr("Сервис перевода вернул ошибку (HTTP %1). Попробуйте позже или выберите другой сервис.").arg(status));
        else if (m_currentReply->error() == QNetworkReply::TimeoutError)
            resetData(NetworkError, tr("Сервис перевода не ответил вовремя"));
        else
            resetData(NetworkError, tr("Не удалось связаться с сервисом перевода"));
        return;
    }

    // Read Json
    const QJsonObject jsonData = QJsonDocument::fromJson(m_currentReply->readAll()).object();

    if (m_sourceLang == Auto) {
        // Parse language
        m_sourceLang = language(m_engine, jsonData.value(QStringLiteral("detected")).toString());
        if (m_sourceLang == NoLanguage) {
            resetData(ParsingError, tr("Error: Unable to parse autodetected language"));
            return;
        }
    }

    addSpaceBetweenParts(m_translation);
    m_translation.append(jsonData.value(QStringLiteral("translated-text")).toString());
}

void OnlineTranslator::buildStateMachine()
{
    // States (sends a request that will be split into several by the translation limit)
    auto *translationState = new QState(m_stateMachine);
    auto *finalState = new QFinalState(m_stateMachine);
    m_stateMachine->setInitialState(translationState);

    translationState->addTransition(translationState, &QState::finished, finalState);

    // Setup translation state
    buildSplitNetworkRequest(translationState, &OnlineTranslator::requestTranslate, &OnlineTranslator::parseTranslate, m_source, requestTextBudget());
}

void OnlineTranslator::buildSplitNetworkRequest(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text, int textLimit)
{
    QString unsendedText = text;
    auto *nextTranslationState = new QState(parent);
    parent->setInitialState(nextTranslationState);

    while (!unsendedText.isEmpty()) {
        auto *currentTranslationState = nextTranslationState;
        nextTranslationState = new QState(parent);

        const int splitIndex = getSplitIndex(unsendedText, textLimit);
        buildNetworkRequestState(currentTranslationState, requestMethod, parseMethod, unsendedText.left(splitIndex));
        currentTranslationState->addTransition(currentTranslationState, &QState::finished, nextTranslationState);
        unsendedText = unsendedText.mid(splitIndex);
    }

    nextTranslationState->addTransition(new QFinalState(parent));
}

void OnlineTranslator::buildNetworkRequestState(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text)
{
    // Network substates
    auto *requestingState = new QState(parent);
    auto *parsingState = new QState(parent);

    parent->setInitialState(requestingState);

    // Substates transitions
    requestingState->addTransition(m_networkManager, &QNetworkAccessManager::finished, parsingState);
    parsingState->addTransition(new QFinalState(parent));

    // Setup requesting state
    requestingState->setProperty(s_textProperty, text);
    connect(requestingState, &QState::entered, this, requestMethod);

    // Setup parsing state
    connect(parsingState, &QState::entered, this, parseMethod);
}

void OnlineTranslator::resetData(TranslationError error, const QString &errorString)
{
    m_error = error;
    m_errorString = errorString;
    m_translation.clear();

    m_stateMachine->stop();
    for (QAbstractState *state : m_stateMachine->findChildren<QAbstractState *>()) {
        if (!m_stateMachine->configuration().contains(state))
            state->deleteLater();
    }
}

// Returns engine-specific language code for translation.
QString OnlineTranslator::languageApiCode(Engine engine, Language lang)
{
    switch (engine) {
    case Google:
        return s_googleLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    case Yandex:
        return s_yandexLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    case Duckduckgo:
        return s_ddgLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    }

    Q_UNREACHABLE();
}

// Parse language from response language code
OnlineTranslator::Language OnlineTranslator::language(Engine engine, const QString &langCode)
{
    if (langCode == QStringLiteral("fil") || langCode == QStringLiteral("tl")) return Filipino;
    if (langCode == QStringLiteral("jv") || langCode == QStringLiteral("jw")) return Javanese;
    switch (engine) {
    case Google:
        return s_googleLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    case Yandex:
        return s_yandexLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    case Duckduckgo:
        return s_ddgLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    }

    Q_UNREACHABLE();
}

// Get split index of the text according to the limit
int OnlineTranslator::getSplitIndex(const QString &untranslatedText, int limit)
{
    int end = 0;
    int encodedBytes = 0;
    while (end < untranslatedText.size()) {
        const int width = untranslatedText.at(end).isHighSurrogate() && end + 1 < untranslatedText.size()
            && untranslatedText.at(end + 1).isLowSurrogate() ? 2 : 1;
        const int cost = QUrl::toPercentEncoding(untranslatedText.mid(end, width)).size();
        if (encodedBytes + cost > limit) break;
        encodedBytes += cost;
        end += width;
    }
    if (end == untranslatedText.size()) return end;

    int splitIndex = untranslatedText.lastIndexOf(QLatin1String(". "), end - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    splitIndex = untranslatedText.lastIndexOf(QChar::Space, end - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    splitIndex = untranslatedText.lastIndexOf(QChar::LineFeed, end - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    // Non-breaking space
    splitIndex = untranslatedText.lastIndexOf(QChar::Nbsp, end - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    return end;
}

void OnlineTranslator::addSpaceBetweenParts(QString &text)
{
    if (text.isEmpty())
        return;

    if (!text.back().isSpace()) {
        text.append(' ');
    }
}
