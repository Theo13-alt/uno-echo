/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "onlinetranslator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

class TranslatorTest final : public QObject
{
    Q_OBJECT
    QTcpServer server;
    QList<QUrl> requests;
    QList<QByteArray> bodies;
    QList<QByteArray> methods;
    QList<QUrl> wireUrls;
    QByteArray response;
    int status = 200;
    bool holdResponse = false;

    void prepare(OnlineTranslator &translator, const QByteArray &body, int code = 200)
    {
        requests.clear(); bodies.clear(); methods.clear(); wireUrls.clear(); response = body; status = code; holdResponse = false;
        translator.setInstance(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    }

private slots:
    void normalizesServer_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("default") << OnlineTranslator::defaultInstance() << OnlineTranslator::defaultInstance();
        QTest::newRow("slashes") << QStringLiteral(" https://example.org/// ") << QStringLiteral("https://example.org");
        QTest::newRow("prefix") << QStringLiteral("https://example.org/mozhi/") << QStringLiteral("https://example.org/mozhi");
        QTest::newRow("local") << QStringLiteral("http://127.0.0.1:1234/") << QStringLiteral("http://127.0.0.1:1234");
        for (const auto *input : {"", "example.org", "ftp://example.org", "https://", "https://a:b@example.org", "https://example.org/?secret=x", "https://example.org/#x", "https://example.org/api/translate/"})
            QTest::newRow(input) << QString::fromLatin1(input) << QString();
    }

    void normalizesServer()
    {
        QFETCH(QString, input); QFETCH(QString, expected);
        QCOMPARE(OnlineTranslator::normalizedInstanceUrl(input), expected);
        OnlineTranslator translator;
        QVERIFY(translator.setInstance(OnlineTranslator::defaultInstance()));
        QCOMPARE(translator.setInstance(input), !expected.isEmpty());
        QCOMPARE(translator.instance(), expected.isEmpty() ? OnlineTranslator::defaultInstance() : expected);
    }

    void initTestCase()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    const int headerEnd = request.indexOf("\r\n\r\n");
                    if (headerEnd < 0 || socket->property("answered").toBool()) return;
                    const QByteArray header = request.left(headerEnd);
                    int contentLength = 0;
                    for (const auto &line : header.split('\n')) {
                        if (line.trimmed().toLower().startsWith("content-length:"))
                            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                    }
                    if (request.size() - headerEnd - 4 < contentLength) return;
                    socket->setProperty("answered", true);
                    const QByteArray bodyReceived = request.mid(headerEnd + 4, contentLength);
                    const QUrl wireUrl = QUrl::fromEncoded(header.split(' ').value(1));
                    const QByteArray method = header.split(' ').first();
                    methods.append(method); bodies.append(bodyReceived); wireUrls.append(wireUrl);
                    // Independently decode HTML form rules, including '+' as a space.
                    QUrl decodedUrl = wireUrl;
                    QUrlQuery decodedQuery;
                    for (const auto &field : bodyReceived.split('&')) {
                        const int equals = field.indexOf('=');
                        QByteArray key = field.left(equals), value = field.mid(equals + 1);
                        key.replace('+', ' '); value.replace('+', ' ');
                        decodedQuery.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromPercentEncoding(key))),
                                                  QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromPercentEncoding(value))));
                    }
                    decodedUrl.setQuery(decodedQuery);
                    requests.append(decodedUrl);
                    if (holdResponse) return;
                    const int replyStatus = wireUrl.toEncoded().size() > 2000 ? 414 : status;
                    const QByteArray body = replyStatus == 414 ? QByteArray("URI too long") : response;
                    socket->write("HTTP/1.1 " + QByteArray::number(replyStatus) + " Test\r\nContent-Type: application/json\r\nContent-Length: " +
                                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void receivesFragmentedFormBody()
    {
        OnlineTranslator translator;
        prepare(translator, R"({"detected":"en","translated-text":"ok"})");
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(client.waitForConnected());
        const QByteArray body = "text=C%2B%2B+a%26b%3Dc%25d&engine=yandex&from=en&to=ru";
        client.write("POST /api/translate HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: "
                     + QByteArray::number(body.size()) + "\r\n\r\n" + body.left(8));
        client.flush();
        QTest::qWait(30);
        QVERIFY(requests.isEmpty());
        client.write(body.mid(8)); client.flush();
        QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 1, 5000);
        QCOMPARE(QUrlQuery(requests.first()).queryItemValue("text", QUrl::FullyDecoded), QStringLiteral("C++ a&b=c%d"));
    }

    void translates_data()
    {
        QTest::addColumn<int>("engine"); QTest::addColumn<int>("target"); QTest::addColumn<QString>("detected");
        for (const auto engine : {OnlineTranslator::Google, OnlineTranslator::Yandex, OnlineTranslator::Duckduckgo})
            for (const auto target : {OnlineTranslator::English, OnlineTranslator::Russian}) {
                const QByteArray label = QByteArray::number(engine) + '-' + QByteArray::number(target);
                QTest::newRow(label) << int(engine) << int(target) << (target == OnlineTranslator::English ? QStringLiteral("ru") : QStringLiteral("en"));
            }
        QTest::newRow("google-alias") << int(OnlineTranslator::Google) << int(OnlineTranslator::Russian) << QStringLiteral("iw");
        QTest::newRow("yandex-alias") << int(OnlineTranslator::Yandex) << int(OnlineTranslator::Russian) << QStringLiteral("sr-Latn");
        QTest::newRow("bing-alias") << int(OnlineTranslator::Duckduckgo) << int(OnlineTranslator::Russian) << QStringLiteral("mww");
        for (const auto engine : {OnlineTranslator::Google, OnlineTranslator::Yandex, OnlineTranslator::Duckduckgo})
            for (const auto &code : {"fil", "tl", "jv", "jw"}) {
                const QByteArray label = QByteArray::number(engine) + '-' + code;
                QTest::newRow(label) << int(engine) << int(OnlineTranslator::Russian) << QString::fromLatin1(code);
            }
    }

    void translates()
    {
        QFETCH(int, engine); QFETCH(int, target); QFETCH(QString, detected);
        OnlineTranslator translator;
        // Unused extra response fields must not change the plain-text translation.
        prepare(translator, QJsonDocument(QJsonObject{{"detected", detected}, {"translated-text", QStringLiteral("Перевод & result")},
                         {"target_transliteration", "unused"}, {"source_transliteration", "unused"}}).toJson());
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        const QString source = QStringLiteral("Привет C++ 1+1=2 a&b=c%d 100% %2B %26 ? / мир 😀\nстрока");
        translator.translate(source, static_cast<OnlineTranslator::Engine>(engine), static_cast<OnlineTranslator::Language>(target));
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::NoError);
        QCOMPARE(translator.translation(), QStringLiteral("Перевод & result"));
        QCOMPARE(requests.size(), 1);
        QCOMPARE(methods.first(), QByteArray("POST"));
        QVERIFY(!wireUrls.first().hasQuery());
        QVERIFY(bodies.first().contains("C%2B%2B"));
        const QUrlQuery query(requests.first());
        QCOMPARE(requests.first().path(), QStringLiteral("/api/translate"));
        QCOMPARE(query.queryItemValue("text", QUrl::FullyDecoded), source);
        QCOMPARE(query.queryItemValue("from"), QStringLiteral("auto"));
        QCOMPARE(query.queryItemValue("to"), target == OnlineTranslator::English ? QStringLiteral("en") : QStringLiteral("ru"));
        QCOMPARE(query.queryItemValue("engine"), engine == OnlineTranslator::Google ? QStringLiteral("google") : engine == OnlineTranslator::Yandex ? QStringLiteral("yandex") : QStringLiteral("duckduckgo"));
    }

    void splitsLongText_data()
    {
        QTest::addColumn<QString>("source");
        QTest::newRow("ascii") << (QString(9999, 'x') + ' ' + QStringLiteral("tail"));
        QTest::newRow("cyrillic") << QStringLiteral("Очень длинная фраза. ").repeated(250);
        QTest::newRow("reserved-characters") << QStringLiteral(" &?%#+=/").repeated(500);
        QTest::newRow("emoji-no-spaces") << QString::fromUtf8("😀").repeated(1000);
        QTest::newRow("boundary-surrogate") << (QString(1910, 'x') + QString::fromUtf8("😀") + QString(1910, 'x'));
    }

    void splitsLongText()
    {
        QFETCH(QString, source);
        OnlineTranslator translator;
        prepare(translator, R"({"detected":"en","translated-text":"part"})");
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate(source, OnlineTranslator::Yandex, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::NoError);
        QVERIFY(requests.size() > 1);
        QCOMPARE(translator.translation(), QStringList(requests.size(), QStringLiteral("part")).join(' '));
        QString sent;
        for (int i = 0; i < requests.size(); ++i) {
            const QString part = QUrlQuery(requests[i]).queryItemValue("text", QUrl::FullyDecoded);
            QVERIFY(!part.isEmpty());
            QVERIFY(!part.front().isLowSurrogate());
            QVERIFY(!part.back().isHighSurrogate());
            QCOMPARE(methods[i], QByteArray("POST"));
            QVERIFY(!wireUrls[i].hasQuery());
            QVERIFY2(bodies[i].size() <= 2000, "Encoded form body exceeded the chunk budget");
            sent += part;
        }
        QCOMPARE(sent, source);
        QCOMPARE(QUrlQuery(requests.last()).queryItemValue("from"), QStringLiteral("en"));
    }

    void rejectsUnknownDetectedLanguage()
    {
        OnlineTranslator translator;
        prepare(translator, R"({"detected":"unknown","translated-text":"wrong"})");
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::ParsingError);
        QVERIFY(translator.translation().isEmpty());
    }

    void usesServerPrefixWithoutDoubleSlash()
    {
        OnlineTranslator translator;
        prepare(translator, R"({"detected":"en","translated-text":"prefix ok"})");
        QVERIFY(translator.setInstance(QStringLiteral("http://127.0.0.1:%1/mozhi///").arg(server.serverPort())));
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::NoError);
        QCOMPARE(requests.first().path(), QStringLiteral("/mozhi/api/translate"));
    }

    void rejectsMissingServerWithoutRequest()
    {
        OnlineTranslator translator;
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QCOMPARE(done.size(), 1);
        QCOMPARE(translator.error(), OnlineTranslator::InstanceError);
        QVERIFY(!translator.isRunning());
    }

    void canSwitchServerAfterAbort()
    {
        OnlineTranslator translator;
        prepare(translator, ""); holdResponse = true;
        translator.translate("old text", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 1, 5000);
        translator.abort();
        QTRY_VERIFY_WITH_TIMEOUT(!translator.isRunning(), 5000);
        QVERIFY(translator.setInstance(QStringLiteral("http://127.0.0.1:%1/new/").arg(server.serverPort())));
        response = R"({"detected":"en","translated-text":"new server"})";
        holdResponse = false;
        requests.clear();
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("new text", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::NoError);
        QCOMPARE(translator.translation(), QStringLiteral("new server"));
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().path(), QStringLiteral("/new/api/translate"));
        QCOMPARE(QUrlQuery(requests.first()).queryItemValue("text"), QStringLiteral("new text"));
    }

    void reportsServiceError_data()
    {
        QTest::addColumn<int>("code");
        for (const auto code : {414, 431, 429, 500, 503})
            QTest::newRow(qPrintable(QString::number(code))) << code;
    }

    void reportsServiceError()
    {
        QFETCH(int, code);
        OnlineTranslator translator;
        prepare(translator, "<html><body>UNTRUSTED " + QByteArray(10000, 'x') + "</body></html>", code);
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::InstanceError);
        QVERIFY(translator.translation().isEmpty());
        QVERIFY(translator.errorString().size() < 200);
        QVERIFY(!translator.errorString().contains("<"));
        QVERIFY(!translator.errorString().contains("UNTRUSTED"));
        if (code == 414 || code == 431) QVERIFY(translator.errorString().contains(QStringLiteral("длинный")));
    }

    void recognisesRateLimitBody()
    {
        OnlineTranslator translator;
        prepare(translator, "instance has been rate limited", 500);
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::InstanceError);
        QVERIFY(translator.errorString().contains(QStringLiteral("частоту")));
    }

    void recoversAfterError()
    {
        OnlineTranslator translator;
        prepare(translator, "failed", 500);
        QSignalSpy done(&translator, &OnlineTranslator::finished);
        translator.translate("first", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::InstanceError);
        QCoreApplication::processEvents();
        done.clear();
        prepare(translator, R"({"detected":"ru","translated-text":"recovered"})");
        translator.translate(QStringLiteral("снова"), OnlineTranslator::Yandex, OnlineTranslator::English);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 5000);
        QCOMPARE(translator.error(), OnlineTranslator::NoError);
        QCOMPARE(translator.translation(), QStringLiteral("recovered"));
        QCOMPARE(requests.size(), 1);
    }

    void abortsPendingRequest()
    {
        OnlineTranslator translator;
        prepare(translator, ""); holdResponse = true;
        translator.translate("hello", OnlineTranslator::Google, OnlineTranslator::Russian);
        QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 1, 5000);
        translator.abort();
        QTRY_VERIFY_WITH_TIMEOUT(!translator.isRunning(), 5000);
        QVERIFY(translator.translation().isEmpty());
        QCOMPARE(translator.error(), OnlineTranslator::NetworkError);
    }
};

QTEST_GUILESS_MAIN(TranslatorTest)
#include "translator_test.moc"
