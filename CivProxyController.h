#pragma once

#include "RadioBackend.h"
#include "CivTypes.h"

#include <QObject>
#include <QHash>


class CivProxyController : public QObject
{
    Q_OBJECT

public:
    explicit CivProxyController(
        QObject *parent = nullptr);

    void setBackend(
        RadioBackend *backend);

    RadioBackend *backend() const
    {
        return m_backend;
    }


public slots:

    void submit(
        const CivRequestContext &context,
        const RadioRequest &request
        );


signals:

    //
    // Hand this back to CivProtocol, which knows
    // how to turn the RadioResponse into actual
    // CI-V bytes.
    //
    void responseReady(
        CivRequestContext context,
        RadioResponse response);

    void requestFailed(
        CivRequestContext context,
        QString error);


private slots:

    void backendRequestCompleted(
        quint64 requestId,
        RadioResponse response);

    void backendRequestFailed(
        quint64 requestId,
        QString error);


private:

    struct PendingRequest
    {
        CivRequestContext context;
    };


    RadioBackend *m_backend = nullptr;

    QHash<quint64, PendingRequest> m_pending;
};

