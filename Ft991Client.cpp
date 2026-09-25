#include "Ft991Client.h"
#include "Ft991CatCodec.h"

#include <QMetaObject>
#include <QDebug>
#include <type_traits>


Ft991Client::Ft991Client(
    QIODevice *device,
    QObject *parent)
    : RadioBackend(parent),
    m_device(device)
{
    // Debug() << "Ft991Client::Ft991Client(): Constructor Entered" << "this =" << this << "device =" << m_device;

    Q_ASSERT(m_device);

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
 * Submit a request to be transmitted to the radio
 */

quint64 Ft991Client::submit(
    const RadioRequest &request)
{
    const quint64 id = m_nextRequestId++;

    QString error;

    auto encoded = encodeRequest(request, error);
    qDebug() << "Ft991Client::submit(): ID:" << id << radioRequestTypeName(request);

    if (!encoded) {
        /*
         * Alternatively, we might be able to do this:
         * QTimer::singleShot(0, this, [this, id, error]() { emit requestFailed(id, error); });
         *
         * Emit asynchronously so submit() always
         * returns before completion/failure signals.
         */
        qDebug() << "Ft991Client::submit(): ID:" << id << "encodeRequest Failed" << radioRequestTypeName(request);
        QMetaObject::invokeMethod(
            this,
            [this, id, error]() {
                emit requestFailed(id, error);
            },
            Qt::QueuedConnection);


        return id;
    }

    PendingRequest pending {id, request, *encoded};

    m_queue.enqueue(std::move(pending));

    if ( m_queue.size() > 1 ) {
        qDebug() << "Ft991client::submit(): Multiple requests pending:"<< m_queue.size();
        QListIterator<PendingRequest> i(m_queue);
        while (i.hasNext()) {
            PendingRequest p = i.next();
            qDebug() << "Ft991Client::submit():     ID:" << p.id
                     << "Request:" << radioRequestTypeName(p.request)
                     << "CAT:" << p.encoded.command;        }
    }
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
        [&error](const auto &req) -> std::optional<EncodedRequest>
        {
            using T = std::decay_t<decltype(req)>;

            // GET FREQUENCY
            if constexpr (std::is_same_v<T, GetFrequency>) {
                QString v = (req.vfo == Vfo::A) ? "VFO_A" : "VFO_B";
                qDebug() << "Ft991Client::encodeRequest(): GetFrequency:" << v;
                if (req.vfo == Vfo::A) {
                    return EncodedRequest {"FA;", "FA"};
                }

                return EncodedRequest {"FB;", "FB"};
            }

            //
            // SET FREQUENCY
            //
            else if constexpr (
                std::is_same_v<T, SetFrequency>) {
                QString v = (req.vfo == Vfo::A) ? "VFO_A" : "VFO_B";
                qDebug() << "Ft991Client::encodeRequest(): GetFrequency:" << v;

                QByteArray mnemonic = (req.vfo == Vfo::A) ? "FA" : "FB";

                return EncodedRequest {
                    Ft991Cat::formatFrequencyCommand(mnemonic, req.hz),
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
                return EncodedRequest {"MD0;", "MD0"
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
                    error = QStringLiteral("Unsupported FT-991A mode");
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
                return EncodedRequest {"TX;", "TX"};
            }


            /*
             * GET RF Power
             */
            else if constexpr (std::is_same_v<T, GetRfPower>)
            {
                return EncodedRequest {"PC;", "PC"};
            }


            /*
             * SET RF Power
             */
            else if constexpr (std::is_same_v<T, SetRfPower>)
            {
                // Need the power value
                int watts = req.percent / 2;
                QByteArray cmd = "PC" + QByteArray::number(watts).rightJustified(3, '0');
                cmd.append(';');
                qDebug() << "Ft991Client::encodeRequest(): formatted power string:" << cmd.toHex();
                return EncodedRequest {cmd, {}};
            }


            //
            // SET TRANSMIT STATE
            //
            else if constexpr (
                std::is_same_v<T, SetTx>)
            {
                return EncodedRequest {req.transmit ? QByteArray("TX1;") : QByteArray("TX0;"),
                    {}
                };
            }


            else
            {
                error =
                    QStringLiteral("RadioRequest is not supported by the FT-991A client");

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
    /*
     * Already waiting for a response.
     */
    if (m_active)
        return;

    if (m_queue.isEmpty())
        return;

     if (!m_device ||
        !m_device->isOpen() ||
        !m_device->isWritable())
    {
        // Fail only the next request.
        PendingRequest pending = m_queue.dequeue();

        emit requestFailed(pending.id,
            QStringLiteral("FT-991A transport is not writable"));


         // Give remaining requests a chance later.
        return;
    }

     // Take the next request from the queue
     // and make it the one currently in progress.
    m_active = m_queue.dequeue();

    const QByteArray &command = m_active->encoded.command;

    // Diagnostic signal only.
    emit catTx(command);

    // Hand the CAT bytes to QSerialPort.

    qint64 written = m_device->write(command);

    qDebug() << "Ft991Client::pumpQueue(): bytes written:" << written << command;

    if (written != command.size()) {
        failActive(
            QStringLiteral(
                "Unable to write complete CAT command"));

        return;
    }


    /*
     * Set commands normally have no response...
     * If this is a SET command, there is normally
     * no answer from the FT-991A.
     */
    if (m_active->encoded.expectedPrefix.isEmpty()) {
        finishActive(AckResponse {});

        return;
    }


    // ...Otherwise this is a QUERY command.
    // Query -- wait for FT-991A response.
    // Wait for readyRead() to deliver the answer.
    //
    m_timeoutTimer.start(m_timeoutMs);
}




/*
 * Matching responses
 *
 * Because...
 *   FA;    → FA...........;
 *   FB;    → FB...........;
 *   MD0;   → MD0..;
 *   TX;    → TX..;
 *   etc...
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


            // GET FREQUENCY
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
                            "Ft991Client::decodeResponse(): Invalid frequency response");

                    return std::nullopt;
                }

                return RadioResponse {
                    FrequencyResponse {
                        req.vfo,
                        *hz
                    }
                };
            }


            // GET MODE
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
                            "Ft991Client::decodeResponse(): Invalid MD response");

                    return std::nullopt;
                }

                auto mode =
                    Ft991Cat::modeFromCat(
                        frame[3]);

                if (!mode)
                {
                    error =
                        QStringLiteral(
                            "Ft991Client::decodeResponse(): Unknown FT-991A mode");

                    return std::nullopt;
                }

                return RadioResponse {
                    ModeResponse {
                        Vfo::A,
                        *mode
                    }
                };
            }


            // GET TX STATE
             else if constexpr (
                std::is_same_v<T, GetTx>)
            {
                if (frame.size() != 4 ||
                    !frame.startsWith("TX") ||
                    !frame.endsWith(';'))
                {
                    error =
                        QStringLiteral(
                            "Ft991Client::decodeResponse(): Invalid TX response");

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
                            "Ft991Client::decodeResponse(): Unknown TX state");

                    return std::nullopt;
                }

                return RadioResponse {
                    TxResponse {
                        state
                    }
                };
            }


            /*
             * Get RF Power
             */
            else if constexpr (std::is_same_v<T, GetRfPower>)
            {
                // PCxxx; ie PC050;
                // qDebug() << "Ft991Client::decodeResponse(): frame size" << frame.size() << "frame" << frame;
                if (frame.size() != 6 ||
                    !frame.startsWith("PC") ||
                    !frame.endsWith(';'))
                {
                    error =
                        QStringLiteral(
                            "Ft991Client::decodeResponse(): Invalid PC response");

                    return std::nullopt;
                }
                // FT991A reports power in watts, max is 50W on UHF/VHF
                // Report percent power to CIV Proxy
                // int powerPercent = frame.mid(2, 3).toInt() / 50 * 100;
                // qDebug() << "Ft991Client::decodeResponse(): Power Percent:" << powerPercent;

                bool ok = false;
                int watts = frame.mid(2, 3).toInt(&ok, 10);
                if (!ok){
                    error = "Ft991Client::decodeResponse(): Invalid PC power value";
                    return std::nullopt;
                }

                return RadioResponse {RfPowerResponse {watts}};
            }

            /*
             * Set RF Power
             */
            else if constexpr (std::is_same_v<T, SetRfPower>)
            {
                // PCxxx; ie PC050;
                qDebug() << "Ft991Client::decodeResponse(): Set RF Power - frame size" << frame.size() << "frame" << frame;
                if (frame.size() != 6 ||
                    !frame.startsWith("PC") ||
                    !frame.endsWith(';'))
                {
                    error =
                        QStringLiteral("Ft991Client::decodeResponse(): Invalid PC response");

                    return std::nullopt;
                }
                // FT991A reports power in watts, max is 50W on UHF/VHF
                // S.A.T Reports power in percent

                bool ok = false;
                int watts = frame.mid(2, 3).toInt(&ok, 10);
                if (!ok){
                    error = "Ft991Client::decodeResponse(): Invalid PC power value";
                    return std::nullopt;
                }

                return RadioResponse {RfPowerResponse {watts}};
            }

            else
            {
                error =
                    QStringLiteral(
                        "Ft991Client::decodeResponse(): Response received for "
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

    const quint64 id = m_active->id;

    m_active.reset();

    qDebug() << "Ft991Client::finishActive(): emit requestCompleted()";
    emit requestCompleted(id, response);

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

    emit requestFailed(id, error);

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
            "Ft991Client::onTimeout(): Timeout waiting for FT-991A response to %1").arg(command));
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

void Ft991Client::feedBytes(
    const QByteArray &data)
{
    if (data.isEmpty())
        return;

    qDebug().noquote()
        << "Ft991Client::feedBytes(): Ft991Client RX:" << data.toHex(' ') << "ASCII:" << QString::fromLatin1(data);

    m_rxBuffer += data;

    while (true)
    {
        qsizetype terminator =
            m_rxBuffer.indexOf(';');

        if (terminator < 0)
            break;

        QByteArray frame = m_rxBuffer.left(terminator + 1);

        m_rxBuffer.remove(0, terminator + 1);

        if ( !first_valid_frame ) {
            emit firstValidFrameReceived();
            first_valid_frame = true;
        }
        emit catRx(frame);

        // Nothing currently waiting for an answer.
        if (!m_active) {
            emit unsolicitedFrame(frame);
            continue;
        }


        // Not the answer we're currently expecting.
         if (!frameMatchesActiveRequest(frame)) {
            emit unsolicitedFrame(frame);
            continue;
        }

        QString error;

        auto response = decodeResponse(m_active->request, frame, error);


        if (!response) {
            failActive(
                QStringLiteral(
                    "Ft991Client::feedBytes(): Malformed FT-991A response: "
                    "%1 (%2)")
                    .arg(
                        QString::fromLatin1(frame),
                        error));

            continue;
        }

        finishActive(*response);
    }
}

QString Ft991Client::radioRequestTypeName(
    const RadioRequest &request)
{
    return std::visit(
        [](const auto &value) -> QString
        {
            using T =
                std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, GetFrequency>)
                return "GetFrequency";

            else if constexpr (std::is_same_v<T, SetFrequency>)
                return "SetFrequency";

            else if constexpr (std::is_same_v<T, GetMode>)
                return "GetMode";

            else if constexpr (std::is_same_v<T, SetMode>)
                return "SetMode";

            else if constexpr (std::is_same_v<T, GetRfPower>)
                return "GetRfPower";

            else if constexpr (std::is_same_v<T, SetRfPower>)
                return "SetRfPower";

            else if constexpr (std::is_same_v<T, GetTx>)
                return "GetTx";

            else if constexpr (std::is_same_v<T, SetTx>)
                return "SetTx";

            else
                return "Unknown";
        },
        request);
}

