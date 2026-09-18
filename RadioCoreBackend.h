#pragma once

#include "RadioBackend.h"
#include "RadioCore.h"


class RadioCoreBackend : public RadioBackend
{
    Q_OBJECT

public:
    explicit RadioCoreBackend(
        QObject *parent = nullptr);

    quint64 submit(
        const RadioRequest &request) override;

    const RadioState &state() const
    {
        return m_core.state();
    }


private:

    RadioCore m_core;
    void setTimeout(int ms) override;
    quint64 m_nextRequestId = 1;
    int m_timeoutMs;

};