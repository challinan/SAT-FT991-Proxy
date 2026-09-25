#include "CivProxyController.h"
#include <QDebug>


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


    /*
     * If we're changing radios while requests are
     * outstanding, those old requests are no longer
     * meaningful.
     */
    if (!m_pending.isEmpty()) {
        const auto pending = m_pending;

        m_pending.clear();

        for (const PendingRequest &request : pending)
        {
            emit requestFailed(
                request.context,
                QStringLiteral("CivProxyController::setBackend(): Radio backend changed"));
        }
    }


    // Disconnect old backend.
    if (m_backend) {
        disconnect(m_backend, nullptr, this, nullptr);
    }

    m_backend = backend;

    if (!m_backend)
        return;

    qDebug() << "CivProxyController::setBackend(): Setting up connections";
    connect(
        m_backend,
        &RadioBackend::requestCompleted,
        this,
        &CivProxyController::backendRequestCompleted);

    connect(
        m_backend,
        &RadioBackend::requestFailed,
        this,
        &CivProxyController::backendRequestFailed);
}

void CivProxyController::submit(
    const CivRequestContext &context,
    const RadioRequest &request)
{
    qDebug() << "CivProxyController::submit(): m_backend:" << m_backend;
    if (!m_backend) {
        emit requestFailed(context,
            QStringLiteral("CivProxyController::submit(): No radio backend selected"));

        return;
    }

    quint64 requestId = m_backend->submit(request);
    qDebug() << "CivProxyController::submit(): backend called radio submit(): requestID:" << requestId;

    // Insert this PendingRequest into our hash lookup table
    m_pending.insert(requestId, PendingRequest {context});
}

void CivProxyController::backendRequestCompleted(
        quint64 requestId,
        RadioResponse response)
{
    qDebug() << "CivProxyController::backendRequestCompleted(): Entered with requestID:" << requestId;
    auto it = m_pending.find(requestId);

    if (it == m_pending.end()) {
        /*
         * Could log this. It means a backend returned
         * an ID we no longer know about.
         */
        return;
    }


    CivRequestContext context = it->context;

    m_pending.erase(it);

    qDebug() << "CivProxyController::backendRequestCompleted(): About to emit responseReady";
    emit responseReady(context, response);  // Slot: CivProtocol::handleRadioResponse()
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

    emit requestFailed(context, error);
}

