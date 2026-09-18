#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QtGlobal>


struct CivRequestContext
{
    //
    // Original CI-V addressing
    //
    quint8 radioAddress = 0xA2;
    quint8 controllerAddress = 0xE0;

    //
    // Complete command body but without:
    //
    // FE FE <to> <from>
    //
    // and without FD.
    //
    // Examples:
    //
    // 03
    //
    // 14 0A
    //
    // 16 58
    //
    QByteArray command;

    //
    // Optional: useful for debugging/logging.
    //
    QByteArray originalFrame;
};


Q_DECLARE_METATYPE(CivRequestContext)