/*
 * High level architecture
 * The main idea is to make the FT-991A simulator and real FT-991A look identical
 * to the CI-V side, including asynchronous completion.
 *
 *                     S.A.T.
 *                       │
 *                    CI-V bytes
 *                       │
 *                 CivProtocol
 *                       │
 *                  RadioRequest
 *                       │
 *               CivProxyController
 *                       │
 *                 RadioBackend
 *                  /          \
 *                 /            \
 *        RadioCoreBackend    Ft991Client
 *          simulator          real radio
 *                 \            /
 *                  \          /
 *                 RadioResponse
 *                       │
 *               CivProxyController
 *                       │
 *                 CivProtocol
 *                       │
 *                CI-V response
 *
 *
 * RadioCore
 *     radio behavior and state
 *
 * RadioCoreBackend
 *     asynchronous RadioBackend wrapper
 *
 * Ft991Client
 *     physical RadioBackend implementation
 *
 * CivProxyController
 *     associates CI-V transactions with
 *     backend transactions

 * CivProtocol
 *    understands Icom CI-V bytes
 *
 *
 * Assuming we already have the serial subsystem
 * So, usage should be close to to this:
 */

m_ft991Client =
    new Ft991Client(
        m_serialPort,
        this);

m_ft991Client->setTimeout(500);

connect(
    m_ft991Client,
    &Ft991Client::requestCompleted,
    this,
    [](quint64 id,
       const RadioResponse &response)
    {
        qDebug()
        << "Request"
        << id
        << "completed";
    });

connect(
    m_ft991Client,
    &Ft991Client::requestFailed,
    this,
    [](quint64 id,
       const QString &error)
    {
        qWarning()
        << "Request"
        << id
        << "failed:"
        << error;
    });

connect(
    m_ft991Client,
    &Ft991Client::catTx,
    this,
    [](const QByteArray &frame)
    {
        qDebug()
        << "CAT TX"
        << frame;
    });

connect(
    m_ft991Client,
    &Ft991Client::catRx,
    this,
    [](const QByteArray &frame)
    {
        qDebug()
        << "CAT RX"
        << frame;
    });

    //
    // After opening the port, then do:
    //
    m_ft991Client->disableAutoInformation();
    m_ft991Client->submit(
        GetFrequency {
            Vfo::A
        });

    // That should give something like this:
    //    CAT TX "FA;"
    //    CAT RX "FA144200000;"
    //    Request 1 completed

    //
    // And this submission
    m_ft991Client->submit(
        SetFrequency {
            Vfo::A,
            145925000
        });
    // Should give this
    //    CAT TX "FA145925000;"
    //    Request 2 completed
    //  With no expected CAT reply

    /*
     * Our application can now select the radio
     * For traveling, do this, and S.A.T is now
     * talking to a pretend radio
     */
    m_simulator =
        new RadioCoreBackend(this);

    m_proxyController =
        new CivProxyController(this);

    m_proxyController->setBackend(
        m_simulator);

    /*
     * At home with the real FT-991A:
     * (Note: Nothing on the CI-V side changes.)
     */
    m_ft991 =
        new Ft991Client(
            m_serialPort,
            this);

    m_proxyController->setBackend(
        m_ft991);

/*
 * Example: Frequency Query
 * Suppose S.A.T sends FE FE A2 E0 03 FD
 * The eventual CI-V decoder produces:
 */
    CivRequestContext context;

    context.radioAddress =
        0xA2;

    context.controllerAddress =
        0xE0;

    context.command =
        QByteArray::fromHex("03");

    context.originalFrame =
        QByteArray::fromHex(
            "FEFEA2E003FD");

    RadioRequest request =
        GetFrequency {
            Vfo::A
        };

    m_proxyController->submit(
        context,
        request);

    /*
     * If the simulator is selected:
     *            GetFrequency
     *                    │
     *                    ▼
     *              RadioCoreBackend
     *                    │
     *               144200000
     *                    │
     *                    ▼
     *           FrequencyResponse
     *
     *
     * If the real FT-991A is selected:
     *
     *             GetFrequency
     *                    │
     *                    ▼
     *               Ft991Client
     *                    │
     *                   FA;
     *                    │
     *                    ▼
     *                FT-991A
     *                    │
     *             FA144200000;
     *                    │
     *                    ▼
     *           FrequencyResponse
     *
     * The controller sees the exact same result in either case:
     */


    FrequencyResponse {
        Vfo::A,
        144200000
    }


    /*
     * Then the CI-V formatter takes over
     * The connection eventually looks like:
     */
    connect(
        m_proxyController,
        &CivProxyController::responseReady,

        m_civProtocol,
        &CivProtocol::handleRadioResponse);

    /*
     * CivProtocol receives:
     *
     * Original CI-V command:
     * 03

     * RadioResponse:
     *     FrequencyResponse
     *     144200000
     *
     * And generates:
     * FE FE E0 A2 03 <BCD frequency> FD
     *
     * Note the CI-V addresses have been reversed:
     * Request:

     * FE FE A2 E0 ...
     *       ^^ ^^
     *       TO FROM
     *
     * Response:

     * FE FE E0 A2 ...
     *       ^^ ^^
     *       TO FROM
     */



