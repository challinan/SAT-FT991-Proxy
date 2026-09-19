#include "mainwindow.h"
#include "SerialPort.h"
#include "ui_mainwindow.h"
#include <QFontDatabase>

#define ENABLE_FT991_SIM    // Radio not available, use the simulator

#ifdef ENABLE_FT991_SIM
#include "RadioCoreBackend.h"
#else
#include "Ft991Client.h"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
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

    // Initialize serial port object


    // Start it running
    startServices();
}

MainWindow::~MainWindow()
{
    delete config_p;
    // delete network_comms_p;
    delete ui;
}


void MainWindow::on_quit_pButton_clicked()
{
    QApplication::quit();
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


    updateSATFrequencyMain(145.0);
    updateSATFrequencySub(435.0);
    ui->labelPower->setText("50%");
}

void MainWindow::on_run_pButton_clicked() {
    qDebug() << "MainWindow::on_run_pButton_clicked() Entered!";

}

void MainWindow::startServices()
{
    bool rc;

    // Hook everything together
    m_civSerial =
        new SerialPort(this);

    m_civ =
        new CivProtocol(this);

    m_proxy =
        new CivProxyController(this);

    // S.A.T. serial RX
    connect(
        m_civSerial,
        &SerialPort::dataReceived,
        m_civ,
        &CivProtocol::feedBytes);


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


    // CI-V response back to S.A.T.
    connect(
        m_civ,
        &CivProtocol::frameReady,
        m_civSerial,
        &SerialPort::write);

    // radioRequest handling
    connect(
        m_civ,
        &CivProtocol::radioRequest,
        m_proxy,
        &CivProxyController::submit);

    connect(
        m_civ,
        &CivProtocol::updatePowerLabel,
        this,
        &MainWindow::updatePowerDisplay);

    // During travel:
#ifdef ENABLE_FT991_SIM
    m_radioBackend =
        new RadioCoreBackend(this);

    m_radioBackend->setTimeout(500);
#else
    // At home:
    m_ft991Serial =
        new SerialPort(this);

    rc = openFT991Serial();
    if ( !rc ) {
        qDebug() << "MainWindow::startServices(): Serial Port open failed";
        throw std::runtime_error("MainWindow::startServices(): Fatal: FT991A Serial Port open failed");
    }

    m_radioBackend =
        new Ft991Client(
            m_ft991Serial->device(),
            this);

    m_ft991Client->disableAutoInformation();
    m_radioBackend->setTimeout(500);

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
#endif

        m_proxy->setBackend(
            m_radioBackend);

    // Status monitoring for GUI
    connect(
        m_radioBackend,
        &RadioBackend::requestCompleted,
        this,
        &MainWindow::radioRequestCompleted);

    m_radioPollTimer = new QTimer(this);
    Q_ASSERT(m_radioPollTimer);
    m_radioPollTimer->setInterval(1000);

    connect(
        m_radioPollTimer,
        &QTimer::timeout,
        this,
        &MainWindow::pollRadioStatus);

    m_radioPollTimer->start();


    // S.A.T Controller
    rc = openCivSerial();
    if ( !rc ) {
        qDebug() << "MainWindow::startServices(): CiV Serial Port open failed";
        throw std::runtime_error("MainWindow::startServices(): Fatal: S.A.T Serial Port open failed");
    }

    qint64 id = m_radioBackend->submit(
        GetFrequency {
            Vfo::A
        });

    qDebug() << "Submitted request" << id;
}

#if 0
void MainWindow::serial_port_detected(QString &s) {
    // qDebug() << "MainWindow::serial_port_detected()" << s;
}
#endif

void MainWindow::on_serialPortComboBox_activated(int index)
{
    qDebug() << "MainWindow::on_serialPortComboBox_activated: " << index;
}


void MainWindow::updateSATFrequencyMain(int freq)
{
    ui->lcdFrequencyMain->setText("145.000");

}

void MainWindow::updateSATFrequencySub(int freq)
{
    ui->lcdFrequencySub->setText("435.000");

}

void MainWindow::updatePowerDisplay(QString S)
{
    ui->labelPower->setText("PLACEH");
}

bool MainWindow::openCivSerial()
{
    SerialPort::Settings settings;

    settings.portName =
        "/dev/cu.PL2303G-USBtoUART1410";

    settings.baudRate =
        QSerialPort::Baud9600;

    settings.dataBits =
        QSerialPort::Data8;

    settings.parity =
        QSerialPort::NoParity;

    settings.stopBits =
        QSerialPort::OneStop;

    settings.flowControl =
        QSerialPort::NoFlowControl;

    settings.dtr = false;
    settings.rts = false;


    qDebug()
        << "Opening CI-V serial port"
        << settings.portName
        << "at"
        << settings.baudRate;


    if (!m_civSerial->open(settings))
    {
        qWarning()
        << "Unable to open CI-V port:"
        << m_civSerial->errorString();

        return false;
    }


    qDebug()
        << "CI-V serial port opened";

    return true;
}

bool MainWindow::openFT991Serial()
{
    SerialPort::Settings settings;

    settings.portName =
        "/dev/cu.xyz";

    settings.baudRate =
        QSerialPort::Baud38400;

    settings.dataBits =
        QSerialPort::Data8;

    settings.parity =
        QSerialPort::NoParity;

    settings.stopBits =
        QSerialPort::OneStop;

    settings.flowControl =
        QSerialPort::NoFlowControl;

    settings.dtr = false;
    settings.rts = false;


    qDebug()
        << "Opening FR991A serial port"
        << settings.portName
        << "at"
        << settings.baudRate;


    if (!m_civSerial->open(settings))
    {
        qWarning()
        << "Unable to open FT991A Serial port:"
        << m_civSerial->errorString();

        return false;
    }


    qDebug()
        << "FT991A serial port opened";

    return true;
}

void MainWindow::radioRequestCompleted(
    quint64 requestId,
    RadioResponse response)
{
    Q_UNUSED(requestId);

    if (auto value =
        std::get_if<FrequencyResponse>(
            &response))
    {
        double mhz =
            value->hz / 1000000.0;

        if (value->vfo == Vfo::A)
        {
            ui->labelRadioFreqA->setText(
                QString::number(
                    mhz,
                    'f',
                    6));
        }
        else
        {
            ui->labelRadioFreqB->setText(
                QString::number(
                    mhz,
                    'f',
                    6));
        }

        return;
    }


    if (auto value =
        std::get_if<ModeResponse>(
            &response))
    {
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

#if 0   // Implement other radio modes, etc
    if (auto value =
        std::get_if<RfPowerResponse>(
            &response))
    {
        ui->labelRfPower->setText(
            QString("%1 %").arg(
                value->percent));

        return;
    }


    if (auto value =
        std::get_if<TxResponse>(
            &response))
    {
        switch (value->state)
        {
        case TxState::Receive:
            ui->labelTxState->setText(
                "RX");
            break;

        case TxState::CatTransmit:
            ui->labelTxState->setText(
                "TX (CAT)");
            break;

        case TxState::RadioTransmit:
            ui->labelTxState->setText(
                "TX");
            break;
        }

        return;
    }
#endif
}

void MainWindow::pollRadioStatus()
{
    if (!m_radioBackend)
        return;

    m_radioBackend->submit(
        GetFrequency {
            Vfo::A
        });

    m_radioBackend->submit(
        GetFrequency {
            Vfo::B
        });

    m_radioBackend->submit(
        GetMode {
            Vfo::A
        });

    m_radioBackend->submit(
        GetRfPower {});

    m_radioBackend->submit(
        GetTx {});
}