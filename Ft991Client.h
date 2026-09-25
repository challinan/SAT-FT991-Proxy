#pragma once

#include "RadioBackend.h"
#include "RadioTypes.h"

#include <QObject>
#include <QIODevice>
#include <QQueue>
#include <QTimer>
#include <QSerialPort>

#include <optional>

/*
 * I particularly like returning a requestId. Later the CI-V side can keep:
 *
 * CI-V request #17
 *         ↓
 *         RadioRequest
 *         ↓
 *         FT991Client request #42
 *         ↓
 *         FT-991A
 *
 *     and associate the eventual RadioResponse with the original CI-V transaction.
 */

class Ft991Client : public RadioBackend
{
    Q_OBJECT

public:
    explicit Ft991Client(
        QIODevice *device,
        QObject *parent = nullptr);

    //
    // Queue a protocol-independent radio request.
    //
    // Returns an ID which is passed back with the
    // completion/failure signal.
    //
    quint64 submit(const RadioRequest &request) override;

    void setTimeout(int milliseconds) override;

    //
    // I recommend doing this when we take control
    // of a real FT-991A.
    //
    void disableAutoInformation();

public slots:
    void feedBytes(
        const QByteArray &data);

signals:
#if 0
    void requestCompleted(
        quint64 requestId,
        RadioResponse response);

    void requestFailed(
        quint64 requestId,
        QString error);
#endif

    //
    // Anything received that isn't the answer to
    // the current query lands here.
    //
    void unsolicitedFrame(
        QByteArray frame);

    //
    // Very useful while developing.
    //
    void catTx(QByteArray frame);
    void catRx(QByteArray frame);

    // UI Updates - Radio is alove
    void firstValidFrameReceived();


private slots:

    void onTimeout();


private:

    struct EncodedRequest
    {
        QByteArray command;

        //
        // Empty means this CAT command does not
        // produce an answer.
        //
        QByteArray expectedPrefix;
    };


    struct PendingRequest
    {
        quint64 id;
        RadioRequest request;
        EncodedRequest encoded;
    };


    QIODevice *m_device = nullptr;

    QQueue<PendingRequest> m_queue;

    std::optional<PendingRequest> m_active;

    QByteArray m_rxBuffer;

    QTimer m_timeoutTimer;

    int m_timeoutMs = 500;

    quint64 m_nextRequestId = 1;
    bool first_valid_frame = 0;



    void pumpQueue();

    std::optional<EncodedRequest>
    encodeRequest(
        const RadioRequest &request,
        QString &error) const;

    std::optional<RadioResponse>
    decodeResponse(
        const RadioRequest &request,
        const QByteArray &frame,
        QString &error) const;

    bool frameMatchesActiveRequest(const QByteArray &frame) const;

    void finishActive(
        const RadioResponse &response);

    void failActive(const QString &error);

    QString radioRequestTypeName(const RadioRequest &request);
};