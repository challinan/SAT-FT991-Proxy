#include "SerialPort.h"

#include <QDebug>


SerialPort::SerialPort(
    QObject *parent)
    : QObject(parent),
    m_port(this)
{
    connect(
        &m_port,
        &QSerialPort::readyRead,
        this,
        &SerialPort::onReadyRead);


    connect(
        &m_port,
        &QSerialPort::errorOccurred,
        this,
        &SerialPort::onError);
}


SerialPort::~SerialPort()
{
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
    if (m_port.isOpen())
        m_port.close();


    m_settings = settings;


    m_port.setPortName(
        settings.portName);

    m_port.setBaudRate(
        settings.baudRate);

    m_port.setDataBits(
        settings.dataBits);

    m_port.setParity(
        settings.parity);

    m_port.setStopBits(
        settings.stopBits);

    m_port.setFlowControl(
        settings.flowControl);


    if (!m_port.open(
            QIODevice::ReadWrite))
    {
        emit errorOccurred(
            QStringLiteral(
                "Unable to open %1: %2")
                .arg(
                    settings.portName,
                    m_port.errorString()));

        return false;
    }


    //
    // Set modem-control lines only after
    // successfully opening the device.
    //
    m_port.setDataTerminalReady(
        settings.dtr);

    m_port.setRequestToSend(
        settings.rts);


    //
    // Throw away anything stale that was sitting
    // in the driver buffers before we opened.
    //
    m_port.clear(
        QSerialPort::AllDirections);


    emit opened();

    return true;
}

/*
 * Close the port
 */
void SerialPort::close()
{
    if (!m_port.isOpen())
        return;


    m_port.close();

    emit closed();
}


bool SerialPort::isOpen() const
{
    return m_port.isOpen();
}

QIODevice *SerialPort::device()
{
    return &m_port;
}


const QIODevice *SerialPort::device() const
{
    return &m_port;
}


QSerialPort *SerialPort::serialPort()
{
    return &m_port;
}


const QSerialPort *
SerialPort::serialPort() const
{
    return &m_port;
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
        m_port.readAll();

    if (data.isEmpty())
        return;


    emit bytesReceived(data);

    emit dataReceived(data);
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
    if (!m_port.isOpen())
    {
        emit errorOccurred(
            QStringLiteral(
                "Attempt to write to closed serial port"));

        return -1;
    }


    qint64 result =
        m_port.write(data);


    if (result < 0)
    {
        emit errorOccurred(
            QStringLiteral(
                "Serial write failed: %1")
                .arg(
                    m_port.errorString()));

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
                m_port.portName(),
                m_port.errorString());


    emit errorOccurred(
        message);
}


/*
 * The remaining accessors
 */
QString SerialPort::portName() const
{
    return m_port.portName();
}


QString SerialPort::errorString() const
{
    return m_port.errorString();
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