#include "CivProtocol.h"

#include <QtGlobal>
#include <QDebug>

#include <algorithm>
#include <cmath>


namespace
{
constexpr quint8 CIV_PREAMBLE = 0xFE;
constexpr quint8 CIV_END      = 0xFD;

constexpr quint8 CIV_OK       = 0xFB;
constexpr quint8 CIV_NG       = 0xFA;
}


CivProtocol::CivProtocol(
    QObject *parent)
    : QObject(parent)
{
}

/*
 * Byte Helper:
 * Using this avoids signed-char ugliness:
 */
quint8 CivProtocol::byteAt(
    const QByteArray &data,
    qsizetype index)
{
    return static_cast<quint8>(
        static_cast<unsigned char>(
            data.at(index)));
}

/*
 * Serial stream parser
 *
 * Make this tolerant of partial frames and junk before the preamble.
 * So if you get:
 *
 * FE FE A2 E0 03
 *
 * followed later by:
 *
 * FD FE FE A2 E0 16 58 FD
 *
 * it correctly produces two complete frames.
 *
 */
void CivProtocol::feedBytes(
    const QByteArray &data)
{
    // qDebug() << "CivProtocol::feedBytes(): Entered with data:" << data.toHex();
    m_rxBuffer += data;

    const QByteArray preamble =
        QByteArray::fromHex("FEFE");

    while (true)
    {
        qsizetype start =
            m_rxBuffer.indexOf(preamble);

        if (start < 0)
        {
            //
            // Preserve one trailing FE in case
            // the next block begins with FE.
            //
            if (!m_rxBuffer.isEmpty() &&
                byteAt(
                    m_rxBuffer,
                    m_rxBuffer.size() - 1)
                    == CIV_PREAMBLE)
            {
                m_rxBuffer =
                    QByteArray(1, char(0xFE));
            }
            else
            {
                m_rxBuffer.clear();
            }

            return;
        }


        if (start > 0)
        {
            m_rxBuffer.remove(
                0,
                start);
        }


        qsizetype end =
            m_rxBuffer.indexOf(
                char(CIV_END),
                4);

        if (end < 0)
            return;


        QByteArray frame =
            m_rxBuffer.left(
                end + 1);

        m_rxBuffer.remove(
            0,
            end + 1);


        qDebug() << "CivProtocol::feedBytes(): Frame received:" << frame.toHex();
        emit frameReceived(frame);
        // emit stateChanged(true  );

        processFrame(frame);
    }
}


/*
 * Basic frame validation
 *
 * This echoed-frame behavior is useful with CI-V: our outgoing
 * response is addressed to E0, not A2, so if the USB
 * interface echoes it back, we simply ignore it.
 */
void CivProtocol::processFrame(
    const QByteArray &frame)
{
    //
    // Minimum:
    //
    // FE FE TO FROM CMD FD
    //
    if (frame.size() < 6)
    {
        emit protocolError(
            QStringLiteral(
                "CI-V frame too short"));

        return;
    }


    if (byteAt(frame, 0) != CIV_PREAMBLE ||
        byteAt(frame, 1) != CIV_PREAMBLE ||
        byteAt(frame, frame.size() - 1)
            != CIV_END)
    {
        emit protocolError(
            QStringLiteral(
                "Invalid CI-V framing"));

        return;
    }


    quint8 destination =
        byteAt(frame, 2);

    quint8 source =
        byteAt(frame, 3);


    //
    // Ignore frames not addressed to our
    // virtual IC-9700.
    //
    // This also naturally ignores our own
    // responses if they are echoed on the
    // one-wire CI-V bus.
    //
    if (destination != m_radioAddress)
        return;


    CivRequestContext context;

    context.radioAddress =
        destination;

    context.controllerAddress =
        source;

    context.command =
        frame.mid(
            4,
            frame.size() - 5);

    context.originalFrame =
        frame;


    if (context.command.isEmpty())
        return;


    processCommand(context);
}

/*
 * Command dispatcher
 *
 * Resist building one giant CI-V command table. The first-byte
 * command dispatch is small, while 14, 16, and 1A are really
 * command families containing their own subcommand namespaces.
 *
 */
void CivProtocol::processCommand(
    const CivRequestContext &context)
{
    quint8 command =
        byteAt(
            context.command,
            0);

    // On the first valid frame, set the status LED
    if ( !first_valid_frame ) {
        emit firstValidFrameReceived();
        first_valid_frame = true;
    }

    switch (command)
    {
    case 0x03:
        qDebug() << "CivProtocol::processCommand(): Processing FrequencyRead command";
        handleFrequencyRead(context);
        break;

    case 0x04:
        qDebug() << "CivProtocol::processCommand(): Processing ReadMode command";
        handleModeRead(context);
        break;

    case 0x05:
        qDebug() << "CivProtocol::processCommand(): Processing FrequencySet command";
        handleFrequencySet(context);
        break;

    case 0x06:
        qDebug() << "CivProtocol::processCommand(): Processing ModeSet command";
        handleModeSet(context);
        break;

    case 0x07:
        qDebug() << "CivProtocol::processCommand(): Processing VfoCommand command";
        handleVfoCommand(context);
        break;

    case 0x14:
        qDebug() << "CivProtocol::processCommand(): Processing 14 command command";
        handle14(context);
        break;

    case 0x16:
        qDebug() << "CivProtocol::processCommand(): Processing 0x16 command";
        handle16(context);
        break;

    case 0x1A:
        qDebug() << "CivProtocol::processCommand(): Processing 0x1A command";
        handle1A(context);
        break;

    default:
        emit unsupportedFrame(
            context.originalFrame);
        qDebug() << "CivProtocol::processCommand(): Unsupported Frame";

        sendNg(context);
        break;
    }
}

/*
 * Command 03 — read frequency
 */
void CivProtocol::handleFrequencyRead(
    const CivRequestContext &context)
{
    if (context.command.size() != 1)
    {
        sendNg(context);
        return;
    }

    RadioRequest request =
        GetFrequency {
            m_selectedVfo
        };

    emit radioRequest(
        context,
        request);
}

/*
 * Command 05 — set frequency
 *
 * The IC-9700 uses five BCD bytes for operating frequency,
 * least-significant digit-pair first. For example, the documented
 * 144.040 MHz representation ends in ...04 44 01
 *
 */
void CivProtocol::handleFrequencySet(
    const CivRequestContext &context)
{
    //
    // CMD + five BCD frequency bytes
    //
    if (context.command.size() != 6)
    {
        sendNg(context);
        return;
    }

    auto hz =
        decodeFrequency(
            context.command.mid(1));

    if (!hz)
    {
        sendNg(context);
        return;
    }


    RadioRequest request =
        SetFrequency {
            m_selectedVfo,
            *hz
        };

    emit radioRequest(
        context,
        request);
}

/*
 * The Decoder
 */
std::optional<quint64>
CivProtocol::decodeFrequency(
    const QByteArray &data)
{
    if (data.size() != 5)
        return std::nullopt;


    quint64 frequency = 0;
    quint64 multiplier = 1;


    for (qsizetype i = 0;
         i < data.size();
         ++i)
    {
        quint8 b =
            byteAt(data, i);

        int low =
            b & 0x0F;

        int high =
            (b >> 4) & 0x0F;


        if (low > 9 ||
            high > 9)
        {
            return std::nullopt;
        }


        frequency +=
            quint64(low) *
            multiplier;

        frequency +=
            quint64(high) *
            multiplier * 10;

        multiplier *= 100;
    }


    return frequency;
}

/*
 * The Encoder
 */
QByteArray CivProtocol::encodeFrequency(
    quint64 hz)
{
    QByteArray result;

    result.reserve(5);


    for (int i = 0;
         i < 5;
         ++i)
    {
        quint8 low =
            hz % 10;

        hz /= 10;

        quint8 high =
            hz % 10;

        hz /= 10;


        result.append(
            char(
                (high << 4) |
                low));
    }


    return result;
}


/*
 * Commands 04 and 06 — mode
 *
 * The IC-9700 uses a mode byte followed by a filter byte;
 * the filter may be omitted when setting mode.
 *
 */
void CivProtocol::handleModeRead(
    const CivRequestContext &context)
{
    if (context.command.size() != 1)
    {
        sendNg(context);
        return;
    }

    emit radioRequest(
        context,
        GetMode {
            m_selectedVfo
        });
}

void CivProtocol::handleModeSet(
    const CivRequestContext &context)
{
    //
    // 06 MODE
    //
    // or
    //
    // 06 MODE FILTER
    //
    if (context.command.size() < 2 ||
        context.command.size() > 3)
    {
        sendNg(context);
        return;
    }


    quint8 mode =
        byteAt(
            context.command,
            1);

    quint8 filter = 0x01;

    if (context.command.size() == 3)
    {
        filter =
            byteAt(
                context.command,
                2);
    }


    auto radioMode =
        decodeMode(
            mode,
            filter);

    if (!radioMode)
    {
        sendNg(context);
        return;
    }


    emit radioRequest(
        context,
        SetMode {
            m_selectedVfo,
            *radioMode
        });
}

/*
 * CODEC
 */
std::optional<RadioMode>
CivProtocol::decodeMode(
    quint8 mode,
    quint8 filter)
{
    switch (mode)
    {
    case 0x00:
        return RadioMode::Lsb;

    case 0x01:
        return RadioMode::Usb;

    case 0x02:
        return
            (filter == 0x02)
                ? RadioMode::AmNarrow
                : RadioMode::Am;

    case 0x03:
        return RadioMode::CwU;

    case 0x04:
        return RadioMode::RttyLsb;

    case 0x05:
        return
            (filter == 0x02)
                ? RadioMode::FmNarrow
                : RadioMode::Fm;

    case 0x07:
        return RadioMode::CwL;

    case 0x08:
        return RadioMode::RttyUsb;
    }

    return std::nullopt;
}

std::optional<QByteArray>
CivProtocol::encodeMode(
    RadioMode mode)
{
    QByteArray result;

    switch (mode)
    {
    case RadioMode::Lsb:
        result.append(char(0x00));
        result.append(char(0x01));
        break;

    case RadioMode::Usb:
        result.append(char(0x01));
        result.append(char(0x01));
        break;

    case RadioMode::Am:
        result.append(char(0x02));
        result.append(char(0x01));
        break;

    case RadioMode::AmNarrow:
        result.append(char(0x02));
        result.append(char(0x02));
        break;

    case RadioMode::CwU:
        result.append(char(0x03));
        result.append(char(0x01));
        break;

    case RadioMode::RttyLsb:
        result.append(char(0x04));
        result.append(char(0x01));
        break;

    case RadioMode::Fm:
        result.append(char(0x05));
        result.append(char(0x01));
        break;

    case RadioMode::FmNarrow:
        result.append(char(0x05));
        result.append(char(0x02));
        break;

    case RadioMode::CwL:
        result.append(char(0x07));
        result.append(char(0x01));
        break;

    case RadioMode::RttyUsb:
        result.append(char(0x08));
        result.append(char(0x01));
        break;

    default:
        return std::nullopt;
    }


    return result;
}

/*
 * Command 07
 *
 * This is interesting given the traffic I've captured.
 *
 * The IC-9700 command table identifies 07 as “Select the VFO mode,”
 * and provides D0 and D1 for MAIN and SUB selection.
 *
 * So we'll consider this enter/select VFO mode.
 *
 * The mapping:
 *
 * IC-9700 MAIN  → FT-991A VFO A
 * IC-9700 SUB   → FT-991A VFO B
 */
void CivProtocol::handleVfoCommand(
    const CivRequestContext &context)
{
    //
    // Bare 07:
    //
    // Select VFO mode.
    //
    if (context.command.size() == 1)
    {
        sendAck(context);
        return;
    }


    if (context.command.size() != 2)
    {
        sendNg(context);
        return;
    }


    quint8 sub =
        byteAt(
            context.command,
            1);


    switch (sub)
    {
    //
    // Select VFO A.
    //
    case 0x00:
        m_selectedVfo = Vfo::A;
        sendAck(context);
        return;


    //
    // Select VFO B.
    //
    case 0x01:
        m_selectedVfo = Vfo::B;
        sendAck(context);
        return;


    //
    // MAIN
    //
    case 0xD0:
        m_selectedVfo = Vfo::A;
        sendAck(context);
        return;


    //
    // SUB
    //
    case 0xD1:
        m_selectedVfo = Vfo::B;
        sendAck(context);
        return;


    default:
        emit unsupportedFrame(
            context.originalFrame);

        sendNg(context);
        return;
    }
}

/*
 * 14 0A — RF power
 *
 * The IC-9700 command family 14 uses two BCD bytes representing
 * a value from 0000–0255 for RF power. The Yaesu FT-991A PC
 * command uses 005–100, so an internal representation as a
 * percentage is a reasonable normalization.
 */
void CivProtocol::handle14(
    const CivRequestContext &context)
{
    if (context.command.size() < 2)
    {
        sendNg(context);
        return;
    }


    quint8 sub =
        byteAt(
            context.command,
            1);


    if (sub != 0x0A)
    {
        emit unsupportedFrame(
            context.originalFrame);

        sendNg(context);
        return;
    }


    //
    // 14 0A
    //
    // Read RF power.
    //
    if (context.command.size() == 2)
    {
        emit radioRequest(
            context,
            GetRfPower {});

        return;
    }


    //
    // 14 0A XX XX
    //
    // Set RF power.
    //
    if (context.command.size() == 4)
    {
        auto raw =
            decodeLevel(
                context.command.mid(
                    2,
                    2));

        if (!raw ||
            *raw < 0 ||
            *raw > 255)
        {
            sendNg(context);
            return;
        }


        int percent =
            qRound(
                (*raw * 100.0)
                / 255.0);


        emit radioRequest(
            context,
            SetRfPower {
                percent
            });

        return;
    }


    sendNg(context);
}

/*
 * BCD level helpers:
 */
std::optional<int>
CivProtocol::decodeLevel(
    const QByteArray &data)
{
    if (data.size() != 2)
        return std::nullopt;


    quint8 a =
        byteAt(data, 0);

    quint8 b =
        byteAt(data, 1);


    int digits[4] = {
        (a >> 4) & 0x0F,
        a & 0x0F,
        (b >> 4) & 0x0F,
        b & 0x0F
    };


    for (int digit : digits)
    {
        if (digit > 9)
            return std::nullopt;
    }


    return
        digits[0] * 1000 +
        digits[1] * 100 +
        digits[2] * 10 +
        digits[3];
}


QByteArray CivProtocol::encodeLevel(
    int value)
{
    value =
        std::clamp(
            value,
            0,
            9999);


    int thousands =
        (value / 1000) % 10;

    int hundreds =
        (value / 100) % 10;

    int tens =
        (value / 10) % 10;

    int ones =
        value % 10;


    QByteArray result;

    result.append(
        char(
            (thousands << 4) |
            hundreds));

    result.append(
        char(
            (tens << 4) |
            ones));

    return result;
}

/*
 * 16 58 and 16 5A
 * These can stay entirely inside the virtual IC-9700.
*/
void CivProtocol::handle16(
    const CivRequestContext &context)
{
    if (context.command.size() < 2)
    {
        sendNg(context);
        return;
    }


    quint8 sub =
        byteAt(
            context.command,
            1);


    //
    // 16 58
    //
    // SSB transmit bandwidth.
    //
    if (sub == 0x58)
    {
        //
        // Read
        //
        if (context.command.size() == 2)
        {
            QByteArray reply =
                QByteArray::fromHex(
                    "1658");

            reply.append(
                char(
                    m_ssbTxBandwidth));

            sendResponse(
                context,
                reply);

            return;
        }


        //
        // Set
        //
        if (context.command.size() == 3)
        {
            quint8 value =
                byteAt(
                    context.command,
                    2);

            if (value > 0x02)
            {
                sendNg(context);
                return;
            }


            m_ssbTxBandwidth =
                value;

            sendAck(context);

            return;
        }


        sendNg(context);
        return;
    }


    //
    // 16 5A
    //
    // Satellite mode.
    //
    if (sub == 0x5A)
    {
        //
        // Read
        //
        if (context.command.size() == 2)
        {
            QByteArray reply =
                QByteArray::fromHex(
                    "165A");

            reply.append(
                char(
                    m_satelliteMode
                        ? 0x01
                        : 0x00));

            sendResponse(
                context,
                reply);

            return;
        }


        //
        // Set
        //
        if (context.command.size() == 3)
        {
            quint8 value =
                byteAt(
                    context.command,
                    2);

            if (value > 1)
            {
                sendNg(context);
                return;
            }

            m_satelliteMode =
                value != 0;

            sendAck(context);

            return;
        }


        sendNg(context);
        return;
    }


    emit unsupportedFrame(
        context.originalFrame);

    sendNg(context);
}

/*
 * 1A 05 01 31
 * These can also stay inside the virtual IC-9700
 */
void CivProtocol::handle1A(
    const CivRequestContext &context)
{
    //
    // Need at least:
    //
    // 1A 05 XX XX
    //
    if (context.command.size() < 4)
    {
        sendNg(context);
        return;
    }


    quint8 sub =
        byteAt(
            context.command,
            1);


    if (sub != 0x05)
    {
        emit unsupportedFrame(
            context.originalFrame);

        sendNg(context);
        return;
    }


    quint8 addressHigh =
        byteAt(
            context.command,
            2);

    quint8 addressLow =
        byteAt(
            context.command,
            3);


    //
    // SET > Connectors > CI-V >
    // CI-V DATA Echo Back
    //
    // 1A 05 01 31
    //
    if (addressHigh == 0x01 &&
        addressLow == 0x31)
    {
        //
        // Read:
        //
        // 1A 05 01 31
        //
        if (context.command.size() == 4)
        {
            QByteArray reply =
                QByteArray::fromHex(
                    "1A050131");

            reply.append(
                char(
                    m_dataEchoBack
                        ? 0x01
                        : 0x00));

            sendResponse(
                context,
                reply);

            return;
        }


        //
        // Set:
        //
        // 1A 05 01 31 00
        //
        if (context.command.size() == 5)
        {
            quint8 value =
                byteAt(
                    context.command,
                    4);

            if (value > 1)
            {
                sendNg(context);
                return;
            }


            m_dataEchoBack =
                value != 0;

            sendAck(context);

            return;
        }
    }


    emit unsupportedFrame(
        context.originalFrame);

    sendNg(context);
}

/*
 * Response framing
 */
QByteArray CivProtocol::makeResponse(
    const CivRequestContext &context,
    const QByteArray &body) const
{
    QByteArray result;

    result.reserve(
        body.size() + 5);


    result.append(
        char(0xFE));

    result.append(
        char(0xFE));


    //
    // Reverse request addresses.
    //
    result.append(
        char(
            context.controllerAddress));

    result.append(
        char(
            context.radioAddress));


    result += body;


    result.append(
        char(0xFD));


    return result;
}


void CivProtocol::sendResponse(
    const CivRequestContext &context,
    const QByteArray &body)
{
    emit frameReady(
        makeResponse(
            context,
            body));
}


void CivProtocol::sendAck(
    const CivRequestContext &context)
{
    QByteArray body;

    body.append(
        char(CIV_OK));

    sendResponse(
        context,
        body);
}


void CivProtocol::sendNg(
    const CivRequestContext &context)
{
    QByteArray body;

    body.append(
        char(CIV_NG));

    sendResponse(
        context,
        body);
}

/*
 * Converting RadioResponse back to CI-V
 *
 * This closes the loop.
 */
void CivProtocol::handleRadioResponse(
    CivRequestContext context,
    RadioResponse response)
{
    //
    // Backend explicitly reported an error.
    //
    if (std::holds_alternative<ErrorResponse>(
            response))
    {
        sendNg(context);
        return;
    }


    quint8 command =
        byteAt(
            context.command,
            0);


    //
    // 03 -- Get frequency
    //
    if (command == 0x03)
    {
        auto value =
            std::get_if<FrequencyResponse>(
                &response);

        if (!value)
        {
            sendNg(context);
            return;
        }


        QByteArray body;

        body.append(
            char(0x03));

        body +=
            encodeFrequency(
                value->hz);


        sendResponse(
            context,
            body);

        return;
    }


    //
    // 04 -- Get mode
    //
    if (command == 0x04)
    {
        auto value =
            std::get_if<ModeResponse>(
                &response);

        if (!value)
        {
            sendNg(context);
            return;
        }


        auto mode =
            encodeMode(
                value->mode);

        if (!mode)
        {
            sendNg(context);
            return;
        }


        QByteArray body;

        body.append(
            char(0x04));

        body += *mode;


        sendResponse(
            context,
            body);

        return;
    }


    //
    // 14 0A -- RF power
    //
    if (command == 0x14 &&
        context.command.size() >= 2 &&
        byteAt(
            context.command,
            1) == 0x0A)
    {
        //
        // Query
        //
        if (context.command.size() == 2)
        {
            auto value =
                std::get_if<RfPowerResponse>(
                    &response);

            if (!value)
            {
                sendNg(context);
                return;
            }


            int raw =
                qRound(
                    value->percent *
                    255.0 /
                    100.0);


            QByteArray body =
                QByteArray::fromHex(
                    "140A");

            body +=
                encodeLevel(raw);


            sendResponse(
                context,
                body);

            return;
        }
    }


    //
    // All of our currently supported backend
    // SET operations expect AckResponse.
    //
    if (command == 0x05 ||
        command == 0x06 ||
        command == 0x14)
    {
        if (std::holds_alternative<
                AckResponse>(
                response))
        {
            sendAck(context);
        }
        else
        {
            sendNg(context);
        }

        return;
    }


    sendNg(context);
}

/*
 * Backend Failure
 */
void CivProtocol::handleRadioFailure(
    CivRequestContext context,
    QString error)
{
    emit protocolError(
        QStringLiteral(
            "Radio backend failure: %1")
            .arg(error));

    sendNg(context);
}