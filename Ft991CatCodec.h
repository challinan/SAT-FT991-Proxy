#pragma once

#include "RadioTypes.h"

#include <QByteArray>
#include <optional>

namespace Ft991Cat
{
std::optional<RadioMode> modeFromCat(char c);
std::optional<char> modeToCat(RadioMode mode);

QByteArray formatFrequencyCommand(
    const QByteArray &mnemonic,
    quint64 hz);

std::optional<quint64> parseFrequencyAnswer(
    const QByteArray &frame,
    const QByteArray &mnemonic);
}