#include "Ft991Protocol.h"

#include <algorithm>


Ft991Protocol::Ft991Protocol(RadioCore &radio)
    : m_radio(radio)
{

    /*
     * The Command Table
     * m_commands is a QHash hash-table based dictionary
     */

     m_commands.insert(
        "FA",
        { "FA", Vfo::A, &decodeFA, &encodeFA }
        );

    m_commands.insert(
        "FB",
        { "FB", Vfo::A, &decodeFB, &encodeFB }
        );

    m_commands.insert(
        "MD",
        { "MD", Vfo::A, &decodeMD, &encodeMD }
        );

    m_commands.insert(
        "TX",
        { "TX", Vfo::A, &decodeTX, &encodeTX }
        );
}

/*
 * Command Dispatcher
 *
 * This dispatcher has no switch() involving FA, FB, MD, TX, etc.
 * New commands just become additional table entries.
 *
 */


QByteArray Ft991Protocol::processFrame(
    const QByteArray &frame,
    QString *error)
{
    if (error)
        error->clear();

    // Minimum valid command: "XX;"
    if (frame.size() < 3 || !frame.endsWith(';'))
    {
        if (error)
            *error = QStringLiteral("Invalid CAT frame");

        return {};
    }

    QByteArray mnemonic =
        frame.left(2).toUpper();

    QByteArray payload =
        frame.mid(2, frame.size() - 3);

    auto it = m_commands.constFind(mnemonic);

    if (it == m_commands.constEnd())
    {
        if (error)
            *error =
                QStringLiteral("Unknown CAT command: %1")
                    .arg(QString::fromLatin1(mnemonic));

        return {};
    }

    const CommandDefinition &definition = it.value();

    DecodedCommand decoded;

    QString decodeError;

    if (!definition.decode(
            payload,
            decoded,
            decodeError))
    {
        if (error)
            *error = decodeError;

        return {};
    }

    RadioResponse response =
        m_radio.execute(decoded.request);

    // Yaesu SET commands normally don't return an answer.
    if (!decoded.wantsResponse)
        return {};

    return definition.encode(response);
}

/*
 * Yaesu requires exactly nine decimal digits for a frequency SET
 * command, with FA; and FB; serving as queries.
 */

bool Ft991Protocol::decodeFrequency(
    const QByteArray &payload,
    Vfo vfo,
    DecodedCommand &decoded,
    QString &error)
{
    // No parameters = Read command
    if (payload.isEmpty())
    {
        decoded.request = GetFrequency { vfo };
        decoded.wantsResponse = true;

        return true;
    }

    if (payload.size() != 9)
    {
        error = QStringLiteral(
            "Frequency must contain exactly 9 digits"
            );

        return false;
    }

    const bool allDigits =
        std::all_of(
            payload.begin(),
            payload.end(),
            [](char c)
            {
                return c >= '0' && c <= '9';
            });

    if (!allDigits)
    {
        error = QStringLiteral(
            "Invalid frequency"
            );

        return false;
    }

    bool ok = false;

    quint64 hz =
        payload.toULongLong(&ok);

    if (!ok ||
        hz < 30000 ||
        hz > 470000000)
    {
        error = QStringLiteral(
            "Frequency outside FT-991A range"
            );

        return false;
    }

    decoded.request =
        SetFrequency { vfo, hz };

    decoded.wantsResponse = false;

    return true;
}


bool Ft991Protocol::decodeFA(
    const QByteArray &payload,
    DecodedCommand &decoded,
    QString &error)
{
    return decodeFrequency(
        payload,
        Vfo::A,
        decoded,
        error
        );
}


bool Ft991Protocol::decodeFB(
    const QByteArray &payload,
    DecodedCommand &decoded,
    QString &error)
{
    return decodeFrequency(
        payload,
        Vfo::B,
        decoded,
        error
        );
}

// Response generation
QByteArray Ft991Protocol::encodeFrequency(
    const RadioResponse &response,
    const QByteArray &mnemonic)
{
    auto value =
        std::get_if<FrequencyResponse>(
            &response);

    if (!value)
        return {};

    QByteArray freq =
        QByteArray::number(value->hz)
            .rightJustified(9, '0');

    return mnemonic + freq + ';';
}


QByteArray Ft991Protocol::encodeFA(
    const RadioResponse &response)
{
    return encodeFrequency(
        response,
        "FA"
        );
}


QByteArray Ft991Protocol::encodeFB(
    const RadioResponse &response)
{
    return encodeFrequency(
        response,
        "FB"
        );
}

// CAT mode mapping
bool Ft991Protocol::modeFromCat(
    char c,
    RadioMode &mode)
{
    // Source: FT-991A CAT manual for mode command.
    switch (c)
    {
    case '1': mode = RadioMode::Lsb;      return true;
    case '2': mode = RadioMode::Usb;      return true;
    case '3': mode = RadioMode::CwU;      return true;
    case '4': mode = RadioMode::Fm;       return true;
    case '5': mode = RadioMode::Am;       return true;
    case '6': mode = RadioMode::RttyLsb;  return true;
    case '7': mode = RadioMode::CwL;      return true;
    case '8': mode = RadioMode::DataLsb;  return true;
    case '9': mode = RadioMode::RttyUsb;  return true;
    case 'A': mode = RadioMode::DataFm;   return true;
    case 'B': mode = RadioMode::FmNarrow; return true;
    case 'C': mode = RadioMode::DataUsb;  return true;
    case 'D': mode = RadioMode::AmNarrow; return true;
    case 'E': mode = RadioMode::C4fm;     return true;
    }

    return false;
}


char Ft991Protocol::modeToCat(
    RadioMode mode)
{
    switch (mode)
    {
    case RadioMode::Lsb:       return '1';
    case RadioMode::Usb:       return '2';
    case RadioMode::CwU:       return '3';
    case RadioMode::Fm:        return '4';
    case RadioMode::Am:        return '5';
    case RadioMode::RttyLsb:   return '6';
    case RadioMode::CwL:       return '7';
    case RadioMode::DataLsb:   return '8';
    case RadioMode::RttyUsb:   return '9';
    case RadioMode::DataFm:    return 'A';
    case RadioMode::FmNarrow:  return 'B';
    case RadioMode::DataUsb:   return 'C';
    case RadioMode::AmNarrow:  return 'D';
    case RadioMode::C4fm:      return 'E';
    }

    return '?';
}

// Decode MD command
bool Ft991Protocol::decodeMD(
    const QByteArray &payload,
    // Vfo vfo,
    DecodedCommand &decoded,
    QString &error)
{
    // MD0; = Read MAIN RX mode
    if (payload == "0")
    {
        decoded.request = GetMode {};
        decoded.wantsResponse = true;

        return true;
    }

    // MD0x; = Set MAIN RX mode
    if (payload.size() == 2 &&
        payload[0] == '0')
    {
        RadioMode mode;

        if (!modeFromCat(
                payload[1],
                mode))
        {
            error =
                QStringLiteral(
                    "Invalid MD mode"
                    );

            return false;
        }

        decoded.request =
            SetMode { Vfo::A, mode };

        decoded.wantsResponse = false;

        return true;
    }

    error =
        QStringLiteral(
            "Invalid MD command"
            );

    return false;
}

// Response:

QByteArray Ft991Protocol::encodeMD(
    const RadioResponse &response)
{
    auto value =
        std::get_if<ModeResponse>(
            &response);

    if (!value)
        return {};

    QByteArray result("MD0");

    result += modeToCat(
        value->mode);

    result += ';';

    return result;
}

// TX
bool Ft991Protocol::decodeTX(
    const QByteArray &payload,
    DecodedCommand &decoded,
    QString &error)
{
    // TX; = Read TX state
    if (payload.isEmpty())
    {
        decoded.request = GetTx {};
        decoded.wantsResponse = true;

        return true;
    }

    if (payload == "0")
    {
        decoded.request =
            SetTx { false };

        decoded.wantsResponse = false;

        return true;
    }

    if (payload == "1")
    {
        decoded.request =
            SetTx { true };

        decoded.wantsResponse = false;

        return true;
    }

    error =
        QStringLiteral(
            "Invalid TX command"
            );

    return false;
}

// TX Response
// TX0;    radio not transmitting, CAT TX off
// TX1;    transmitting because CAT requested it
// TX2;    radio transmitting, but not because CAT requested it

QByteArray Ft991Protocol::encodeTX(
    const RadioResponse &response)
{
    auto value =
        std::get_if<TxResponse>(
            &response);

    if (!value)
        return {};

    switch (value->state)
    {
    case TxState::Receive:
        return "TX0;";

    case TxState::CatTransmit:
        return "TX1;";

    case TxState::RadioTransmit:
        return "TX2;";
    }

    return {};
}


