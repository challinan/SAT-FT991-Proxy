/*
 * This class doesn't own your serial port. Give received serial
 * bytes to feedBytes() and connect frameReady() to your
 * existing serial subsystem.
 */

#pragma once

#include "CivTypes.h"
#include "RadioTypes.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <optional>


class CivProtocol : public QObject
{
    Q_OBJECT

public:
    explicit CivProtocol(
        QObject *parent = nullptr);

    void setRadioAddress(quint8 address)
    {
        m_radioAddress = address;
    }

    quint8 radioAddress() const
    {
        return m_radioAddress;
    }


public slots:

    //
    // Feed arbitrary chunks of serial data.
    //
    // One call can contain:
    //
    //   half a frame
    //   exactly one frame
    //   multiple frames
    //   garbage + frames
    //
    void feedBytes(
        const QByteArray &data);


    //
    // Called by CivProxyController after the
    // backend completes a RadioRequest.
    //
    void handleRadioResponse(
        CivRequestContext context,
        RadioResponse response);

    void handleRadioFailure(
        CivRequestContext context,
        QString error);


signals:

    void firstValidFrameReceived();

    //
    // Send to CivProxyController.
    //
    void radioRequest(
        CivRequestContext context,
        RadioRequest request);


    //
    // Send this to the physical CI-V serial port.
    //
    void frameReady(
        QByteArray frame);


    //
    // Diagnostics.
    //
    void frameReceived(
        QByteArray frame);

    void unsupportedFrame(
        QByteArray frame);

    void protocolError(
        QString error);

    void updatePowerLabel(QString S);


private:

    quint8 m_radioAddress = 0xA2;
    bool first_valid_frame = 0;

    QByteArray m_rxBuffer;


    //
    // Virtual IC-9700 state.
    //
    // These have no direct FT-991A equivalent.
    //
    bool m_satelliteMode = false;

    //
    // 16 58:
    // 00 = WIDE
    // 01 = MID
    // 02 = NAR
    //
    quint8 m_ssbTxBandwidth = 0x01;

    //
    // 1A 05 0131
    //
    bool m_dataEchoBack = false;


    //
    // Proxy policy:
    //
    // IC-9700 MAIN -> FT-991A VFO A
    // IC-9700 SUB  -> FT-991A VFO B
    //
    Vfo m_selectedVfo = Vfo::A;


    void processFrame(
        const QByteArray &frame);

    void processCommand(
        const CivRequestContext &context);


    void handleFrequencyRead(
        const CivRequestContext &context);

    void handleFrequencySet(
        const CivRequestContext &context);

    void handleModeRead(
        const CivRequestContext &context);

    void handleModeSet(
        const CivRequestContext &context);

    void handleVfoCommand(
        const CivRequestContext &context);

    void handle14(
        const CivRequestContext &context);

    void handle16(
        const CivRequestContext &context);

    void handle1A(
        const CivRequestContext &context);


    // ---------- Framing ----------

    QByteArray makeResponse(
        const CivRequestContext &context,
        const QByteArray &body) const;

    void sendResponse(
        const CivRequestContext &context,
        const QByteArray &body);

    void sendAck(
        const CivRequestContext &context);

    void sendNg(
        const CivRequestContext &context);


    // ---------- CI-V codecs ----------

    static std::optional<quint64>
    decodeFrequency(
        const QByteArray &data);

    static QByteArray
    encodeFrequency(
        quint64 hz);

    static std::optional<RadioMode>
    decodeMode(
        quint8 mode,
        quint8 filter);

    static std::optional<QByteArray>
    encodeMode(
        RadioMode mode);

    static std::optional<int>
    decodeLevel(
        const QByteArray &data);

    static QByteArray
    encodeLevel(
        int value);

    static quint8 byteAt(
        const QByteArray &data,
        qsizetype index);
};

