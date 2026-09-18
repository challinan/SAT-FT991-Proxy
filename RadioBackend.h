#pragma once

#include "RadioTypes.h"

#include <QObject>


class RadioBackend : public QObject
{
    Q_OBJECT

public:
    explicit RadioBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual ~RadioBackend() = default;

    virtual quint64 submit(
        const RadioRequest &request) = 0;

    virtual void setTimeout(int milliseconds) = 0;


signals:

    void requestCompleted(
        quint64 requestId,
        RadioResponse response);

    void requestFailed(
        quint64 requestId,
        QString error);

};