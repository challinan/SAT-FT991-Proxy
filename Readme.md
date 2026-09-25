On the Yaesu FT-991A, the FT (Function TX) CAT command determines which VFO is used to transmit when you press the PTT button.
Unlike the dual-receiver Icom IC-9700, the Yaesu FT-991A is a single-receiver radio. However, it still maintains two distinct frequency memories: VFO-A and VFO-B. The FT command controls how the radio handles these VFOs during Split operations (transmitting on a different frequency than you are listening to).
The Two Settings Explained
1. FT0; (VFO-A Band Transmitter TX)
• What it does: Sets VFO-A as your transmit frequency.
• Normal Operation: In standard, non-split operation, you listen on VFO-A and transmit on VFO-A.
• Reverse Split: If Split mode is enabled, this setting means you will receive on VFO-B and transmit on VFO-A.
2. FT1; (VFO-B Band Transmitter TX)
• What it does: Sets VFO-B as your transmit frequency.
• Standard Split Operation: This is the most common setting for working DX split or using split-frequency repeaters. It tells the radio: Receive/listen on VFO-A, but instantly flip to VFO-B when transmitting.

While the Icom uses "Main" and "Sub" to switch between two entirely independent physical receivers, the Yaesu FT-991A uses FT (and its counterpart FR for Function RX) to manage a single synthesizer pipeline.
• To Listen on A and Transmit on B (Standard Split):
        • Set Receive to VFO-A: FR0;
        • Set Transmit to VFO-B: FT1;
• To Listen on B and Transmit on A (Reverse Split):
        • Set Receive to VFO-B: FR1;
        • Set Transmit to VFO-A: FT0;
        

The design uses a static command-definition table plus a separate radio-state model. 
The command table describes the protocol; the state model contains the current 
simulated/actual radio state.

# High Level Architecture:

              ┌───────────────────┐
FT-991A CAT → │ FT991 CAT Decoder │
              └─────────┬─────────┘
                        │
                        ▼
                 RadioRequest
                        │
                  ┌─────▼─────┐
                  │ RadioCore │
                  │   State   │
                  └─────┬─────┘
                        │
                 RadioResponse
                        │
          ┌─────────────┴─────────────┐
          ▼                           ▼
   FT991 Formatter              CI-V Formatter
          │                           │
     ASCII CAT                    binary CI-V


-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

# Command Vocabulary

CI-V                Internal                 Yaesu CAT

03 ...       -->    GetFrequency      -->    FA;
05 ...       -->    SetFrequency      -->    FA145900000;

04 ...       -->    GetMode           -->    MD0;
06 ...       -->    SetMode           -->    MD04;

1C 00 00     -->    SetTx(false)      -->    TX0;
1C 00 01     -->    SetTx(true)       -->    TX1;


Simulator Functional Path

S.A.T. serial data
       │
       ▼
CivProtocol::feedBytes()
       │
       ▼
decode CI-V command
       │
       ▼
emit radioRequest(context, request)
       │
       ▼
CivProxyController::submit()
       │
       ▼
RadioBackend::submit()
       │
       ├── RadioCoreBackend      simulator
       │
       └── Ft991Client           real FT-991A


# Thread model
keep all protocol traffic through one thread/queue. Eventually the CI-V interface
the FT-991A serial interface, the GUI, and perhaps the simulator may all want 
to touch RadioCore. Either keep RadioCore owned by the communications 
worker thread (preferred), or protect its state with a QMutex. 
Don't let the UI directly manipulate RadioState.

# Symmetrical Architecture


                   RadioRequest
                        │
             ┌──────────┴──────────┐
             │                     │
       RadioCore              Ft991Client
       simulator                    │
             │                Yaesu CAT
             │                     │
       RadioResponse          Real FT-991A
                                   │
                              RadioResponse


# Architecture refinement
The main idea is to make the simulator and real FT-991A look identical t
o the CI-V side, including asynchronous completion.

RadioCore is the state/behavior engine, while:
RadioCoreBackend is the simulator-facing radio.


                    S.A.T.
                      │
                   CI-V bytes
                      │
                CivProtocol
                      │
                 RadioRequest
                      │
              CivProxyController
                      │
                RadioBackend
                 /          \
                /            \
       RadioCoreBackend    Ft991Client
         simulator          real radio
                \            /
                 \          /
                RadioResponse
                      │
              CivProxyController
                      │
                CivProtocol
                      │
               CI-V response
               
# Example command flow
S.A.T.
  │
  │ FE FE A2 E0 14 0A FD
  ▼
CivProtocol
  │
  │ GetRfPower
  ▼
CivProxyController
  │
  ▼
Ft991Client
  │
  │ PC;
  ▼
FT-991A
  │
  │ PC040;
  ▼
RfPowerResponse { 40 }
  │
  ▼
CivProtocol
  │
  │ FE FE E0 A2 14 0A 01 28 FD
  ▼
S.A.T.