#pragma once

#include "RadioState.h"

class RadioCore
{
public:
    RadioCore() = default;

    RadioResponse execute(const RadioRequest &request);

    const RadioState &state() const
    {
        return m_state;
    }

private:
    RadioState m_state;
};