/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "ttsengine.h"

#include "src/utils/logger.h"
#include "src/utils/pythonenv.h"

#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTimer>

namespace {

constexpr int kReadyPollMs = 400;
constexpr int kManagedAttempts = 300;
constexpr int kExternalAttempts = 15;

} // namespace

TtsEngine::TtsEngine(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_readyTimer = new QTimer(this);
    m_readyTimer->setInterval(kReadyPollMs);
    connect(m_readyTimer, &QTimer::timeout, this, &TtsEngine::pollReadiness);
}

TtsEngine::~TtsEngine()
{
    stop();
}

bool TtsEngine::settingsChangeNeedsRestart(const QJsonObject &before, const QJsonObject &after) const
{
    return before != after;
}

QString TtsEngine::sampleText() const
{
    return sampleForLanguage(m_voice);
}

QJsonObject TtsEngine::saveSettings() const
{
    QJsonObject settings;
    settings.insert(QStringLiteral("voice"), m_voice);
    settings.insert(QStringLiteral("speed"), m_speed);
    settings.insert(QStringLiteral("external_server"), m_useExternal);
    settings.insert(QStringLiteral("server_url"), m_externalUrl);
    return settings;
}

void TtsEngine::loadSettings(const QJsonObject &settings)
{
    const QString wanted = settings.value(QStringLiteral("voice")).toString();

    if (m_voice.isEmpty())
        m_voice = wanted;
    else
        setVoice(wanted);

    setSpeed(settings.value(QStringLiteral("speed")).toInt(kNormalSpeed));
    m_useExternal = settings.value(QStringLiteral("external_server")).toBool();
    m_externalUrl = settings.value(QStringLiteral("server_url")).toString();
}

void TtsEngine::start()
{
    if (m_useExternal)
        connectExternal(m_externalUrl, m_voice);
    else
        startManaged(m_voice);
}

void TtsEngine::stop()
{
    m_readyTimer->stop();
    cancelSynthesis();
    m_queuedText.clear();

    setState(State::Stopped);

    if (!m_process)
        return;

    emit logLine(QStringLiteral("Stopping the %1 server.").arg(name()));

    m_process->terminate();
    if (!m_process->waitForFinished(3000))
        m_process->kill();

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void TtsEngine::setVoice(const QString &voice)
{
    if (voice.isEmpty() || voice == m_voice)
        return;

    const QString previous = m_voice;
    m_voice = voice;

    if (m_state == State::Stopped || m_state == State::Failed)
        return;

    if (m_mode == Mode::External || !voiceChangeNeedsRestart(previous, voice)) {
        emit logLine(QStringLiteral("Voice switched to %1").arg(voice));
        return;
    }

    startManaged(voice);
}

void TtsEngine::synthesize(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    switch (m_state) {
    case State::Ready:
        cancelSynthesis();
        sendSynthesis(trimmed);
        break;
    case State::Starting:
        m_queuedText = trimmed;
        emit logLine(QStringLiteral("The server is still loading, so the text is queued."));
        break;
    case State::Stopped:
    case State::Failed:
        logWarning(QStringLiteral("synthesize refused: state is %1").arg(m_state == State::Stopped ? QStringLiteral("Stopped") : QStringLiteral("Failed")));
        emit errorOccurred(tr("The %1 server is not running.").arg(name()));
        break;
    }
}

void TtsEngine::cancelSynthesis()
{
    m_queuedText.clear();

    if (!m_synthesisReply)
        return;

    QNetworkReply *reply = m_synthesisReply;
    m_synthesisReply = nullptr;

    reply->abort();
}

QString TtsEngine::normalizeUrl(const QString &raw)
{
    QString url = raw.trimmed();
    if (url.isEmpty())
        return url;

    if (!url.contains(QStringLiteral("://")))
        url.prepend(QStringLiteral("http://"));

    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);

    return url;
}

bool TtsEngine::voiceChangeNeedsRestart(const QString &current, const QString &next) const
{
    Q_UNUSED(current)
    Q_UNUSED(next)
    return true;
}

bool TtsEngine::wantsServerVoiceList() const
{
    return m_mode == Mode::External;
}

QJsonObject TtsEngine::synthesisPayload(const QString &text) const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text);

    if (!m_voice.isEmpty())
        payload.insert(QStringLiteral("voice"), m_voice);

    return payload;
}

QStringList TtsEngine::adoptVoiceList(const QJsonObject &voices)
{
    return voices.keys();
}

bool TtsEngine::looksLikeAudio(const QByteArray &data) const
{
    return data.startsWith("RIFF");
}

bool TtsEngine::extractServerScript(const QString &resource, const QString &target)
{
    QFile bundled(resource);
    if (!bundled.open(QIODevice::ReadOnly)) {
        logWarning(QStringLiteral("cannot read bundled script %1: %2").arg(resource, bundled.errorString()));
        emit errorOccurred(tr("The bundled %1 server script is missing.").arg(name()));
        return false;
    }

    QDir().mkpath(QFileInfo(target).absolutePath());

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logWarning(QStringLiteral("cannot write %1: %2").arg(target, file.errorString()));
        emit errorOccurred(tr("Cannot write %1").arg(target));
        return false;
    }

    file.write(bundled.readAll());
    return true;
}

QString TtsEngine::sampleForLanguage(const QString &code)
{
    static const QHash<QString, QString> samples = {
        {QStringLiteral("af"), QStringLiteral("Dit is 'n spraaktoets.")},
        {QStringLiteral("am"), QStringLiteral("ይህ የንግግር ሙከራ ነው።")},
        {QStringLiteral("ar"), QStringLiteral("هذا اختبار للنطق.")},
        {QStringLiteral("az"), QStringLiteral("Bu, nitq testidir.")},
        {QStringLiteral("bg"), QStringLiteral("Това е тест на говора.")},
        {QStringLiteral("bn"), QStringLiteral("এটি একটি বাক পরীক্ষা।")},
        {QStringLiteral("bs"), QStringLiteral("Ovo je test govora.")},
        {QStringLiteral("ca"), QStringLiteral("Aquesta és una prova de veu.")},
        {QStringLiteral("cs"), QStringLiteral("Toto je test řeči.")},
        {QStringLiteral("cy"), QStringLiteral("Dyma brawf llais.")},
        {QStringLiteral("da"), QStringLiteral("Dette er en taletest.")},
        {QStringLiteral("de"), QStringLiteral("Dies ist ein Sprachtest.")},
        {QStringLiteral("el"), QStringLiteral("Αυτή είναι μια δοκιμή ομιλίας.")},
        {QStringLiteral("en"), QStringLiteral("This is a speech test.")},
        {QStringLiteral("es"), QStringLiteral("Esta es una prueba de voz.")},
        {QStringLiteral("et"), QStringLiteral("See on kõnetest.")},
        {QStringLiteral("eu"), QStringLiteral("Hau ahots proba bat da.")},
        {QStringLiteral("fa"), QStringLiteral("این یک آزمایش گفتار است.")},
        {QStringLiteral("fi"), QStringLiteral("Tämä on puhetesti.")},
        {QStringLiteral("fil"), QStringLiteral("Ito ay isang pagsubok sa pagsasalita.")},
        {QStringLiteral("fr"), QStringLiteral("Ceci est un test de synthèse vocale.")},
        {QStringLiteral("ga"), QStringLiteral("Is tástáil urlabhra é seo.")},
        {QStringLiteral("gl"), QStringLiteral("Esta é unha proba de voz.")},
        {QStringLiteral("gu"), QStringLiteral("આ એક વાણી પરીક્ષણ છે.")},
        {QStringLiteral("he"), QStringLiteral("זהו מבחן דיבור.")},
        {QStringLiteral("hi"), QStringLiteral("यह एक वाक् परीक्षण है।")},
        {QStringLiteral("hr"), QStringLiteral("Ovo je test govora.")},
        {QStringLiteral("hu"), QStringLiteral("Ez egy beszédteszt.")},
        {QStringLiteral("hy"), QStringLiteral("Սա խոսքի փորձարկում է։")},
        {QStringLiteral("id"), QStringLiteral("Ini adalah tes suara.")},
        {QStringLiteral("is"), QStringLiteral("Þetta er talpróf.")},
        {QStringLiteral("it"), QStringLiteral("Questa è una prova vocale.")},
        {QStringLiteral("ja"), QStringLiteral("これは音声のテストです。")},
        {QStringLiteral("jv"), QStringLiteral("Iki tes swara.")},
        {QStringLiteral("ka"), QStringLiteral("ეს არის მეტყველების ტესტი.")},
        {QStringLiteral("kk"), QStringLiteral("Бұл сөйлеу сынағы.")},
        {QStringLiteral("km"), QStringLiteral("នេះជាការសាកល្បងសំឡេង។")},
        {QStringLiteral("kn"), QStringLiteral("ಇದು ಧ್ವನಿ ಪರೀಕ್ಷೆ.")},
        {QStringLiteral("ko"), QStringLiteral("이것은 음성 테스트입니다.")},
        {QStringLiteral("lb"), QStringLiteral("Dat ass e Sproochtest.")},
        {QStringLiteral("lo"), QStringLiteral("ນີ້ແມ່ນການທົດສອບສຽງ.")},
        {QStringLiteral("lt"), QStringLiteral("Tai kalbos testas.")},
        {QStringLiteral("lv"), QStringLiteral("Šis ir runas tests.")},
        {QStringLiteral("mk"), QStringLiteral("Ова е тест на говорот.")},
        {QStringLiteral("ml"), QStringLiteral("ഇതൊരു സംഭാഷണ പരീക്ഷണമാണ്.")},
        {QStringLiteral("mn"), QStringLiteral("Энэ бол ярианы туршилт.")},
        {QStringLiteral("mr"), QStringLiteral("ही एक भाषण चाचणी आहे.")},
        {QStringLiteral("ms"), QStringLiteral("Ini ialah ujian pertuturan.")},
        {QStringLiteral("mt"), QStringLiteral("Dan huwa test tad-diskors.")},
        {QStringLiteral("my"), QStringLiteral("ဤသည်မှာ အသံစမ်းသပ်မှုဖြစ်သည်။")},
        {QStringLiteral("nb"), QStringLiteral("Dette er en taletest.")},
        {QStringLiteral("ne"), QStringLiteral("यो एउटा वाणी परीक्षण हो।")},
        {QStringLiteral("nl"), QStringLiteral("Dit is een spraaktest.")},
        {QStringLiteral("no"), QStringLiteral("Dette er en taletest.")},
        {QStringLiteral("pl"), QStringLiteral("To jest test mowy.")},
        {QStringLiteral("ps"), QStringLiteral("دا د وینا ازموینه ده.")},
        {QStringLiteral("pt"), QStringLiteral("Este é um teste de voz.")},
        {QStringLiteral("ro"), QStringLiteral("Acesta este un test de vorbire.")},
        {QStringLiteral("ru"), QStringLiteral("Это проверка синтеза речи.")},
        {QStringLiteral("si"), QStringLiteral("මෙය කථන පරීක්ෂණයකි.")},
        {QStringLiteral("sk"), QStringLiteral("Toto je test reči.")},
        {QStringLiteral("sl"), QStringLiteral("To je preizkus govora.")},
        {QStringLiteral("so"), QStringLiteral("Kani waa tijaabo hadal.")},
        {QStringLiteral("sq"), QStringLiteral("Ky është një test i të folurit.")},
        {QStringLiteral("sr"), QStringLiteral("Ово је провера говора.")},
        {QStringLiteral("su"), QStringLiteral("Ieu tés sora.")},
        {QStringLiteral("sv"), QStringLiteral("Det här är ett talprov.")},
        {QStringLiteral("sw"), QStringLiteral("Huu ni mtihani wa sauti.")},
        {QStringLiteral("ta"), QStringLiteral("இது ஒரு பேச்சு சோதனை.")},
        {QStringLiteral("te"), QStringLiteral("ఇది ఒక ప్రసంగ పరీక్ష.")},
        {QStringLiteral("th"), QStringLiteral("นี่คือการทดสอบเสียงพูด")},
        {QStringLiteral("tr"), QStringLiteral("Bu bir konuşma testidir.")},
        {QStringLiteral("uk"), QStringLiteral("Це перевірка синтезу мовлення.")},
        {QStringLiteral("ur"), QStringLiteral("یہ تقریر کا ایک ٹیسٹ ہے۔")},
        {QStringLiteral("uz"), QStringLiteral("Bu nutq sinovi.")},
        {QStringLiteral("vi"), QStringLiteral("Đây là một bài kiểm tra giọng nói.")},
        {QStringLiteral("zh"), QStringLiteral("这是一段语音测试。")},
        {QStringLiteral("zu"), QStringLiteral("Lokhu kuwuvivinyo lokukhuluma.")},
    };

    QString tag = code.toLower();
    const qsizetype separator = tag.indexOf(QRegularExpression(QStringLiteral("[-_]")));
    if (separator > 0)
        tag.truncate(separator);

    return samples.value(tag, samples.value(QStringLiteral("en")));
}

void TtsEngine::connectExternal(const QString &url, const QString &voice)
{
    stop();

    m_mode = Mode::External;
    m_port = 0;
    m_voice = voice;
    m_baseUrl = normalizeUrl(url);
    m_serverVoices.clear();

    if (m_baseUrl.isEmpty()) {
        logWarning(QStringLiteral("connectExternal: empty server address (raw: \"%1\")").arg(url));
        emit errorOccurred(tr("No server address given."));
        setState(State::Failed);
        return;
    }

    emit logLine(QStringLiteral("Connecting to the %1 server at %2").arg(name(), m_baseUrl));
    beginWaitingForServer(kExternalAttempts);
}

void TtsEngine::startManaged(const QString &voice)
{
    if (m_process)
        stop();

    m_mode = Mode::Managed;

    if (!PythonEnv::venvReady()) {
        logWarning(QStringLiteral("startManaged: no usable virtual environment at %1").arg(PythonEnv::venvDir()));
        emit errorOccurred(tr("The Python environment is not ready."));
        setState(State::Failed);
        return;
    }

    if (voice.isEmpty() && needsVoiceToStart()) {
        logWarning(QStringLiteral("startManaged: no voice selected"));
        emit errorOccurred(tr("No voice selected."));
        setState(State::Failed);
        return;
    }

    m_voice = voice;

    if (!prepareManagedStart()) {
        setState(State::Failed);
        return;
    }

    m_port = pickFreePort();
    if (m_port == 0) {
        logWarning(QStringLiteral("startManaged: could not bind a free port on 127.0.0.1"));
        emit errorOccurred(tr("No free local port."));
        setState(State::Failed);
        return;
    }

    m_baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(m_port);

    QStringList args = serverArguments(voice);
    args << QStringLiteral("--host") << QStringLiteral("127.0.0.1")
         << QStringLiteral("--port") << QString::number(m_port);

    m_process = new QProcess(this);
    QProcess *process = m_process;
    PythonEnv::prepareChildEnvironment(m_process);
    m_process->setWorkingDirectory(serverWorkingDirectory());

    connect(m_process, &QProcess::readyRead, this, [this, process] {
        if (m_process == process)
            readProcessOutput();
    });
    connect(m_process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        if (m_process != process)
            return;

        readProcessOutput();

        if (m_state != State::Stopped) {
            logWarning(QStringLiteral("server exited with code %1 while %2 (voice: %3, port: %4)")
                           .arg(exitCode)
                           .arg(m_state == State::Starting ? QStringLiteral("starting") : QStringLiteral("running"))
                           .arg(m_voice.isEmpty() ? QStringLiteral("none") : m_voice)
                           .arg(m_port));
            emit errorOccurred(tr("The %1 server stopped (code %2).").arg(name()).arg(exitCode));
            setState(State::Failed);
        }

        m_readyTimer->stop();
        m_process->deleteLater();
        m_process = nullptr;
    });

    emit logLine(QStringLiteral("$ %1 %2").arg(PythonEnv::venvPython(), args.join(QLatin1Char(' '))));

    m_process->start(PythonEnv::venvPython(), args);
    beginWaitingForServer(kManagedAttempts);
}

void TtsEngine::logWarning(const QString &line) const
{
    Log(Logger::Level::Warning, QStringLiteral("[%1] %2").arg(id(), line));
}

void TtsEngine::requestVoiceList()
{
    if (m_baseUrl.isEmpty())
        return;

    QNetworkReply *reply = m_network->get(QNetworkRequest(QUrl(endpoint(QStringLiteral("/voices")))));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit logLine(QStringLiteral("Could not read the server voice list: %1").arg(reply->errorString()));
            return;
        }

        m_serverVoices = adoptVoiceList(QJsonDocument::fromJson(reply->readAll()).object());
        emit logLine(QStringLiteral("The server offers %1 voice(s).").arg(m_serverVoices.size()));
        emit voicesAvailable(m_serverVoices);
    });
}

void TtsEngine::beginWaitingForServer(int attempts)
{
    m_readyAttempts = 0;
    m_maxReadyAttempts = attempts;
    setState(State::Starting);
    m_readyTimer->start();
}

void TtsEngine::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void TtsEngine::pollReadiness()
{
    if (m_state != State::Starting || m_readyReply)
        return;

    if (++m_readyAttempts > m_maxReadyAttempts) {
        m_readyTimer->stop();
        logWarning(QStringLiteral("no answer from %1 after %2 attempts (%3 mode)")
                       .arg(m_baseUrl)
                       .arg(m_maxReadyAttempts)
                       .arg(m_mode == Mode::External ? QStringLiteral("external") : QStringLiteral("managed")));
        emit errorOccurred(m_mode == Mode::External
                               ? tr("No %1 server answered at %2.").arg(name(), m_baseUrl)
                               : tr("The %1 server did not answer in time.").arg(name()));
        setState(State::Failed);
        return;
    }

    QNetworkReply *reply = m_network->get(QNetworkRequest(QUrl(endpoint(QStringLiteral("/info")))));
    m_readyReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QByteArray body = reply->readAll();

        reply->deleteLater();
        if (m_readyReply == reply)
            m_readyReply = nullptr;

        if (!ok || m_state != State::Starting)
            return;

        m_readyTimer->stop();

        const QJsonObject info = QJsonDocument::fromJson(body).object();
        const QString reported = voiceFromInfo(info);

        if (m_mode == Mode::External && m_voice.isEmpty())
            m_voice = reported;

        emit logLine(QStringLiteral("%1 is ready at %2, voice %3")
                         .arg(name(), m_baseUrl, reported.isEmpty() ? m_voice : reported));

        setState(State::Ready);

        if (wantsServerVoiceList())
            requestVoiceList();

        if (!m_queuedText.isEmpty()) {
            const QString text = m_queuedText;
            m_queuedText.clear();
            sendSynthesis(text);
        }
    });
}

void TtsEngine::sendSynthesis(const QString &text)
{
    QNetworkRequest request{QUrl(endpoint(QStringLiteral("/synthesize")))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(synthesisPayload(text)).toJson(QJsonDocument::Compact));
    m_synthesisReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_synthesisReply == reply)
            m_synthesisReply = nullptr;
        reply->deleteLater();

        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;

        if (reply->error() != QNetworkReply::NoError) {
            logWarning(QStringLiteral("synthesis request failed: ") + reply->errorString());
            emit errorOccurred(reply->errorString());
            return;
        }

        const QByteArray audio = reply->readAll();
        if (!looksLikeAudio(audio)) {
            logWarning(QStringLiteral("synthesis returned %1 bytes that are not audio; first 400: %2")
                           .arg(audio.size())
                           .arg(QString::fromUtf8(audio.left(400))));
            emit errorOccurred(tr("The server returned no audio."));
            return;
        }

        emit audioReady(audio);
    });
}

void TtsEngine::readProcessOutput()
{
    if (!m_process)
        return;

    const QString chunk = PythonEnv::stripAnsiEscapes(QString::fromUtf8(m_process->readAll()));
    if (chunk.isEmpty())
        return;

    m_pendingLine += chunk;
    m_pendingLine.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const int lastBreak = m_pendingLine.lastIndexOf(QLatin1Char('\n'));
    if (lastBreak < 0)
        return;

    const QStringList lines = m_pendingLine.left(lastBreak).split(QLatin1Char('\n'));
    m_pendingLine = m_pendingLine.mid(lastBreak + 1);

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            emit logLine(trimmed);
    }
}

QString TtsEngine::endpoint(const QString &path) const
{
    return m_baseUrl + path;
}

quint16 TtsEngine::pickFreePort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0))
        return 0;

    const quint16 port = server.serverPort();
    server.close();
    return port;
}