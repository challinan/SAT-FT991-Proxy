#pragma once

#include "RadioProtocol.h"
#include "RadioCore.h"

#include <QHash>

class Ft991Protocol : public RadioProtocol
{
public:
    explicit Ft991Protocol(RadioCore &radio);

    QByteArray processFrame(
        const QByteArray &frame,
        QString *error = nullptr
        ) override;


private:

    struct DecodedCommand
    {
        RadioRequest request { GetTx {} };

        // True for CAT Read commands.
        // False for CAT Set commands.
        bool wantsResponse = false;
    };


    using DecodeFunction =
        bool (*)(
            const QByteArray &payload,
            DecodedCommand &decoded,
            QString &error
            );

    using EncodeFunction =
        QByteArray (*)(
            const RadioResponse &response
            );


    struct CommandDefinition
    {
        QByteArray mnemonic;
        Vfo vfo;
        DecodeFunction decode;
        EncodeFunction encode;
    };


    RadioCore &m_radio;

    QHash<QByteArray, CommandDefinition> m_commands;


    // ---- Command decoders ----

    static bool decodeFA(
        const QByteArray &payload,
        DecodedCommand &decoded,
        QString &error);

    static bool decodeFB(
        const QByteArray &payload,
        DecodedCommand &decoded,
        QString &error);

    static bool decodeMD(
        const QByteArray &payload,
        // Vfo vfo,
        DecodedCommand &decoded,
        QString &error);

    static bool decodeTX(
        const QByteArray &payload,
        DecodedCommand &decoded,
        QString &error);


    // ---- Response encoders ----

    static QByteArray encodeFA(
        const RadioResponse &response);

    static QByteArray encodeFB(
        const RadioResponse &response);

    static QByteArray encodeMD(
        const RadioResponse &response);

    static QByteArray encodeTX(
        const RadioResponse &response);


    // ---- Utility functions ----

    static bool decodeFrequency(
        const QByteArray &payload,
        Vfo vfo,
        DecodedCommand &decoded,
        QString &error);

    static QByteArray encodeFrequency(
        const RadioResponse &response,
        const QByteArray &mnemonic);

    static bool modeFromCat(
        char c,
        RadioMode &mode);

    static char modeToCat(
        RadioMode mode);
};