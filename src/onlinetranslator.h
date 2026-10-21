/* Modified for UNO echo by Arachna, 2026-10-20.
 * SPDX-FileCopyrightText: 2026 Arachna
 */
/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ONLINETRANSLATOR_H
#define ONLINETRANSLATOR_H

#include <QMap>
#include <QObject>
#include <QPointer>

class QStateMachine;
class QState;
class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

class OnlineTranslator : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OnlineTranslator)

public:
    // Source-language codes are needed to parse Mozhi's detected-language response.
    enum Language {
        NoLanguage = -1,
        Auto,
        Afrikaans,
        Albanian,
        Amharic,
        Arabic,
        Armenian,
        Assamese,
        Aymara,
        Azerbaijani,
        Bajan,
        BalkanGipsy,
        Bambara,
        Bangla,
        Bashkir,
        Basque,
        Belarusian,
        Bemba,
        Bhojpuri,
        Bislama,
        Bosnian,
        Breton,
        Bulgarian,
        Cantonese,
        Catalan,
        Cebuano,
        Chamorro,
        Chichewa,
        ChineseLiterary,
        ChineseSimplified,
        ChineseTraditional,
        Chuvash,
        Comorian,
        Coptic,
        Corsican,
        AntiguanCreole,
        BahamianCreole,
        GrenadianCreole,
        GuyaneseCreole,
        JamaicanCreole,
        VincentianCreole,
        VirginIslandsCreole,
        SaintLucianCreole,
        SeselwaCreole,
        UpperGuineaCreole,
        Croatian,
        Czech,
        Danish,
        Dari,
        Divehi,
        Dogri,
        Dutch,
        Dzongkha,
        Elvish,
        Emoji,
        English,
        Esperanto,
        Estonian,
        Ewe,
        Fanagalo,
        Faroese,
        Fijian,
        Filipino,
        Finnish,
        French,
        FrenchCanada,
        Frisian,
        Galician,
        Ganda,
        Georgian,
        German,
        Greek,
        GreekClassical,
        Guarani,
        Gujarati,
        HaitianCreole,
        Hausa,
        Hawaiian,
        Hebrew,
        HillMari,
        Hindi,
        Hmong,
        HmongDaw,
        Hungarian,
        Icelandic,
        Igbo,
        Ilocano,
        Indonesian,
        Inuinnaqtun,
        Inuktitut,
        InuktitutGreenlandic,
        InuktitutLatin,
        Irish,
        Italian,
        Japanese,
        Javanese,
        Kabuverdianu,
        Kabylian,
        Kannada,
        Kazakh,
        KazakhLatin,
        Khmer,
        Kinyarwanda,
        Kirundi,
        Klingon,
        Konkani,
        Korean,
        Krio,
        KurdishCentral,
        KurdishNorthern,
        KurdishSorani,
        Kyrgyz,
        Lao,
        Latin,
        Latvian,
        Lingala,
        Lithuanian,
        LowerSorbian,
        Luganda,
        Luxembourgish,
        Macedonian,
        Maithili,
        Malagasy,
        Malay,
        Malayalam,
        Maltese,
        ManxGaelic,
        Marathi,
        Mari,
        Marshallese,
        Meiteilon,
        Mende,
        Mizo,
        Mongolian,
        MongolianCyrillic,
        MongolianTraditional,
        Morisyen,
        Myanmar,
        Maori,
        Nepali,
        Niuean,
        Norwegian,
        Nyanja,
        Odia,
        Oromo,
        Palauan,
        Papiamentu,
        Pashto,
        Persian,
        Pijin,
        Polish,
        PortugueseBrazilian,
        PortuguesePortugal,
        Potawatomi,
        Punjabi,
        Quechua,
        QueretaroOtomi,
        Romanian,
        Rundi,
        Russian,
        Samoan,
        Sango,
        Sanskrit,
        ScotsGaelic,
        SerbianCyrillic,
        SerbianLatin,
        Sesotho,
        SesothoSaLeboa,
        Setswana,
        Shona,
        Sindhi,
        Sinhala,
        Slovak,
        Slovenian,
        Somali,
        Spanish,
        SrananTongo,
        Sundanese,
        Swahili,
        Swedish,
        Syriac,
        Tahitian,
        Tajik,
        Tamashek,
        Tamil,
        Tatar,
        Telugu,
        Tetum,
        Thai,
        Tibetan,
        Tigrinya,
        TokPisin,
        Tokelauan,
        Tongan,
        Tsonga,
        Turkish,
        Turkmen,
        Tuvaluan,
        Twi,
        Udmurt,
        Ukrainian,
        Uma,
        UpperSorbian,
        Urdu,
        Uyghur,
        UzbekCyrillic,
        UzbekLatin,
        Vietnamese,
        Wallisian,
        Welsh,
        Wolof,
        Xhosa,
        Yakut,
        Yiddish,
        Yoruba,
        YucatecMaya,
        Zulu,
    };
    Q_ENUM(Language)

    // Preserve the stored engine IDs used by existing profiles.
    enum Engine { Google = 0, Yandex = 1, Duckduckgo = 3 };
    Q_ENUM(Engine)

    enum TranslationError { NoError, NetworkError, InstanceError, ParsingError };

    explicit OnlineTranslator(QObject *parent = nullptr);
    void translate(const QString &text, Engine engine, Language translationLang, Language sourceLang = Auto);
    void abort();
    bool isRunning() const;
    const QString &translation() const;
    TranslationError error() const;
    const QString &errorString() const;
    static QString defaultInstance();
    static QString normalizedInstanceUrl(const QString &url);
    bool setInstance(const QString &url);
    const QString &instance() const;

signals:
    void finished();

private:
    void requestTranslate();
    void parseTranslate();
    void buildStateMachine();
    void buildSplitNetworkRequest(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text, int textLimit);
    void buildNetworkRequestState(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text);
    void resetData(TranslationError error = NoError, const QString &errorString = {});
    QByteArray requestBody(const QString &text, Language sourceLang) const;
    int requestTextBudget() const;
    static QString languageApiCode(Engine engine, Language lang);
    static Language language(Engine engine, const QString &langCode);
    static int getSplitIndex(const QString &text, int limit);
    static void addSpaceBetweenParts(QString &text);

    static const QMap<Language, QString> s_genericLanguageCodes;
    static const QMap<Language, QString> s_googleLanguageCodes;
    static const QMap<Language, QString> s_yandexLanguageCodes;
    static const QMap<Language, QString> s_ddgLanguageCodes;
    // Conservative encoded form-body budget; retain bounded sequential chunks.
    static constexpr int s_requestBodyLimit = 2000;
    static constexpr char s_textProperty[] = "Text";

    QStateMachine *m_stateMachine;
    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_currentReply;
    Language m_sourceLang = NoLanguage;
    Language m_translationLang = NoLanguage;
    TranslationError m_error = NoError;
    Engine m_engine = Google;
    QString m_source;
    QString m_translation;
    QString m_errorString;
    QString m_instance;
};

#endif // ONLINETRANSLATOR_H
