#include "terminal_debug.h"
#include <QDebug>
#include <QDateTime>

class CivProtocol;

QHash<QByteArray, QString> TerminalDebug::commandTable;

TerminalDebug::TerminalDebug(const QString &devicePath, QObject *parent)
    : QFile{parent}
{
    m_device = new QFile(devicePath);
    if (m_device->open(QIODevice::ReadWrite | QIODevice::Unbuffered)) {
        qDebug() << "TerminalDebug::TerminalDebug(): Successfully opened device:" << devicePath;
    }

    initializeHashTable();

}

TerminalDebug::~TerminalDebug() {
    close();
    delete m_device;
}

void TerminalDebug::incomingDebugData(const QByteArray &data, QString serialPort, Dir dir)
{
    // Create a precise timestamp
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    QString logLine;
    QString serialPortWithDir = serialPort;

    serialPortWithDir.append((dir == Dir::in) ? "-IN" : "-OUT");

    QString tmpStr = serialPortWithDir.leftJustified(10, ' ');
    serialPortWithDir = tmpStr;

    // Format the log line (e.g., "[16:44:00.123] [Port A] 05 A1 FF")

    if ( serialPort.contains("FT991A") ) {
        m_device->write(prepareLogLine(serialPortWithDir, timestamp, data).toUtf8());
        return;
    }

    if ( serialPort.contains("SAT") ) {
        if ( dir == Dir::in && data.contains("\xFE\xFE\xE0\xA2") ) {
            // This looks like an echo frame
            return;
        }

        if ( dir == Dir::out && data.contains("\xFE\xFE\xA2\xE0") ) {
            // This looks like an echo frame
            return;
        }

        m_device->write(prepareLogLine(serialPortWithDir, timestamp, data).toUtf8());
        return;
    }

    m_device->write("\x1B[31mTerminalDebug::incomingDebugData(): packet has incorrect serialPort string\x1B[31m\n");
     return;
}

QString TerminalDebug::prepareLogLine(QString &serPortDir, QString &timestamp, const QByteArray &data)
{
    QString displayLine, logLine;
    QString bandStr = (m_Band == CivBand::Main) ? "MAIN" : "SUB ";

    if ( serPortDir.contains("FT991A") ) {
        displayLine = data;
    }
    else {
        // S.A.T frame
        displayLine = data.toHex();

        AckStatus ack = getAckStatus(data);
        CommandMatch cM;

        // If this is a ACK or NACK packet , note it
        if ( ack != AckStatus::none ) {
            QString strAck = ack == (AckStatus::ack) ? "[ACK]" : "[NG]";
            displayLine.append(" \x1B[31m" + strAck + "\x1B[0m");
        }
        else {
            if ( data.length() > 4 ) {
                cM = lookupCommand(data.mid(4));
                displayLine.append(" [" + cM.label + "]");
            }
        }

    }

    logLine = QString("[%1] [%2] [%3] %4\n")
                  .arg(timestamp, serPortDir, bandStr, displayLine); // Displays bytes cleanly as hex space-separated

    return logLine;

}

CommandMatch TerminalDebug::lookupCommand(const QByteArray &cmdBytes) const
{
    CommandMatch result;

    // qDebug() << "TerminalDebug::lookupCommand(): Entered" << cmdBytes.toHex();
    for (auto it = commandTable.constBegin(); it != commandTable.constEnd(); ++it) {

        const QByteArray &key = it.key();

        if (cmdBytes.startsWith(key) && key.size() > result.length) {
            result.label = it.value();
            result.length = key.size();
        }
    }

    // Handle the outliers without getting too complicated:
    if ( result.label.contains("Function Control") && cmdBytes.length() > 4)
        result.label = "MALFORMED 0x16 FRAME";

    return result;
}

void TerminalDebug::initializeHashTable() const {
    commandTable = {
        { QByteArray::fromHex("00"),   "Set Frequency [00]" },
        { QByteArray::fromHex("01fd"),   "Request Mode Data / Transceive" },
        { QByteArray::fromHex("0100"),   "Set Mode LSB" },
        { QByteArray::fromHex("0101"),   "Set Mode USB" },
        { QByteArray::fromHex("0102"),   "Set Mode AM" },
        { QByteArray::fromHex("0103"),   "Set Mode CW" },
        { QByteArray::fromHex("0104"),   "Set Mode RTTY" },
        { QByteArray::fromHex("0105"),   "Set Mode FM" },
        { QByteArray::fromHex("0107"),   "Set Mode CW-R" },
        { QByteArray::fromHex("0108"),   "Set Mode RTTY-R" },
        { QByteArray::fromHex("0117"),   "Set Mode DV" },
        { QByteArray::fromHex("0122"),   "Set Mode DD" },
        { QByteArray::fromHex("03"),   "Read Frequency [03]" },
        { QByteArray::fromHex("04"),   "Read Mode" },
        { QByteArray::fromHex("05"),   "Set Frequency [05]" },
        { QByteArray::fromHex("06"),   "Set Mode" },
        { QByteArray::fromHex("07B0"),   "Exchange Main/Sub Bands" },
        { QByteArray::fromHex("07FD"),   "Select VFO Mode" },
        { QByteArray::fromHex("07D0FD"),   "Select MAIN Band" },
        { QByteArray::fromHex("07D1FD"),   "Select SUB Band" },
        { QByteArray::fromHex("1000"),   "SetTuning Step OFF" },
        { QByteArray::fromHex("1001"),   "SetTuning Step 100Hz" },
        { QByteArray::fromHex("14"),   "Level Control" },
        { QByteArray::fromHex("140AFD"), "Read RF Power" },
        { QByteArray::fromHex("140A"), "Set RF Power" },
        { QByteArray::fromHex("16"),   "Function Control Unknown" },
        { QByteArray::fromHex("1642fd"),   "Read Repeater Tone" },
        { QByteArray::fromHex("164200"),   "Set Repeater Tone Off" },
        { QByteArray::fromHex("1658fd"), "Read SSB Bandwidth" },
        { QByteArray::fromHex("165800"), "Report/Set SSB Bandwidth WIDE" },
        { QByteArray::fromHex("165801"), "Report/Set SSB Bandwidth MID" },
        { QByteArray::fromHex("165802"), "Report/Set SSB Bandwidth NAR" },
        { QByteArray::fromHex("1659"),   "Read SUB Band (DUALWATCH) Status" },
        { QByteArray::fromHex("165900"), "Set SUB Band (DUALWATCH) OFF" },
        { QByteArray::fromHex("165901"), "Set SUB Band (DUALWATCH) ON" },
        { QByteArray::fromHex("165A"), "Satellite Function" },
        { QByteArray::fromHex("1A06fd"),   "Request Data Mode w/ Filter Settings" },
        { QByteArray::fromHex("1A06"),   "Set Data Mode w/ Filter Settings" },
        { QByteArray::fromHex("1A05"), "Extended Settings" },
        { QByteArray::fromHex("1A05013100"),   "DATA ECHO BACK OFF" },
        { QByteArray::fromHex("1A05013101"),   "DATA ECHO BACK ON" },
        { QByteArray::fromHex("165AFD"),   "READ Satellite Mode" },
        { QByteArray::fromHex("165A00"),   "SET Satellite Mode OFF" },
        { QByteArray::fromHex("0f00"),   "SET SPLIT OFF" },
        { QByteArray::fromHex("0f01"),   "SET SPLIT ON" },
        { QByteArray::fromHex("1A05"), "Extended Settings" },
        { QByteArray::fromHex("2100"), "Set RIT Settings" },
        { QByteArray::fromHex("2101"), "Read RIT Status On/Off" },
        { QByteArray::fromHex("2712"), "SCOPE MAIN/SUB Settings" }
    };
}

AckStatus TerminalDebug::getAckStatus(const QByteArray &data)
{
    if ( data.startsWith("\xFE\xFE")
        && data.endsWith("\xFB\xFD")) {
        return AckStatus::ack;
    }
    if ( data.startsWith("\xFE\xFE")
        && data.endsWith("\xFA\xFD")) {
        return AckStatus::nack;
    }

    return AckStatus::none;
}

void TerminalDebug::updateBand(CivBand b)
{
    m_Band = b;
}