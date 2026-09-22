#include "SerialPort.h"

#include <QDebug>


SerialPort::SerialPort(
    QObject *parent)
    : QObject(parent),
    m_serialport(this)
{
    connect(
        &m_serialport,
        &QSerialPort::readyRead,
        this,
        &SerialPort::onReadyRead);


    connect(
        &m_serialport,
        &QSerialPort::errorOccurred,
        this,
        &SerialPort::onError);

    connect(
        &m_serialport,
        &QIODevice::aboutToClose,
        this,
        [this]()
        {
            qWarning()
            << "*************** QSerialPort ABOUT TO CLOSE ******************"
            << "port =" << m_serialport.portName()
            << "this =" << this
            << "QSerialPort =" << &m_serialport
            << "isOpen =" << m_serialport.isOpen()
            << "openMode =" << m_serialport.openMode()
            << "error =" << m_serialport.error()
            << "errorString =" << m_serialport.errorString();
        });
}


SerialPort::~SerialPort()
{
    qDebug() << "SerialPort::~SerialPort(): serial port has been DESTROYED by destructor call ************";
    close();
}

/*
 * Opening the port
 *
 * Qt applies the serial configuration when the port is opened
 * the port is also opened exclusively, so another process can't
 * simultaneously grab the same serial device.
 *
 */
bool SerialPort::open(
    const Settings &settings)
{
    if (m_serialport.isOpen()) {
        Q_ASSERT("CALLING OPEN WHEN THE PORT IS ALREADY OPENED **********+++++++++++++**********");
        m_serialport.close();
    }


    m_settings = settings;


    m_serialport.setPortName(
        settings.portName);

    m_serialport.setBaudRate(
        settings.baudRate);

    m_serialport.setDataBits(
        settings.dataBits);

    m_serialport.setParity(
        settings.parity);

    m_serialport.setStopBits(
        settings.stopBits);

    m_serialport.setFlowControl(
        settings.flowControl);


    if (!m_serialport.open(
            QIODevice::ReadWrite))
    {
        emit errorOccurred(
            QStringLiteral(
                "Unable to open %1: %2")
                .arg(
                    settings.portName,
                    m_serialport.errorString()));

        return false;
    }
    qDebug() << "SerialPort::open(): Serial Port was just opened" << m_serialport.isOpen();

    //
    // Set modem-control lines only after
    // successfully opening the device.
    //
    m_serialport.setDataTerminalReady(
        settings.dtr);

    m_serialport.setRequestToSend(
        settings.rts);


    //
    // Throw away anything stale that was sitting
    // in the driver buffers before we opened.
    //
    m_serialport.clear(
        QSerialPort::AllDirections);


    qDebug() << "SerialPort::open(): about to call emit opened()" << m_serialport.isOpen();
    emit opened();

    return true;
}

/*
 * Close the port
 */
void SerialPort::close()
{
    qDebug() << "SerialPort::close(): Serial Port has been CLOSED *********";
    if (!m_serialport.isOpen())
        return;

     m_serialport.close();

    emit closed();
}


bool SerialPort::isOpen() const
{
    return m_serialport.isOpen();
}

QIODevice *SerialPort::device()
{
    return &m_serialport;
}


const QIODevice *SerialPort::device() const
{
    return &m_serialport;
}


QSerialPort *SerialPort::serialPort()
{
    return &m_serialport;
}


const QSerialPort *
SerialPort::serialPort() const
{
    return &m_serialport;
}

/*
 * Receiving data - keep this completely ignorant of the protocol
 *
 * Notice there is no receive buffer here.  That's intentional.
 *
 * For the FT-991A:

 * SerialPort/QSerialPort
 *         ↓ arbitrary bytes
 * Ft991Client
 *         ↓ finds ';'
 * CAT frames
 *
 * For CI-V:
 *
 * SerialPort/QSerialPort
 *         ↓ arbitrary bytes
 * CivProtocol
 *         ↓ finds FE FE ... FD
 * CI-V frames
 *
 * Much cleaner than having the serial class know what constitutes a message.
 */
void SerialPort::onReadyRead()
{
    QByteArray data =
        m_serialport.readAll();

    if (data.isEmpty())
        return;


    emit bytesReceived(data);

    emit bytesReceived(data);
}

/*
 * Sending
 *
 * Do not call flush() after every write. QSerialPort starts
 * ransmitting buffered output when control returns to the Qt
 * event loop; Qt specifically notes that normally you don't
 * need to call flush()
 *
 */
qint64 SerialPort::write(
    const QByteArray &data)
{
    if (!m_serialport.isOpen())
    {
        emit errorOccurred(
            QStringLiteral(
                "SerialPort::write(): Attempt to write to closed serial port"));

        return -1;
    }

    qDebug() << "SerialPort::write(): Entered with" << data;

    qint64 result =
        m_serialport.write(data);


    if (result < 0)
    {
        emit errorOccurred(
            QStringLiteral(
                "Serial write failed: %1")
                .arg(
                    m_serialport.errorString()));

        return result;
    }


    if (result > 0)
    {
        emit bytesTransmitted(
            data.left(result));
    }


    return result;
}

/*
 * Error handling
 *
 * That NoError check isn't arbitrary: Qt documents that
 * errorOccurred(NoError) is historically emitted on a successful open
 *
 */
void SerialPort::onError(
    QSerialPort::SerialPortError error)
{
    //
    // QSerialPort historically emits NoError
    // during successful open(), so ignore it.
    //
    if (error ==
        QSerialPort::NoError)
    {
        return;
    }


    QString message =
        QStringLiteral(
            "Serial port %1: %2")
            .arg(
                m_serialport.portName(),
                m_serialport.errorString());


    emit errorOccurred(
        message);
}


/*
 * The remaining accessors
 */
QString SerialPort::portName() const
{
    return m_serialport.portName();
}


QString SerialPort::errorString() const
{
    return m_serialport.errorString();
}


const SerialPort::Settings &
SerialPort::settings() const
{
    return m_settings;
}


QList<QSerialPortInfo>
SerialPort::availablePorts()
{
    return
        QSerialPortInfo::
        availablePorts();
}