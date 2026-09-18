#include "Ft991Client.h"
#include "Ft991CatCodec.h"

#include <QMetaObject>
#include <type_traits>


Ft991Client::Ft991Client(
    QIODevice *device,
    QObject *parent)
    : RadioBackend(parent),
    m_device(device)
{
    Q_ASSERT(m_device);

    connect(
        m_device,
        &QIODevice::readyRead,
        this,
        &Ft991Client::onReadyRead);

    m_timeoutTimer.setSingleShot(true);

    connect(
        &m_timeoutTimer,
        &QTimer::timeout,
        this,
        &Ft991Client::onTimeout);
}


void Ft991Client::setTimeout(int milliseconds)
{
    m_timeoutMs = milliseconds;
}

/*
 * The public submit() call
 */

quint64 Ft991Client::submit(
    const RadioRequest &request)
{
    const quint64 id =
        m_nextRequestId++;

    QString error;

    auto encoded =
        encodeRequest(
            request,
            error);

    if (!encoded)
    {
        //
        // Emit asynchronously so submit() always
        // returns before completion/failure signals.
        //
        QMetaObject::invokeMethod(
            this,
            [this, id, error]()
            {
                emit requestFailed(
                    id,
                    error);
            },
            Qt::QueuedConnection);

        return id;
    }

    PendingRequest pending {
        id,
        request,
        *encoded
    };

    m_queue.enqueue(
        std::move(pending));

    pumpQueue();

    return id;
}

/*
 * Encoding RadioRequest into Yaesu CAT
 *
 *  This is the inverse of your simulator.
 *
 */
std::optional<Ft991Client::EncodedRequest>
Ft991Client::encodeRequest(
    const RadioRequest &request,
    QString &error) const
{
    return std::visit(
        [&error](const auto &req)
        -> std::optional<EncodedRequest>
        {
            using T =
                std::decay_t<decltype(req)>;


            //
            // GET FREQUENCY
            //
            if constexpr (
                std::is_same_v<T, GetFrequency>)
            {
                if (req.vfo == Vfo::A)
                {
                    return EncodedRequest {
                        "FA;",
                        "FA"
                    };
                }

                return EncodedRequest {
                    "FB;",
                    "FB"
                };
            }


            //
            // SET FREQUENCY
            //
            else if constexpr (
                std::is_same_v<T, SetFrequency>)
            {
                QByteArray mnemonic =
                    (req.vfo == Vfo::A)
                        ? "FA"
                        : "FB";

                return EncodedRequest {
                    Ft991Cat::formatFrequencyCommand(
                        mnemonic,
                        req.hz),

                    // no answer
                    {}
                };
            }


            //
            // GET MODE
            //
            else if constexpr (
                std::is_same_v<T, GetMode>)
            {
                return EncodedRequest {
                    "MD0;",
                    "MD0"
                };
            }


            //
            // SET MODE
            //
            else if constexpr (
                std::is_same_v<T, SetMode>)
            {
                auto catMode =
                    Ft991Cat::modeToCat(
                        req.mode);

                if (!catMode)
                {
                    error =
                        QStringLiteral(
                            "Unsupported FT-991A mode");

                    return std::nullopt;
                }

                QByteArray command("MD0");

                command += *catMode;
                command += ';';

                return EncodedRequest {
                    command,
                    {}
                };
            }


            //
            // GET TRANSMIT STATE
            //
            else if constexpr (
                std::is_same_v<T, GetTx>)
            {
                return EncodedRequest {
                    "TX;",
                    "TX"
                };
            }


            //
            // SET TRANSMIT STATE
            //
            else if constexpr (
                std::is_same_v<T, SetTx>)
            {
                return EncodedRequest {
                    req.transmit
                        ? QByteArray("TX1;")
                        : QByteArray("TX0;"),
                    {}
                };
            }


            else
            {
                error =
                    QStringLiteral(
                        "RadioRequest is not supported "
                        "by the FT-991A client");

                return std::nullopt;
            }
        },
        request);
}

/*
 * The queue
 *
 * Here's the part I consider most important.
 *
 */
void Ft991Client::pumpQueue()
{
    //
    // Already waiting for a response.
    //
    if (m_active)
        return;

    if (m_queue.isEmpty())
        return;

    if (!m_device ||
        !m_device->isOpen() ||
        !m_device->isWritable())
    {
        //
        // Fail only the next request.
        //
        PendingRequest pending =
            m_queue.dequeue();

        emit requestFailed(
            pending.id,
            QStringLiteral(
                "FT-991A transport is not writable"));

        //
        // Give remaining requests a chance later.
        //
        return;
    }

    m_active =
        m_queue.dequeue();

    const QByteArray &command =
        m_active->encoded.command;

    emit catTx(command);

    qint64 written =
        m_device->write(command);

    if (written != command.size())
    {
        failActive(
            QStringLiteral(
                "Unable to write complete CAT command"));

        return;
    }


    //
    // Set commands normally have no response.
    //
    if (m_active->encoded.expectedPrefix.isEmpty())
    {
        finishActive(
            AckResponse {});

        return;
    }


    //
    // Query -- wait for FT-991A response.
    //
    m_timeoutTimer.start(
        m_timeoutMs);
}



void Ft991Client::onReadyRead()
{
    m_rxBuffer +=
        m_device->readAll();

    while (true)
    {
        qsizetype terminator =
            m_rxBuffer.indexOf(';');

        if (terminator < 0)
            break;

        QByteArray frame =
            m_rxBuffer.left(
                terminator + 1);

        m_rxBuffer.remove(
            0,
            terminator + 1);

        emit catRx(frame);


        //
        // No request pending?
        //
        if (!m_active)
        {
            emit unsolicitedFrame(frame);
            continue;
        }


        //
        // Not the response we are waiting for?
        //
        if (!frameMatchesActiveRequest(frame))
        {
            emit unsolicitedFrame(frame);
            continue;
        }


        QString error;

        auto response =
            decodeResponse(
                m_active->request,
                frame,
                error);

        if (!response)
        {
            failActive(
                QStringLiteral(
                    "Malformed FT-991A response: %1 (%2)")
                    .arg(
                        QString::fromLatin1(frame),
                        error));

            continue;
        }

        finishActive(
            *response);
    }
}

/*
 * Matching responses
 *
 * Because...
 *   FA;    → FA...........;
 *   FB;    → FB...........;
 *   MD0;   → MD0..;
 *   TX;    → TX..;
 *
 */
bool Ft991Client::frameMatchesActiveRequest(
    const QByteArray &frame) const
{
    if (!m_active)
        return false;

    const QByteArray &prefix =
        m_active->encoded.expectedPrefix;

    if (prefix.isEmpty())
        return false;

    return frame.startsWith(prefix);
}

/*
 * Decode the FT-991A response
 */
std::optional<RadioResponse>
Ft991Client::decodeResponse(
    const RadioRequest &request,
    const QByteArray &frame,
    QString &error) const
{
    return std::visit(
        [&frame, &error](const auto &req)
        -> std::optional<RadioResponse>
        {
            using T =
                std::decay_t<decltype(req)>;


            //
            // FREQUENCY
            //
            if constexpr (
                std::is_same_v<T, GetFrequency>)
            {
                QByteArray mnemonic =
                    (req.vfo == Vfo::A)
                        ? "FA"
                        : "FB";

                auto hz =
                    Ft991Cat::parseFrequencyAnswer(
                        frame,
                        mnemonic);

                if (!hz)
                {
                    error =
                        QStringLiteral(
                            "Invalid frequency response");

                    return std::nullopt;
                }

                return RadioResponse {
                    FrequencyResponse {
                        req.vfo,
                        *hz
                    }
                };
            }


            //
            // MODE
            //
            else if constexpr (
                std::is_same_v<T, GetMode>)
            {
                //
                // MD0x;
                //
                if (frame.size() != 5 ||
                    !frame.startsWith("MD0") ||
                    !frame.endsWith(';'))
                {
                    error =
                        QStringLiteral(
                            "Invalid MD response");

                    return std::nullopt;
                }

                auto mode =
                    Ft991Cat::modeFromCat(
                        frame[3]);

                if (!mode)
                {
                    error =
                        QStringLiteral(
                            "Unknown FT-991A mode");

                    return std::nullopt;
                }

                return RadioResponse {
                    ModeResponse {
                        Vfo::A,
                        *mode
                    }
                };
            }


            //
            // TX STATE
            //
            else if constexpr (
                std::is_same_v<T, GetTx>)
            {
                if (frame.size() != 4 ||
                    !frame.startsWith("TX") ||
                    !frame.endsWith(';'))
                {
                    error =
                        QStringLiteral(
                            "Invalid TX response");

                    return std::nullopt;
                }

                TxState state;

                switch (frame[2])
                {
                case '0':
                    state =
                        TxState::Receive;
                    break;

                case '1':
                    state =
                        TxState::CatTransmit;
                    break;

                case '2':
                    state =
                        TxState::RadioTransmit;
                    break;

                default:
                    error =
                        QStringLiteral(
                            "Unknown TX state");

                    return std::nullopt;
                }

                return RadioResponse {
                    TxResponse {
                        state
                    }
                };
            }


            else
            {
                error =
                    QStringLiteral(
                        "Response received for "
                        "non-query request");

                return std::nullopt;
            }
        },
        request);
}

/*
 * Completing/Failing requests
 */
void Ft991Client::finishActive(
    const RadioResponse &response)
{
    if (!m_active)
        return;

    m_timeoutTimer.stop();

    const quint64 id =
        m_active->id;

    m_active.reset();

    emit requestCompleted(
        id,
        response);

    //
    // Avoid deep recursion if somebody queues a
    // large number of set commands.
    //
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            pumpQueue();
        },
        Qt::QueuedConnection);
}


void Ft991Client::failActive(
    const QString &error)
{
    if (!m_active)
        return;

    m_timeoutTimer.stop();

    const quint64 id =
        m_active->id;

    m_active.reset();

    emit requestFailed(
        id,
        error);

    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            pumpQueue();
        },
        Qt::QueuedConnection);
}


void Ft991Client::onTimeout()
{
    if (!m_active)
        return;

    const QString command =
        QString::fromLatin1(
            m_active->encoded.command);

    failActive(
        QStringLiteral(
            "Timeout waiting for FT-991A response to %1")
            .arg(command));
}

/*
 * Turn AI off when connecting to the real radio
 * We don't want unsolicited updates
 */
void Ft991Client::disableAutoInformation()
{
    if (!m_device ||
        !m_device->isOpen() ||
        !m_device->isWritable())
    {
        return;
    }

    QByteArray command("AI0;");

    emit catTx(command);

    m_device->write(command);
}


