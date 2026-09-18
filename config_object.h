#ifndef CONFIGOBJECT_H
#define CONFIGOBJECT_H

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QFile>
#include <QDialog>
#include <QStandardPaths>
#include <QDebug>
#include <QMap>

#define CONFIG_FILE_NAME "/.macrr/macremote-server-config.ini"

class ConfigDialog;

class ConfigObject : public QObject
{
    Q_OBJECT
public:
    explicit ConfigObject(QObject *parent = nullptr);
    void open_config_dialog();
    void key_value_changed(int index);
    void store_new_key_value();
    void delete_key_value();
    void debug_display_map();
    void write_map_to_disk();
    QString get_value_from_key(QString s);
    void get_value_from_key();

private:
    void process_line(QString s);
    // QStringList paramList;    // Used to store contents of config file line by line
    QFile file;
    ConfigDialog *pDialog;
    QMap<QString, QString> configMap;
    QString current_key;
    QString current_value;
};

/**  This is configdialog.h
 *
 */
namespace Ui {
class ConfigDialog;
}

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog() override;

    void set_conf_obj_pointer(QObject *p);

private slots:
    void on_config_buttonBox_clicked();
    void on_config_LineEdit_textEdited(const QString &arg1);
    void on_config_ComboBox_currentIndexChanged(int index);
    void on_config_buttonBox_accepted();
    void on_delete_key_pbutton_clicked();

private:
    QObject *config_obj_p;

public:
    Ui::ConfigDialog *ui;
};
#endif // CONFIGOBJECT_H
