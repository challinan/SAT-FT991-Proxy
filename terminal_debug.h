#ifndef TERMINAL_DEBUG_H
#define TERMINAL_DEBUG_H

#include <QObject>
#include <QByteArray>
#include <QFile>
#include <QQueue>
#include <QTimer>
#include <QHash>

enum class Dir
{
    in,
    out
};

enum class AckStatus
{
    ack,
    nack,
    none
};

enum class CivBand {
    Main,
    Sub
};

struct CommandMatch
{
    QString label;
    int length = 0;
};

class TerminalDebug : public QFile
{
    Q_OBJECT

public:
    explicit TerminalDebug(const QString &devicePath, QObject *parent = nullptr);
    ~TerminalDebug();


private:
    QString prepareLogLine(QString &serPortDir, QString &timestamp, const QByteArray &data = QByteArray());
    CommandMatch lookupCommand(const QByteArray &cmdBytes) const;
    void initializeHashTable() const;
    AckStatus getAckStatus(const QByteArray &data);


public slots:
    void incomingDebugData(const QByteArray &data, QString serialPort, Dir direction);
    void updateBand(CivBand b);

signals:

private:
    QFile *m_device;;
    QTimer *m_SATTimeout;
    CivBand m_Band;

    static QHash<QByteArray, QString> commandTable;

};

#endif // TERMINAL_DEBUG_H
