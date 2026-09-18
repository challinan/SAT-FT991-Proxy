#include "Ft991CatCodec.h"

#include <algorithm>


std::optional<RadioMode>
Ft991Cat::modeFromCat(char c)
{
    switch (c)
    {
    case '1': return RadioMode::Lsb;
    case '2': return RadioMode::Usb;
    case '3': return RadioMode::CwU;
    case '4': return RadioMode::Fm;
    case '5': return RadioMode::Am;
    case '6': return RadioMode::RttyLsb;
    case '7': return RadioMode::CwL;
    case '8': return RadioMode::DataLsb;
    case '9': return RadioMode::RttyUsb;
    case 'A': return RadioMode::DataFm;
    case 'B': return RadioMode::FmNarrow;
    case 'C': return RadioMode::DataUsb;
    case 'D': return RadioMode::AmNarrow;
    case 'E': return RadioMode::C4fm;
    }

    return std::nullopt;
}


std::optional<char>
Ft991Cat::modeToCat(RadioMode mode)
{
    switch (mode)
    {
    case RadioMode::Lsb:      return '1';
    case RadioMode::Usb:      return '2';
    case RadioMode::CwU:      return '3';
    case RadioMode::Fm:       return '4';
    case RadioMode::Am:       return '5';
    case RadioMode::RttyLsb:  return '6';
    case RadioMode::CwL:      return '7';
    case RadioMode::DataLsb:  return '8';
    case RadioMode::RttyUsb:  return '9';
    case RadioMode::DataFm:   return 'A';
    case RadioMode::FmNarrow: return 'B';
    case RadioMode::DataUsb:  return 'C';
    case RadioMode::AmNarrow: return 'D';
    case RadioMode::C4fm:     return 'E';
    }

    return std::nullopt;
}


QByteArray
Ft991Cat::formatFrequencyCommand(
    const QByteArray &mnemonic,
    quint64 hz)
{
    QByteArray freq =
        QByteArray::number(hz)
            .rightJustified(9, '0');

    return mnemonic + freq + ';';
}


std::optional<quint64>
Ft991Cat::parseFrequencyAnswer(
    const QByteArray &frame,
    const QByteArray &mnemonic)
{
    //
    // Example:
    //
    // FA144200000;
    //
    // 2 command chars
    // 9 frequency digits
    // 1 terminator
    //

    if (frame.size() != 12)
        return std::nullopt;

    if (!frame.startsWith(mnemonic))
        return std::nullopt;

    if (!frame.endsWith(';'))
        return std::nullopt;

    QByteArray digits = frame.mid(2, 9);

    const bool allDigits =
        std::all_of(
            digits.begin(),
            digits.end(),
            [](char c)
            {
                return c >= '0' && c <= '9';
            });

    if (!allDigits)
        return std::nullopt;

    bool ok = false;

    quint64 hz =
        digits.toULongLong(&ok);

    if (!ok)
        return std::nullopt;

    return hz;
}