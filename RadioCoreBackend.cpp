#include "RadioCoreBackend.h"

#include <QMetaObject>


RadioCoreBackend::RadioCoreBackend(
    QObject *parent)
    : RadioBackend(parent)
{
}


quint64 RadioCoreBackend::submit(
    const RadioRequest &request)
{
    const quint64 id =
        m_nextRequestId++;

    //
    // Deliberately complete asynchronously.
    //
    // That makes this backend behave just like the
    // physical FT-991A backend from the caller's
    // point of view.
    //
    QMetaObject::invokeMethod(
        this,

        [this, id, request]()
        {
            RadioResponse response =
                m_core.execute(request);

            if (auto error =
                std::get_if<ErrorResponse>(
                    &response))
            {
                emit requestFailed(
                    id,
                    error->message);

                return;
            }

            emit requestCompleted(
                id,
                response);
        },

        Qt::QueuedConnection);

    return id;
}

void RadioCoreBackend::setTimeout(int ms)
{
    m_timeoutMs = ms;
}