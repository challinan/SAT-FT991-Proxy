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


using RadioRequest = std::variant<
    GetFrequency,
    SetFrequency,
    GetMode,
    SetMode,
    GetRfPower,
    SetRfPower,
    GetTx,
    SetTx
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
    int percent;
};

struct TxResponse
{
    TxState state;
};

struct AckResponse
{
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