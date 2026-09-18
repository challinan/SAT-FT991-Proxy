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


I'm at "Great, Let's see what it looks like"