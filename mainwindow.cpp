#include "mainwindow.h"
#include "SerialPort.h"
#include "ui_mainwindow.h"
#include "terminal_debug.h"
#include <QFontDatabase>

// Radio not available, use the simulator
// (Defined in FT991A-emulator.pro file)
#ifdef ENABLE_FT991_SIM
#include "RadioCoreBackend.h"
#else
#include "Ft991Client.h"
#endif
#include <QOperatingSystemVersion>
#include <QLibraryInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    qDebug() << QOperatingSystemVersion::current();
    qDebug() << QLibraryInfo::version();
    qDebug() << QLibraryInfo::path(QLibraryInfo::LibrariesPath);

    qRegisterMetaType<RadioRequest>(
        "RadioRequest");

    qRegisterMetaType<RadioResponse>(
        "RadioResponse");

    qDebug() << "Command line args:" << QApplication::arguments();
    // Initialize UI
    ui->setupUi(this);
    initializeUiLabels();

    // Initialize configuration subsystem
    config_p = new ConfigObject;
    config_p->debug_display_map();  // For debug only

    // Trace file
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_loggerPath.append("-");
    m_loggerPath.append(timestamp);

    // Start it running
    startServices();
}

MainWindow::~MainWindow()
{
    delete config_p;
    // delete network_comms_p;
    delete ui;
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << event;  // Silence the param not used warning
#if 0
    if ( m_radioBackend ) {
        m_radioBackend->submit(
            SetSplit{ false });

        m_radioBackend->submit(
            SetMemChannel {20});
    }
    else {
        qDebug() << "MainWindow::~MainWindow(): radio backend has already been destroyed";
    }
#endif
}


void MainWindow::on_config_pButton_clicked()
{
    config_p->open_config_dialog();
}

void MainWindow::initializeUiLabels() {
    QFont myFont( "Arial", 18, QFont::Bold);
    ui->appLabel->setFont(myFont);
    ui->appLabel->setText("S.A.T Proxy");
    this->setWindowTitle("S.A.T Proxy");
    ui->serialPortLabel->setText("Select Serial Port");
    ui->rigPortLabel->setText("Select Rig");
    ui->audioPortLabel->setText("Select Audio Port");
    ui->run_pButton->setDefault(true);
    ui->run_pButton->setAutoDefault(true);

    // Setup the Frequency displays
    // Inside your MainWindow constructor:
    int fontId = QFontDatabase::addApplicationFont(":/fonts/ds-digii.ttf"); // Path to your resource
    if (fontId != -1) {
        QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);
        QFont digitalFont(family, 16); // Set your custom size here

        // Now you can easily make it explicitly BOLD!
        digitalFont.setBold(true);
        ui->lcdFrequencyMain->setFont(digitalFont);
        ui->lcdFrequencySub->setFont(digitalFont);
    }

    // Style it with the exact color scheme you want
    ui->lcdFrequencyMain->setStyleSheet("color: #000080; background-color: lightGray; qproperty-alignment: 'AlignRight | AlignVCenter';");
    ui->lcdFrequencySub->setStyleSheet("color: #000080; background-color: lightGray; qproperty-alignment: 'AlignRight | AlignVCenter';");


    ui->labelPower->setText("50%");

    // Initialize the status LEDs
    ui->ledSAT_ready->setLedColor(Qt::red);
    ui->ledSAT_ready->setIsOn(true);
    ui->ledFT991A_ready->setLedColor(Qt::red);
    ui->ledFT991A_ready->setIsOn(true);
    ui->ledMainActive->setLedColor(Qt::gray);
    ui->ledSubActive->setLedColor(Qt::gray);

#ifdef ENABLE_FT991_SIM
    ui->rigTypeLabel->setText("Simulator");
    ui->labelSATSerialPort->setText("Unknown");
    ui->labelFT991SerialPort->setText("Unknown");
#else
    ui->rigTypeLabel->setText("FT991A");
    ui->labelSATSerialPort->setText("Unknown");
    ui->labelFT991SerialPort->setText("Unknown");
#endif

    updateRFPowerDisplay(0);
}

void MainWindow::on_run_pButton_clicked() {
    qDebug() << "MainWindow::on_run_pButton_clicked() Entered!";

}

void MainWindow::startServices()
{
    bool rc;

#ifdef USE_TERM_DEBUGLOGGER
    m_pD = new TerminalDebug(m_loggerPath, this);
#else
    m_pD = nullptr;
#endif
    // Hook everything together
    m_civSerial = new SerialPort(this, m_pD);
    m_civSerial->setPortHumanName("SAT");
    m_civ = new CivProtocol(this);
    m_civ->init();  // Establish initial state
    m_proxy = new CivProxyController(this);

    // If we are configured for Debug Terminal, connect it.
    if ( m_pD ) {
        connect(m_civ,
                &CivProtocol::sendToLogger,
                m_pD,
                &TerminalDebug::incomingDebugData);

        connect(m_civ,
                &CivProtocol::updateMainSubLeds,
                m_pD,
                &TerminalDebug::updateBand);
    }

    // S.A.T. serial RX
    connect(
        m_civSerial,
        &SerialPort::bytesReceived,
        m_civ,
        &CivProtocol::feedBytes);

    // CI-V response back to S.A.T.
    connect(
        m_civ,
        &CivProtocol::frameReady,
        m_civSerial,
        &SerialPort::write);

    // CI-V wants something from the radio
    connect(
        m_civ,
        &CivProtocol::radioRequest,
        m_proxy,
        &CivProxyController::submit);


    // Radio replied
    connect(
        m_proxy,
        &CivProxyController::responseReady,
        m_civ,
        &CivProtocol::handleRadioResponse);


    // Radio request failed
    connect(
        m_proxy,
        &CivProxyController::requestFailed,
        m_civ,
        &CivProtocol::handleRadioFailure);


    // During travel:
#ifdef ENABLE_FT991_SIM
    m_radioBackend = new RadioCoreBackend(this);

    m_radioBackend->setTimeout(500);
#else
    // At home:
    m_ft991Serial = new SerialPort(this, m_pD);
    m_ft991Serial->setPortHumanName("FT991A");

    rc = openFT991Serial();
    if ( !rc ) {
        qDebug() << "MainWindow::startServices(): Serial Port open failed";
        throw std::runtime_error("MainWindow::startServices(): Fatal: FT991A Serial Port open failed");
    }

    // qDebug() << "****************This is device()" << m_ft991Serial->device();
    m_ft991Client =
        new Ft991Client(m_ft991Serial->device(), this);

    // If we are configured for Debug Terminal, connect it.
    if ( m_pD ) {
        if ( connect(
                m_ft991Client,
                &Ft991Client::sendToLogger,
                m_pD,
                &TerminalDebug::incomingDebugData) ) {
            qDebug() << "Mainwindow::startServices(): Connection succeeded sendToLogger->incomingDebugData for Ft991client";
        }

        if ( connect(
                m_civSerial,
                &SerialPort::sendToLogger,
                m_pD,
                &TerminalDebug::incomingDebugData) ) {
            qDebug() << "Mainwindow::startServices(): Connection succeeded sendToLogger->incomingDebugData for civSerial";
        }
    }

    m_radioBackend = m_ft991Client;

    // m_ft991Client->disableAutoInformation();
    m_radioBackend->setTimeout(500);

    // FT-991A Serial Port RX
    connect(
        m_ft991Serial,
        &SerialPort::bytesReceived,
        m_ft991Client,
        &Ft991Client::feedBytes);

    // Debug only
    connect(
        m_radioBackend,
        &Ft991Client::requestCompleted,
        this,
        [](quint64 id, const RadioResponse &response) {
            qDebug() << "Ft991Client: Request" << id << "completed" << radioResponseTypeName(response);
        });

    // Status monitoring for GUI
    connect(
        m_radioBackend,
        &RadioBackend::requestCompleted,
        this,
        &MainWindow::radioRequestCompleted);

    connect(
        m_ft991Client,
        &Ft991Client::requestFailed,
        this,
        [](quint64 id,
           const QString &error)
        {
            qDebug() << "MainWindow::startServices(): SIGNAL: Ft991Client::requestFailed: Request" << id << "failed:" << error;
        });

    connect(
        m_ft991Client,
        &Ft991Client::catTx,
        this,
        [](const QByteArray &frame)
        {
            qDebug() << "MainWindow::startServices(): SIGNAL catTX ->" << frame;
        });

    connect(
        m_ft991Client,
        &Ft991Client::unsolicitedFrame,
        this,
        [](const QByteArray &frame)
        {
            qDebug() << "MainWindow::startServices(): Unsolicited Frame in Ft991Client::feedBytes() ->" << frame;
        });

    connect(
        m_ft991Client,
        &Ft991Client::catRx,
        this,
        [](const QByteArray &frame)
        {
            qDebug() << "MainWindow::startServices(): CAT RX" << frame;
        });

    // Report on TxData Transmitted
    connect(m_ft991Serial, &SerialPort::bytesTransmitted,
            this, [this] (const QByteArray &frame) {
                qDebug().noquote() << "SIGNAL: FT991 CAT TXDATA TRANSMITTED:" << m_ft991Serial->portName()
                << frame.toHex(' ') << "ASCII:" << QString::fromLatin1(frame);
            });

    connect(m_civSerial, &SerialPort::bytesTransmitted,
            this, [this] (const QByteArray &frame) {
                qDebug() << "SIGNAL: TXDATA TRANSMITTED:" << m_civSerial->portName() << frame;
            });

    connect(
        m_ft991Client,
        &Ft991Client::firstValidFrameReceived,
        this,
        [this]() {
            ui->ledFT991A_ready->setLedColor(Qt::green);
        });

#endif

    m_proxy->setBackend(m_radioBackend);
    qDebug() << "MainWindow::startServices(): backend setup" << m_radioBackend;

    connect(
        m_civ,
        &CivProtocol::firstValidFrameReceived,
        this,
        [this]() {
            ui->ledSAT_ready->setLedColor(Qt::green);
        }
    );

    // Unsupported Frame from CivProtocol
    connect(
        m_civ,
        &CivProtocol::unsupportedFrame,
        this, [](const QByteArray &frame) {
            qDebug() << "Mainwindow::startServices(): Unsupported frame "
                        ">>>>>>>>>: POSSIBLE COLLISION:" << frame.toHex();
        });

    // UI Update for TX Bandwidth
    connect(
        m_civ,
        &CivProtocol::updateTXBw,
        this,
        &MainWindow::updateTxBwDisplay);

    // UI Update for RF Power
    connect(
        m_civ,
        &CivProtocol::updateRfPwr,
        this,
        &MainWindow::updateRFPowerDisplay);

    // UI Update for Main Sub LEDs
    connect(
        m_civ,
        &CivProtocol::updateMainSubLeds,
        this,
        &MainWindow::updateUIMainSubLEDs);

    // UI Update for Main Freq Display
    connect(
        m_civ,
        &CivProtocol::updateMainFreqDisplay,
        this,
        &MainWindow::updateSATFrequencySub);

    // UI Update for SUB Freq Display
    connect(
        m_civ,
        &CivProtocol::updateSubFreqDisplay,
        this,
        &MainWindow::updateSATFrequencyMain);

    /*
     * Setup a timer to poll the radio
     */
    m_radioPollTimer = new QTimer(this);
    Q_ASSERT(m_radioPollTimer);
    m_radioPollTimer->setInterval(1000);

    connect(
        m_radioPollTimer,
        &QTimer::timeout,
        this,
        &MainWindow::pollRadioStatus);

    // m_radioPollTimer->start();

    // S.A.T Controller
    rc = openCivSerial();
    if ( !rc ) {
        qDebug() << "MainWindow::startServices(): CiV Serial Port open failed";
        throw std::runtime_error("MainWindow::startServices(): Fatal: S.A.T Serial Port open failed");
    }

    m_radioBackend->submit(
        GetFrequency {
            Vfo::A
        });

    m_radioBackend->submit(
        GetFrequency {
            Vfo::B
        });

    m_radioBackend->submit(
        SetVfoModeViaMA {});

    m_radioBackend->submit(
        SetSplit{ true });

    // Now that all our signals are connected, initialize Civ
    m_civ->init();  // Establish initial state
}

#if 0
void MainWindow::serial_port_detected(QString &s) {
    // qDebug() << "MainWindow::serial_port_detected()" << s;
}
#endif

void MainWindow::serialPortComboBox_activated(int index)
{
    qDebug() << "MainWindow::on_serialPortComboBox_activated: " << index;
}


void MainWindow::updateSATFrequencyMain(int freq)
{
    ui->lcdFrequencyMain->setText(QString::number(freq));

}

void MainWindow::updateSATFrequencySub(int freq)
{
    ui->lcdFrequencySub->setText(QString::number(freq));

}

void MainWindow::updatePowerDisplay(QString str)
{
    ui->labelPower->setText("TODO" + str);
}

bool MainWindow::openCivSerial()
{
    SerialPort::Settings settings;

    settings.portName = "/dev/cu.PL2303G-USBtoUART140";
    settings.baudRate = QSerialPort::Baud9600;
    settings.dataBits = QSerialPort::Data8;
    settings.parity = QSerialPort::NoParity;
    settings.stopBits = QSerialPort::OneStop;
    settings.flowControl = QSerialPort::NoFlowControl;
    settings.dtr = false;
    settings.rts = false;

    qDebug()
        << "MainWindow::openCivSerial(): Opening CI-V serial port"
        << settings.portName << "at" << settings.baudRate;

    if (!m_civSerial->open(settings)) {
        qWarning()
        << "MainWindow::updatePowerDisplay(): Unable to open CI-V port:"
        << m_civSerial->errorString();

        return false;
    }

    qDebug() << "MainWindow::openCivSerial(): CI-V serial port opened";

    return true;
}

bool MainWindow::openFT991Serial()
{
    SerialPort::Settings settings;

    settings.portName = "/dev/cu.usbserial-00C45B130";
    settings.baudRate = QSerialPort::Baud38400;
    settings.dataBits = QSerialPort::Data8;
    settings.parity = QSerialPort::NoParity;
    settings.stopBits = QSerialPort::OneStop;
    settings.flowControl = QSerialPort::NoFlowControl;
    settings.dtr = false;
    settings.rts = false;

    qDebug()
        << "Opening FT991A serial port"
        << settings.portName
        << "at"
        << settings.baudRate;

    if (!m_ft991Serial->open(settings))
    {
        qWarning()
        << "Unable to open FT991A Serial port:"
        << m_ft991Serial->errorString();

        return false;
    }

    qDebug() << "MainWindow::openFT991Serial(): FT991A serial port opened";

    return true;
}

void MainWindow::radioRequestCompleted(
    quint64 requestId,
    RadioResponse response)
{

    qDebug() << "MainWindow::radioRequestCompleted():"
             << "ID:" << requestId << radioResponseTypeName(response);

    // FrequencyResponse
    if (auto value = std::get_if<FrequencyResponse>(&response)) {
        double mhz = value->hz / 1000000.0;
        QString str = QString::number(mhz, 'f', 6);

        if (value->vfo == Vfo::A) {
            ui->labelRadioFreqA->setText(str);
            ui->labelRadioFreqA->setStyleSheet("color: green;");
        }
        else {
            ui->labelRadioFreqB->setText(str);
            ui->labelRadioFreqB->setStyleSheet("color: green;");
        }

        return;
    }

    // ModeResponse
    if (auto value = std::get_if<ModeResponse>(&response)) {
        QString text;

        switch (value->mode)
        {
        case RadioMode::Lsb:
            text = "LSB";
            break;

        case RadioMode::Usb:
            text = "USB";
            break;

        case RadioMode::CwU:
            text = "CW";
            break;

        case RadioMode::CwL:
            text = "CW-R";
            break;

        case RadioMode::Fm:
            text = "FM";
            break;

        case RadioMode::FmNarrow:
            text = "FM-N";
            break;

        case RadioMode::Am:
            text = "AM";
            break;

        case RadioMode::AmNarrow:
            text = "AM-N";
            break;

        case RadioMode::DataLsb:
            text = "DATA-L";
            break;

        case RadioMode::DataUsb:
            text = "DATA-U";
            break;

        case RadioMode::DataFm:
            text = "DATA-FM";
            break;

        case RadioMode::RttyLsb:
            text = "RTTY-L";
            break;

        case RadioMode::RttyUsb:
            text = "RTTY-U";
            break;

        case RadioMode::C4fm:
            text = "C4FM";
            break;
        }

        if (value->vfo == Vfo::A)
            ui->labelRadioModeA->setText(text);
        else
            ui->labelRadioModeB->setText(text);

        return;
    }

    /*
    AckResponse,
    ErrorResponse
    */

    // RfPowerResponse
    if (auto value =
        std::get_if<RfPowerResponse>(&response))
    {
        ui->labelRfPower->setText(
            QString("%1W").arg(value->watts));

        return;
    }

    // TxResponse
    if (auto value =
        std::get_if<TxResponse>(&response))
    {
        switch (value->state)
        {
        case TxState::Receive:
            ui->labelTxState->setText("RX");
            break;

        case TxState::CatTransmit:
            ui->labelTxState->setText("TX (CAT)");
            break;

        case TxState::RadioTransmit:
            ui->labelTxState->setText("TX");
            break;
        }

        return;
    }
}

void MainWindow::pollRadioStatus()
{
    // 1. Check if the previous request was ignored
    m_missedResponsesCount++;

    if (m_missedResponsesCount >= MAX_MISSED_RESPONSES) {
        // 2. Pause the timer
        m_radioPollTimer->stop();
        qDebug() << "MainWindow::pollRadioStatus(): Radio stopped responding. Polling paused.";
        return;
    }

    if (!m_radioBackend)
        return;

    qDebug().noquote() << "\n\033[32mMainWindow::pollRadioStatus(): Polling started\033[0m";
    m_radioBackend->submit(GetFrequency {Vfo::A});
    m_radioBackend->submit(GetFrequency {Vfo::B});
    m_radioBackend->submit(GetMode {Vfo::A});
    m_radioBackend->submit(GetRfPower {});
    m_radioBackend->submit(GetTx {});
    qDebug() << "MainWindow::pollRadioStatus(): Polling complete\n\n";
}

void MainWindow::resumePolling()
{
    m_missedResponsesCount = 0;
    m_radioPollTimer->start(); // Resumes the 1000ms timer
    qDebug() << "Polling resumed.";
}

QString MainWindow::radioResponseTypeName(
    const RadioResponse &response)
{
    return std::visit(
        [](const auto &value) -> QString
        {
            using T =
                std::decay_t<decltype(value)>;

            if constexpr (
                std::is_same_v<T, FrequencyResponse>)
                return "FrequencyResponse";

            else if constexpr (
                std::is_same_v<T, ModeResponse>)
                return "ModeResponse";

            else if constexpr (
                std::is_same_v<T, RfPowerResponse>)
                return "RfPowerResponse";

            else if constexpr (
                std::is_same_v<T, TxResponse>)
                return "TxResponse";

            else if constexpr (
                std::is_same_v<T, AckResponse>)
                return "AckResponse";

            else if constexpr (
                std::is_same_v<T, ErrorResponse>)
                return "ErrorResponse";

            else
                return "Unknown";
        },
        response);
}

QString MainWindow::radioRequestTypeName(
    const RadioRequest &request)
{
    return std::visit(
        [](const auto &value) -> QString
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (
                std::is_same_v<T, GetFrequency>)
                return "GetFrequency";

            else if constexpr (
                std::is_same_v<T, SetFrequency>)
                return "SetFrequency";

            else if constexpr (
                std::is_same_v<T, GetMode>)
                return "GetMode";

            else if constexpr (
                std::is_same_v<T, SetMode>)
                return "SetMode";

            else if constexpr (
                std::is_same_v<T, GetRfPower>)
                return "GetRfPower";

            else if constexpr (
                std::is_same_v<T, SetRfPower>)
                return "SetRfPower";

            else if constexpr (
                std::is_same_v<T, GetTx>)
                return "GetTx";

            else if constexpr (
                std::is_same_v<T, SetTx>)
                return "SetTx";

            else if constexpr (
                std::is_same_v<T, SetSplit>)
                return "SetSplit";

            else if constexpr (
                std::is_same_v<T, SetVfoModeViaMA>)
                return "Set FT991A to VFO Mode";

            else if constexpr (
                std::is_same_v<T, SetMemChannel>)
                return "Set FT991A to Mem Channel";

            else if constexpr (
                std::is_same_v<T, SwapAB>)
                return "Set FT991A A/B (Swap VFOs)";

            else
                return "Unknown";
        },
        request);
}

void MainWindow::updateTxBwDisplay(int bw)
{
    QString bwStr;

    switch (bw) {
    case 0x00:
        bwStr = "WIDE";
        break;
    case 0x01:
        bwStr = "MID";
        break;
    case 0x02:
        bwStr = "NAR";
    default:
        bwStr = "INVALID";
        break;
    }

    ui->labelSSB_TxBW->setText(bwStr);
}

void MainWindow::updateRFPowerDisplay(int pwr) {
    ui->labelPower->setText(QString::number(pwr));
}

void MainWindow::updateUIMainSubLEDs(CivBand b) {

    if ( b == CivBand::Sub ) {
        ui->ledMainActive->setLedColor(Qt::green);
        ui->ledSubActive->setLedColor(Qt::red);
    }
    else {
        ui->ledMainActive->setLedColor(Qt::red);
        ui->ledSubActive->setLedColor(Qt::green);

    }
}