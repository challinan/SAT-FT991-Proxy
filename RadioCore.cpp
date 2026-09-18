#include "RadioCore.h"

#include <type_traits>

RadioResponse RadioCore::execute(const RadioRequest &request)
{
    return std::visit(
        [this](const auto &req) -> RadioResponse
        {
            using T = std::decay_t<decltype(req)>;

            if constexpr (std::is_same_v<T, GetFrequency>)
            {
                return FrequencyResponse {
                    req.vfo,
                    m_state.frequency(req.vfo)
                };
            }

            else if constexpr (std::is_same_v<T, SetFrequency>)
            {
                m_state.setFrequency(req.vfo, req.hz);

                return AckResponse {};
            }

            else if constexpr (std::is_same_v<T, GetMode>)
            {
                return ModeResponse {
                    req.vfo,
                    m_state.mode(req.vfo)
                };
            }

            else if constexpr (std::is_same_v<T, SetMode>)
            {
                m_state.setMode(
                    req.vfo,
                    req.mode
                    );

                return AckResponse {};
            }

            else if constexpr (std::is_same_v<T, GetTx>)
            {
                return TxResponse {
                    m_state.txState
                };
            }

            else if constexpr (std::is_same_v<T, SetTx>)
            {
                m_state.txState =
                    req.transmit
                        ? TxState::CatTransmit
                        : TxState::Receive;

                return AckResponse {};
            }

            else
            {
                return ErrorResponse {
                    QStringLiteral("Unsupported radio request")
                };
            }
        },
        request
        );
}