#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include "config_object.h"

#include "CivProtocol.h"
#include "CivProxyController.h"
#include "RadioTypes.h"

class SerialPort;
class CivProtocol;
class CivProxyController;
class Ft991Client;
class RadioBackend;


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void startServices();
    void resumePolling();
    QString radioResponseTypeName(const RadioResponse &response);

private slots:
    void on_quit_pButton_clicked();
    void on_config_pButton_clicked();
    void on_run_pButton_clicked();
    void serialPortComboBox_activated(int index);
    void updateSATFrequencyMain(int freq);
    void updateSATFrequencySub(int freq);
    void updatePowerDisplay(QString S);
    void radioRequestCompleted(quint64 requestId, RadioResponse response);
    void updatePowerLabel(QString S);
    void pollRadioStatus();

private:
    QTimer *m_radioPollTimer = nullptr;
    int m_missedResponsesCount = 0;
    const int MAX_MISSED_RESPONSES = 3;

private:
    Ui::MainWindow *ui;
    ConfigObject *config_p;
    QString s_tmp;
    void initializeUiLabels();
    bool openCivSerial();
    bool openFT991Serial();

    //
    // Serial hardware
    //
    SerialPort *m_civSerial = nullptr;
    SerialPort *m_ft991Serial = nullptr;


    //
    // Protocol machinery
    //
    CivProtocol *m_civ = nullptr;
    CivProxyController *m_proxy = nullptr;
    Ft991Client *m_ft991Client = nullptr;
    RadioBackend *m_radioBackend = nullptr;

public slots:
};
#endif // MAINWINDOW_H
