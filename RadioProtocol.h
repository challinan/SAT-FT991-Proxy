#pragma once

#include <QByteArray>
#include <QString>

/*
* A Tiny Common Interface
*/
class RadioProtocol
{
public:
    virtual ~RadioProtocol() = default;

    virtual QByteArray processFrame(
        const QByteArray &frame,
        QString *error = nullptr
        ) = 0;
};