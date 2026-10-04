#pragma once

#include <QtGlobal>
#include <QString>
#include <QMetaType>

#include <variant>


enum class Vfo
{
    A,
    B
};


enum class RadioMode
{
    Lsb,
    Usb,
    CwU,
    Fm,
    Am,
    RttyLsb,
    CwL,
    DataLsb,
    RttyUsb,
    DataFm,
    FmNarrow,
    DataUsb,
    AmNarrow,
    C4fm
};


enum class TxState
{
    Receive,
    CatTransmit,
    RadioTransmit
};


// ---------- Requests ----------

struct GetFrequency
{
    Vfo vfo;
};

struct SetFrequency
{
    Vfo vfo;
    quint64 hz;
};

struct GetMode
{
    Vfo vfo;
};

struct SetMode
{
    Vfo vfo;
    RadioMode mode;
};

struct SelectVfoMode {};

struct GetRfPower
{
};

struct SetRfPower
{
    int percent;
};

struct GetTx
{
};

struct SetTx
{
    bool transmit;
};

/*
 * FT command is used to set split on or off as follows:
 * FT2; sets split off
 * FT3; sets split on, with VFO-B as TX
 * Note FT-991A does not support TX on VFO-A while in split more
 */
struct SetSplit{
    bool split;
};

struct SetVfoModeViaMA{
};

struct SetMemChannel{
    int channel;
};

struct SwapAB {};

/*
 *  NOTE: when adding here, be sure to add to the helper
 * routine MainWindow::radioRequestTypeName()
 * as well as encodeRequest in Ft991Client.cpp
 */
using RadioRequest = std::variant<
    GetFrequency,
    SetFrequency,
    SelectVfoMode,
    GetMode,
    SetMode,
    GetRfPower,
    SetRfPower,
    GetTx,
    SetTx,
    SetSplit,
    SetVfoModeViaMA,
    SetMemChannel,
    SwapAB
    >;


// ---------- Responses ----------

struct FrequencyResponse
{
    Vfo vfo;
    quint64 hz;
};

struct ModeResponse
{
    Vfo vfo;
    RadioMode mode;
};

struct RfPowerResponse
{
    int watts;
};

struct TxResponse
{
    TxState state;
};

struct AckResponse
{
    bool ackReceived;
};

struct ErrorResponse
{
    QString message;
};


using RadioResponse = std::variant<
    FrequencyResponse,
    ModeResponse,
    RfPowerResponse,
    TxResponse,
    AckResponse,
    ErrorResponse
    >;


Q_DECLARE_METATYPE(RadioRequest)
Q_DECLARE_METATYPE(RadioResponse)