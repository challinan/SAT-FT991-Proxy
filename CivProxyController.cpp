#include "CivProxyController.h"


CivProxyController::CivProxyController(
    QObject *parent)
    : QObject(parent)
{
}


void CivProxyController::setBackend(
    RadioBackend *backend)
{
    if (m_backend == backend)
        return;


    //
    // If we're changing radios while requests are
    // outstanding, those old requests are no longer
    // meaningful.
    //
    if (!m_pending.isEmpty())
    {
        const auto pending =
            m_pending;

        m_pending.clear();

        for (const PendingRequest &request :
             pending)
        {
            emit requestFailed(
                request.context,
                QStringLiteral(
                    "Radio backend changed"));
        }
    }


    //
    // Disconnect old backend.
    //
    if (m_backend)
    {
        disconnect(
            m_backend,
            nullptr,
            this,
            nullptr);
    }


    m_backend = backend;


    if (!m_backend)
        return;


    connect(
        m_backend,
        &RadioBackend::requestCompleted,

        this,
        &CivProxyController::
        backendRequestCompleted);


    connect(
        m_backend,
        &RadioBackend::requestFailed,

        this,
        &CivProxyController::
        backendRequestFailed);
}

void CivProxyController::submit(
    const CivRequestContext &context,
    const RadioRequest &request)
{
    if (!m_backend)
    {
        emit requestFailed(
            context,
            QStringLiteral(
                "No radio backend selected"));

        return;
    }


    quint64 requestId =
        m_backend->submit(request);


    m_pending.insert(
        requestId,
        PendingRequest {
            context
        });
}

void CivProxyController::
    backendRequestCompleted(
        quint64 requestId,
        RadioResponse response)
{
    auto it =
        m_pending.find(requestId);

    if (it == m_pending.end())
    {
        //
        // Could log this. It means a backend returned
        // an ID we no longer know about.
        //
        return;
    }


    CivRequestContext context =
        it->context;

    m_pending.erase(it);


    emit responseReady(
        context,
        response);
}

void CivProxyController::
    backendRequestFailed(
        quint64 requestId,
        QString error)
{
    auto it =
        m_pending.find(requestId);

    if (it == m_pending.end())
        return;


    CivRequestContext context =
        it->context;

    m_pending.erase(it);


    emit requestFailed(
        context,
        error);
}

