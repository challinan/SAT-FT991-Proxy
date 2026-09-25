#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>


class SerialPort : public QObject
{
    Q_OBJECT

public:

    struct Settings
    {
        QString portName;

        qint32 baudRate =
            QSerialPort::Baud9600;

        QSerialPort::DataBits dataBits =
            QSerialPort::Data8;

        QSerialPort::Parity parity =
            QSerialPort::NoParity;

        QSerialPort::StopBits stopBits =
            QSerialPort::OneStop;

        QSerialPort::FlowControl flowControl =
            QSerialPort::NoFlowControl;

        bool dtr = false;
        bool rts = false;
    };


    explicit SerialPort(QObject *parent = nullptr);

    ~SerialPort() override;


    bool open(const Settings &settings);

    void close();

    bool isOpen() const;

    //
    // This is what Ft991Client wants.
    //
    QIODevice *device();

    const QIODevice *device() const;


    QSerialPort *serialPort();

    const QSerialPort *serialPort() const;


    QString portName() const;

    QString errorString() const;

    const Settings &settings() const;
    void setPortHumanName(QString s);


    static QList<QSerialPortInfo>
    availablePorts();


signals:

    void opened();

    void closed();

    void byteReceived(QByteArray data);

    void errorOccurred(QString message);

    //
    // Optional diagnostics.
    //
    void bytesReceived(QByteArray data);

    void bytesTransmitted(QByteArray data);


public slots:

    qint64 write(const QByteArray &data);


private slots:

    void onReadyRead();

    void onError(QSerialPort::SerialPortError error);


private:

    QSerialPort m_serialport;
    QString portHumanName = "";
    Settings m_settings;
};