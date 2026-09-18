#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include "config_object.h"

#include <CivProtocol.h>
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

private slots:
    void on_quit_pButton_clicked();
    void on_config_pButton_clicked();
    void on_run_pButton_clicked();
    void on_serialPortComboBox_activated(int index);
    void updateSATFrequencyMain(int freq);
    void updateSATFrequencySub(int freq);
    void updatePowerDisplay(QString S);
    void radioRequestCompleted(quint64 requestId, RadioResponse response);

    void pollRadioStatus();

private:
    QTimer *m_radioPollTimer = nullptr;


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
