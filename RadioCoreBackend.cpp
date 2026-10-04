#include "RadioCoreBackend.h"

#include <QMetaObject>
#include <QDebug>

RadioCoreBackend::RadioCoreBackend(
    QObject *parent)
    : RadioBackend(parent)
{
    qDebug() << "RadioCoreBackend::RadioCoreBakend(): ************** CONSTRUCTOR ENTERED ************************";
}


quint64 RadioCoreBackend::submit(
    const RadioRequest &request)
{
    const quint64 id = m_nextRequestId++;
    qDebug() << "RadioCoreBackend::submit(): Entered: RequestID =" << id;

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
            RadioResponse response = m_core.execute(request);

            if (auto error = std::get_if<ErrorResponse>(&response)) {
                emit requestFailed(id, error->message);

                return;
            }

            emit requestCompleted(id, response);
        },

        Qt::QueuedConnection);

    return id;
}

void RadioCoreBackend::setTimeout(int ms)
{
    m_timeoutMs = ms;
}