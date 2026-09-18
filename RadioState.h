#pragma once

#include "RadioTypes.h"

/*
 * This represents the radio state
 */

struct RadioState
{
    quint64 frequencyA = 144200000;
    quint64 frequencyB = 432100000;

    RadioMode modeA = RadioMode::Usb;
    RadioMode modeB = RadioMode::Fm;
    int rfPowerPercent = 40;

    Vfo activeRxVfo = Vfo::A;

    TxState txState = TxState::Receive;


    quint64 frequency(Vfo vfo) const
    {
        return (vfo == Vfo::A) ? frequencyA : frequencyB;
    }

    void setFrequency(Vfo vfo, quint64 hz)
    {
        if (vfo == Vfo::A)
            frequencyA = hz;
        else
            frequencyB = hz;
    }


    RadioMode mode(Vfo vfo) const
    {
        return (vfo == Vfo::A) ? modeA : modeB;
    }

    void setMode(Vfo vfo, RadioMode mode)
    {
        if (vfo == Vfo::A)
            modeA = mode;
        else
            modeB = mode;
    }
};