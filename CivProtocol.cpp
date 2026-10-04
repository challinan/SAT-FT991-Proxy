#include "CivProtocol.h"

#include <QtGlobal>
#include <QDebug>

#include <algorithm>


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

void CivProtocol::init() {
    // Initial state
    m_selectedBand = CivBand::Main;
    emit updateMainSubLeds(m_selectedBand);
    m_selectedVfo = selectedVfo();

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
    for (char ch : data) {

        const quint8 b = static_cast<quint8>(ch);

        switch (m_rxState) {

        case RxState::WaitingForFe1:
            if (b == 0xFE) {
                m_rxFrame.clear();
                m_rxFrame.append(ch);
                m_rxState = RxState::WaitingForFe2;
            }
            break;

        case RxState::WaitingForFe2:
            if (b == 0xFE) {
                m_rxFrame.append(ch);
                m_rxState = RxState::ReceivingFrame;
            }
            else {
                // False start.
                m_rxFrame.clear();
                m_rxState = RxState::WaitingForFe1;
            }
            break;

        case RxState::ReceivingFrame:
            m_rxFrame.append(ch);

            if (b == 0xFD) {

                QByteArray frame = m_rxFrame;

                resetReceiver();

                processFrame(frame);
            }
            else if (m_rxFrame.size() > MaxCivFrameSize) {

                emit framingError(m_rxFrame);
                resetReceiver();
            }

            break;
        }
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
    // qDebug() << "CivProtocol::processFrame(): Entered:" << frame.toHex();
    /*
     * Minimum:
     *
     * FE FE TO FROM CMD FD
     */
    CivRequestContext context;
    const quint8 destination = static_cast<quint8>(frame.at(2));
    const quint8 source = static_cast<quint8>(frame.at(3));

    context.radioAddress = destination;
    context.controllerAddress = source;
    context.command = frame.mid(4, frame.size() - 5);
    context.originalFrame = frame;

    if (frame.size() < 6) {
        emit protocolError(QStringLiteral("CI-V frame too short"));
        return;
    }

    if (byteAt(frame, 0) != CIV_PREAMBLE ||
        byteAt(frame, 1) != CIV_PREAMBLE ||
        byteAt(frame, frame.size() - 1)
            != CIV_END)
    {
        emit protocolError(QStringLiteral("CivProtocol::processFrame(): Invalid CI-V framing"));
        sendNg(context);
        return;
    }

    /*
     * Ignore frames not addressed to our virtual IC-9700.
     *
     * This also naturally ignores our own responses if
     * they are echoed on the one-wire CI-V bus.
     */
    if (destination != m_radioAddress)
        return;

    if (context.command.isEmpty())
        return;

    if ( frame.indexOf("\xfe\xfe", 3) != -1 ) {
        // Frame contains multiple preamble sequences, discard it
        sendNg(context);
        return;
    }

    if (destination == 0xA2 && source == 0xE0) {
        emit sendToLogger(frame, "SAT", Dir::in);
        processCommand(context);

        // On the first valid frame, set the UI status LED
        if ( !first_valid_frame ) {
            emit firstValidFrameReceived();
            first_valid_frame = true;
        }
        return;
    }

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
    quint8 command = byteAt(context.command, 0);

    switch (command)
    {
    case 0x00:
        qDebug() << "CivProtocol::processCommand(): Processing 'Send the Frequency Data - Transceive'" << context.originalFrame.toHex();
        handleFrequencyRead(context);
        break;

    case 0x01:
        qDebug() << "CivProtocol::processCommand(): Processing 'Mode and Filter Setting'" << context.originalFrame.toHex();
        handleModeFilter(context);
        break;

    case 0x03:
        qDebug() << "CivProtocol::processCommand(): Processing FrequencyRead[0x03] command" << context.originalFrame.toHex();
        handleFrequencyRead(context);
        break;

    case 0x04:
        qDebug() << "CivProtocol::processCommand(): Processing ReadMode command" << context.originalFrame.toHex();
        handleModeRead(context);
        break;

    case 0x05:
        qDebug() << "CivProtocol::processCommand(): Processing FrequencySet [05] command" << context.originalFrame.toHex();
        handleFrequencySet(context);
        break;

    case 0x06:
        qDebug() << "CivProtocol::processCommand(): Processing ModeSet command" << context.originalFrame.toHex();
        handleModeSet(context);
        break;

    case 0x07:
        // qDebug() << "CivProtocol::processCommand(): Processing Main/Sub/Vfo command" << context.originalFrame.toHex();
        handleVfoCommand(context);
        break;

    case 0x0f:
        if ( context.command.length() == 1 ) {
            //S.A.T. is asking for split status
            QByteArray reply = QByteArray::fromHex("0f00");
            sendResponse(context, reply);
            return;
        }

        if ( context.command.at(1) == 1 ) {
            m_icomSplit = true;
        }
        else {
            if ( context.command.at(1) == 0 )
                m_icomSplit = false;
        }
        qDebug() << "CivProtocol::processCommand(): Processing SPLIT On/OFF:" << m_icomSplit << context.originalFrame.toHex();
        // IC-9700 has MAIN/SUB as well as Split.  Don't set FT-991A to split off, that defeats the config we want.
        // emit radioRequest(context, SetSplit {m_icomSplit});
        sendAck(context);
        return;

    case 0x14:
        // qDebug() << "CivProtocol::processCommand(): Processing 14(multi) command command" << context.originalFrame.toHex();
        handle14(context);
        break;

    case 0x16:
        // qDebug() << "CivProtocol::processCommand(): Processing 0x16(multi) command" << context.originalFrame.toHex();
        handle16(context);
        break;

    case 0x1A:
        qDebug() << "CivProtocol::processCommand(): Processing 0x1A(multi) command" << context.originalFrame.toHex();
        handle1A(context);
        break;

    case 0x21:
        qDebug() << "CivProtocol::processCommand(): Processing RIT Freq/Offset command ->> NOP" << context.originalFrame.toHex();
        if ( context.command.length() > 10 ) {
            // I've seen these bad packets that are more then 18 bytes long, and they are corrupt
            sendNg(context);
            return;
        }
        sendAck(context);
        break;

    case 0x27:
        qDebug() << "CivProtocol::processCommand(): Processing SCOPE setting ->> NOP" << context.originalFrame.toHex();
        sendNg(context);
        break;

    // Tuning Step
    case 0xF1:
        m_tuningStep = 10;
        sendAck(context);
        return;

    default:
        emit unsupportedFrame(context.originalFrame);
        sendNg(context);
        break;
    }
}

/*
 * Command 00/03 — read frequency
 */
void CivProtocol::handleFrequencyRead(
    const CivRequestContext &context)
{
    // Is this a read command or set command?
    RadioRequest request;
    if (context.command.size() == 1) {
        qDebug() << "CivProtocol::handleFrequencyRead():" << context.originalFrame.toHex();
        if (m_selectedBand == CivBand::Main) {
            request = GetFrequency {Vfo::A};
        }
        else {
            request = GetFrequency {Vfo::B};
        }

        emit radioRequest(context, request);
        return;
    }

    if ( context.command.size() == 6 ) {
        qDebug() << "CivProtocol::handleFrequencyRead(): Set Frequency Command" << context.originalFrame.toHex();
        handleFrequencySet(context);
        return;
    }

    sendNg(context);
    return;
}

/*
 * Command 05 — set frequency
 *  Also handles 00/03 from above
 *
 * The IC-9700 uses five BCD bytes for operating frequency,
 * least-significant digit-pair first. For example, the documented
 * 144.040 MHz representation ends in ...04 44 01
 *
 */
void CivProtocol::handleFrequencySet(
    const CivRequestContext &context)
{
    /*
     * CMD + five BCD frequency bytes
     */
    if (context.command.size() != 6) {
        sendNg(context);
        return;
    }

    auto hz = decodeFrequency(context.command.mid(1));

    if (!hz) {
        sendNg(context);
        return;
    }

    RadioRequest request = SetFrequency {selectedVfo(), *hz};
    emit radioRequest(context, request);

//    if ( m_selectedVfo == Vfo::A ) {
    if ( m_selectedBand == CivBand::Main ) {
        emit updateMainFreqDisplay(*hz);
    }
    else {
        emit updateSubFreqDisplay(*hz);
    }
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

    for (qsizetype i = 0; i < data.size(); ++i) {
        quint8 b = byteAt(data, i);

        int low = b & 0x0F;

        int high = (b >> 4) & 0x0F;


        if (low > 9 || high > 9) {
            return std::nullopt;
        }


        frequency += quint64(low) * multiplier;

        frequency += quint64(high) * multiplier * 10;

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
    if (context.command.size() != 1) {
        sendNg(context);
        return;
    }

    emit radioRequest(context, GetMode {selectedVfo()});
}

void CivProtocol::handleModeSet(
    const CivRequestContext &context)
{
    /*
     * 06 MODE FILTER
     */
    if (context.command.size() < 2 ||
        context.command.size() > 3)
    {
        sendNg(context);
        return;
    }


    quint8 mode = byteAt(context.command, 1);

    quint8 filter = 0x01;

    if (context.command.size() == 3)
    {
        filter =
            byteAt(
                context.command,
                2);
    }


    auto radioMode = decodeMode(mode, filter);

    if (!radioMode) {
        sendNg(context);
        return;
    }

    emit radioRequest(context, SetMode {selectedVfo(), *radioMode});
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
 * and provides D0 and D1 for MAIN and SUB selection.  The IC-9700
 * has two separate receivers, designated Main and Sub, and each of
 * those has VFO-A and VFO-B, which the FT-991A does not support.
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
    /* 
     *  CMD 07:
     *  VFO mode commands
     */
    RadioRequest request;

    // A single 0x07 is a valid request to select VFO mode (in case the radio is in Memory mode, for example
    if (context.command.size() == 1) {
        qDebug() << "CivProtocol::handleVfoCommand(): Select VFO Mode" << context.command;
        request = SelectVfoMode {};
        emit radioRequest(context, request);
        sendAck(context);
        return;
    }

    quint8 sub = byteAt(context.command, 1);

    switch (sub) {
    // Select VFO A
    case 0x00:
        m_selectedVfo = selectedVfo();
        sendAck(context);
        return;

    // Select VFO B.
    case 0x01:
        m_selectedVfo = selectedVfo();
        sendAck(context);
        return;

    // Exchange (Swap) Main and Sub Bands.
    // How we deal with sub bands is going to be tricky.  The FT-991A has AB; for this
    case 0xB0:
        // m_selectedBand = (m_selectedBand == CivBand::Main) ? CivBand::Sub : CivBand::Main;
        // emit updateMainSubLeds(m_selectedBand);
        request = SwapAB {};
        emit radioRequest(context, request);
        sendAck(context);
        return;

    /*
     *  Select MAIN
     *  On the IC-9700, the MAIN band is used for the downlink (Receive Frequency)
     *  On the FT-991A, split mode has TX on VFO-B
     */
    case 0xD0:
        // Select the MAIN band command
        m_selectedBand = CivBand::Main;
        emit updateMainSubLeds(m_selectedBand);
        sendAck(context);
        return;

    /*
     *  Select SUB
     *  On the IC-9700, the SUB band is used for the uplink (Transmit Frequency)
     */
    case 0xD1:
        // Select the MAIN band command
        m_selectedBand = CivBand::Sub;
        emit updateMainSubLeds(m_selectedBand);
        sendAck(context);
        return;

     default:
        emit unsupportedFrame(context.originalFrame);
        sendNg(context);
        return;
    }
}

/*
 * 14 Family of commands
 */
void CivProtocol::handle14(
    const CivRequestContext &context)
{
    if (context.command.size() < 2) {
        sendNg(context);
        return;
    }

    quint8 sub = byteAt(context.command, 1);

    if (sub != 0x0A) {
        emit unsupportedFrame(context.originalFrame);
        sendNg(context);
        return;
    }

    /*
     * 14 0A
     * Read RF power
     * The IC-9700 command family 14 uses two BCD bytes representing
     * a value from 0000–0255 for RF power. The Yaesu FT-991A PC
     * command uses 005–100, so an internal representation as a
     * percentage is a reasonable normalization.
     */
    if (context.command.size() == 2) {
        emit radioRequest(context, GetRfPower {});

        return;
    }

    /*
     * 14 0A XX XX
     * Set RF power
     */
    if (context.command.size() == 4)
    {
        auto raw = decodeLevel(context.command.mid(2, 2));

        if (!raw || *raw < 0 || *raw > 255) {
            sendNg(context);
            return;
        }

        int percent = qRound((*raw * 100.0) / 255.0);

        emit radioRequest(context, SetRfPower {percent});
        int watts = 50 * percent;
        emit updateRfPwr(watts);

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
    if (context.command.size() < 2) {
        sendNg(context);
        return;
    }

    quint8 sub = byteAt( context.command, 1);

    /*
     * 16 42
     * Set	the Repeater Tone function..
     */
    if (sub == 0x42) {
        // Set
        if (context.command.size() == 3) {

            sendAck(context);
            qDebug() << "CivProtocol::handle16(): Repeater Tone Function  --> NOP";

            return;
        }
    }

    /*
     * 16 58
     * Read SSB transmit bandwidth.
     */
    if (sub == 0x58) {
        // Read
        if (context.command.size() == 2) {
            QByteArray reply = QByteArray::fromHex("1658");

            reply.append(char(m_ssbTxBandwidth));

            sendResponse(context, reply);
            emit updateTXBw(m_ssbTxBandwidth);
            qDebug() << "CivProtocol::handle16():" << ((context.command.size() == 2) ? "Read" : "Set") << "TX Bandwidth";

            return;
        }
    }

    /*
     * 16 58
     * Set SSB transmit bandwidth.
     */
    if (context.command.size() == 3) {
        quint8 value = byteAt(context.command, 2);

        if (value > 0x02) {
            sendNg(context);
            return;
        }

        m_ssbTxBandwidth = value;
        sendAck(context);
        emit updateTXBw(m_ssbTxBandwidth);

        return;
    }

    /*
     * 16 59
     * Send/read the sub band (the Dualwatch function).
     */
    // Read
    if (sub == 0x59) {
        // Read
        if (context.command.size() == 2) {
            // Reply that it is OFF
            QByteArray reply = QByteArray::fromHex("00");

            sendResponse(context, reply);
            qDebug() << "CivProtocol::handle16(): Read sub band (Dualwatch)";

            return;
        }
        // Set
        if (context.command.size() == 3) {
            quint8 value = byteAt(context.command, 2);

            if (value > 0x02) {
                sendNg(context);
                return;
            }
            qDebug() << "CivProtocol::handle16(): SET sub band (Dualwatch) >> NOP";

            sendAck(context);

            return;
        }
    }

    //
    // 16 5A
    //
    // Satellite mode.
    //
    if (sub == 0x5A)
    {
        /*
         *  Read Satellite Mode
         */
        if (context.command.size() == 2)
        {
            QByteArray reply =
                QByteArray::fromHex("165A");

            reply.append(char(m_satelliteMode ? 0x01 : 0x00));

            sendResponse(context, reply);
            qDebug() << "CivProtocol::handle16(): Read Satellite Mode command";

            return;
        }


        //
        // Set
        //
        if (context.command.size() == 3)
        {
            quint8 value = byteAt(context.command, 2);

            if (value > 1) {
                sendNg(context);
                return;
            }

            m_satelliteMode = value != 0;

            sendAck(context);

            return;
        }


        sendNg(context);
        return;
    }

    emit unsupportedFrame(context.originalFrame);
    sendNg(context);
}

/*
 * 1A Command has many sub commands
 */
void CivProtocol::handle1A(
    const CivRequestContext &context)
{

    if (context.command.size() < 3)
    {
        qDebug() << "CivProtocol::handle1A(): Possible malformed command less than 3 bytes?" << context.command.toHex();
        sendNg(context);
        return;
    }

    quint8 sub = byteAt(context.command, 1);

    if (sub == 0x05) {
        // 1A 05 has many possible commands
        handle1A05(context);
        return;
    }

    // Data mode with filter width settings
    if (sub == 0x06) {
        /*
         * 1A 06 DD FF Data Mode with Filter Settings
         * DD - Data Mode ->> 00 = OFF, 01 = ON
         * FF - Filter Mode ->> 01 = FIL1, 02 = FIL2, 03 - FIL3
         * Note: C-IV Reference says "*When Data Mode is set to 00, must also set also set 00 to Filter Mode"
         */
        if ( context.command.length() == 4 ) {
            qDebug() << "CivProtocol::handle1A(): Data mode with filter width settings ----------------------->> FAKING IT";
            // Fake it for now
            sendAck(context);

            return;

        }
        else {
            qDebug() << "CivProtocol::handle1A(): Possible malformed command (1A 06) should be 4 bytes?" << context.command.toHex();
            sendNg(context);
        }
    return;
    }

    quint8 addressHigh = byteAt(context.command, 2);
    quint8 addressLow = byteAt(context.command, 3);

    /*
     * Set Transceive - same as FT-991A Auto Information (AI) command
     * Send unsolicited status reponses from radio
     */
    if (addressHigh == 0x01 &&
        addressLow == 0x27) {
            qDebug() << "CivProtocol::handle1A(): Set Transceive ??????????????????";
        sendAck(context);
    }

    emit unsupportedFrame(
        context.originalFrame);

    sendNg(context);
}

void CivProtocol::handle1A05(const CivRequestContext &context)
{
    // 0x1A 0x05 has many command possibilities

    // Combine byte 2 and 3 into a single 16-bit integer
    uint16_t subCommand = (static_cast<uint8_t>(context.command[2]) << 8) | static_cast<uint8_t>(context.command[3]);

    switch (subCommand) {
    case 0x0127: // Matches byte 2 == 0x01 and byte 3 == 0x27
        // Set Transceive mode (1A05 0127)
        qDebug() << "CivProtocol::handle1A05(): ***---***---***---***--- S.A.T WANTS TRANSCEIVE MODE ---***---***---***---***---***";
        sendAck(context);
        break;

    case 0x0131:
        /*
         * SET > Connectors > CI-V > CI-V DATA Echo Back
         * 1A 05 01 31
         */


        // Read: 1A 05 01 31
        if (context.command.size() == 4) {
            QByteArray reply = QByteArrayLiteral("\x1A\x05\x01\x31");

            reply.append(char(m_dataEchoBack ? 0x01 : 0x00));
            sendResponse(context, reply);
            qDebug() << "CivProtocol::handle1A(): Read DATA echo back command: Reply with EchoBack = " << m_dataEchoBack;
            return;
        }


        /*
         * Set:
         * 1A 05 01 31 00
         */
        if (context.command.size() == 5) {
            quint8 value =
                byteAt(context.command, 4);

            if (value > 1) {
                sendNg(context);
                return;
            }

            m_dataEchoBack = value != 0;
            sendAck(context);
            qDebug() << "CivProtocol::handle1A(): Set DATA echo back command" << (value ? "ON" : "OFF");
            return;
        }
        break;


    default:
        qDebug() << "CivProtocol::handle1A05(): ***---***---***---***--- Unsupported 1A05 Request ---***---***---***---***---***" << context.command.toHex();
        sendNg(context);
        break;
    }

    return;
}

/*
 * Response framing
 */
QByteArray CivProtocol::makeResponse(
    const CivRequestContext &context,
    const QByteArray &body) const
{
    QByteArray result;

    result.reserve(body.size() + 5);

    result.append(char(0xFE));
    result.append(char(0xFE));

    /*
     * Reverse request addresses.
     */
    result.append(char(context.controllerAddress));
    result.append(char(context.radioAddress));

    result += body;
    result.append(char(0xFD));

    return result;
}


void CivProtocol::sendResponse(
    const CivRequestContext &context,
    const QByteArray &body)
{
    QByteArray response = makeResponse(context, body);

    emit frameReady(response);
}

void CivProtocol::sendAck(
    const CivRequestContext &context)
{
    QByteArray body;
    body.append(char(CIV_OK));

    sendResponse(context, body);
}


void CivProtocol::sendNg(
    const CivRequestContext &context)
{
    QByteArray body;
    body.append(char(CIV_NG));

    sendResponse(context, body);
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
    if (std::holds_alternative<ErrorResponse>(response)) {
        sendNg(context);
        qDebug() << "CivProtocol::handleRadioResonse(): Backend Error";
        return;
    }

    quint8 command = byteAt(context.command, 0);
    qDebug() << "CivProtocol::handleRadioResonse(): command is" << command;

    //
    // 03 -- Get frequency
    //
    if (command == 0x03)
    {
        auto value = std::get_if<FrequencyResponse>(&response);

        if (!value) {
            sendNg(context);
            return;
        }

        QByteArray body;

        body.append(char(0x03));

        body += encodeFrequency(value->hz);

        sendResponse(context, body);

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


        auto mode = encodeMode(value->mode);

        if (!mode) {
            sendNg(context);
            return;
        }

        QByteArray body;
        body.append(char(0x04));
        body += *mode;

        sendResponse(context, body);

        return;
    }


    //
    // 14 0A -- RF power
    //
    if (command == 0x14 &&
        byteAt(context.command, 1) == 0x0A &&
        context.command.size() >= 2)
     {
        // This is a read request to read radio power - send back the response
        qDebug() << "CivProtocol::handleRadioResonse(): RF Power Query/Set response";
        if (context.command.size() == 2) {
            auto value = std::get_if<RfPowerResponse>(&response);

            if (!value) {
                sendNg(context);
                return;
            }

            // IC-9700 uses 0000-0255 as percent power.  FT-991A reports watts
            const int maxWatts = 50;

            double fraction = static_cast<double>(value->watts) / maxWatts;

            fraction = std::clamp(fraction, 0.0, 1.0);

            int civValue = qRound(fraction * 255.0);
            qDebug() << "CivProtocol::handleRadioResonse(): (RfPowerResponse) value:" << civValue
                     << "fraction" << fraction;

            QByteArray body = QByteArray::fromHex("140A");

            body += encodeLevel(civValue);

            sendResponse(context, body);
            emit updateRfPwr(value->watts);

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

int CivProtocol::maxPowerWattsForFrequency(
    quint64 hz) const
{
    //
    // FT-991A:
    //
    // HF / 6m        = 100 W
    // 2m / 70cm      = 50 W
    //

    if ((hz >= 144000000ULL &&
         hz <= 148000000ULL) ||
        (hz >= 430000000ULL &&
         hz <= 450000000ULL))
    {
        return 50;
    }

    return 100;
}

void CivProtocol::handleModeFilter(const CivRequestContext &context) {

    // Isolate Mode and Filter bytes
    // C-IV CMD: 01 XX XX
    quint16 modeByte = context.command.at(1);
    quint16 filterByte = context.command.at(2);
    qDebug() << "CivProtocol::handleModeFilter(): mode is" << modeByte << "filter is" << filterByte;

    RadioMode mode;

    switch (modeByte) {
    case 0:
        mode = RadioMode::Lsb;
        break;
    case 1:
        mode = RadioMode::Usb;
        break;
    case 2:
        mode = RadioMode::Am;
        break;
    case 3:
        mode = RadioMode::CwU;
        break;
    case 4:
        mode = RadioMode::RttyUsb;
        break;
    case 5:
        mode = RadioMode::Fm;
        break;
    case 7:
        mode = RadioMode::CwL;
        break;
    case 8:
        mode = RadioMode::RttyLsb;
        break;
    case 17:
        mode = RadioMode::DataFm;
        break;
    case 22:
        mode = RadioMode::DataFm;
        break;
    }

    if ( m_selectedBand == CivBand::Main ) {
        emit radioRequest(context, SetMode {selectedVfo(), mode});
    }
    else {
        //  FT991A does not have a command to directly set the mode in VFO B
        emit radioRequest(context, SwapAB {} );
        emit radioRequest(context, SetMode {selectedVfo(), mode});
        emit radioRequest(context, SwapAB {} );
    }
    sendAck(context);

}

void CivProtocol::resetReceiver()
{
    m_rxFrame.clear();
    m_rxState = RxState::WaitingForFe1;
}

Vfo CivProtocol::selectedVfo() const
{
    return m_selectedBand == CivBand::Main ? Vfo::B : Vfo::A;
}
