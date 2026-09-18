#include "config_object.h"
#include "mainwindow.h"
#include "ui_configdialog.h"


ConfigObject::ConfigObject(QObject *parent)
    : QObject{parent}
{
    qDebug() << "Config object constructor entered";
    QStringList strList = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
    long size = strList.size();
    if ( size > 1 ) {
        // Something strange happened - this function should return $HOME
        qDebug() << "ConfigObject constructor encountered multiple home locations - exiting";
        QApplication::quit();
    }

    file.setFileName(strList.at(0) + QString(CONFIG_FILE_NAME));
    qDebug() << "File name set to " << file.fileName();

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ConfigObject constructor failed opening filename";
        QApplication::quit();
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if ( line.startsWith("#") ) continue;   // Discard comments
        process_line(line);
    }
    file.close();
    // open_config_dialog();
}

void ConfigObject::process_line(QString s) {
    /* Add key/value pair to our map object */
    QString tmp_key = s.section(',', 0 , 0);
    QString tmp_value = s.section(',', 1);
    configMap.insert(tmp_key, tmp_value);
    // qDebug() << "process_line: " << "tmp_key = " << tmp_key << "  tmp_value = " << tmp_value;
}

void ConfigObject::open_config_dialog() {
    qDebug() << "open_config_dialog entered - this pointer = "<< this;
    pDialog = new ConfigDialog();
    pDialog->set_conf_obj_pointer(static_cast<QObject *>(this));

    // Populate the ComboBox with the keys from the configMap
    QMap<QString, QString>::const_iterator i = configMap.constBegin();
    int index = 0;
    while (i != configMap.constEnd()) {
        pDialog->ui->config_ComboBox->insertItem(index, i.key());
        ++i; ++index;
    }
    pDialog->ui->config_ComboBox->setCurrentText("Rig File");
    pDialog->show();
    get_value_from_key();
}

void ConfigObject::get_value_from_key() {
#if 0
    // Display the entire map
    QMap<QString, QString>::const_iterator i = configMap.constBegin();
    int j = 0;
    while (i != configMap.constEnd()) {
        qDebug() << "Map[" << j << "]:" << i.key() << ": " << i.value();
        ++i; j++;
    }
#endif

    // Get the key currently being displayed by the Combo Box
    current_key = pDialog->ui->config_ComboBox->currentText();
    qDebug() << "key currently being displayed by the Combo Box: " << current_key;
    // Find the key in the map to get it's corresponding value - second parm is default if key not found
    current_value = configMap.value(current_key, "<no value>");
    // Populate the value in the text edit field
    pDialog->ui->config_LineEdit->setText("");
    pDialog->ui->config_LineEdit->insert(current_value);
    qDebug() << "ConfigObject::get_value_from_key(): current_key= " << current_key << "current_value= " << current_value;
    return;
}

void ConfigObject::key_value_changed(int index) {
    qDebug() << "key_value_changed signalled to ConfigObject: index = " << index;
    get_value_from_key();
}

void ConfigObject::store_new_key_value() {
    QString key = pDialog->ui->config_ComboBox->currentText();
    if ( key.isEmpty() ) {
        // Do not store an empty key/value pair
        return;
    }
    QString s = pDialog->ui->config_LineEdit->text();
    configMap.insert(current_key, s);
    qDebug() << "ConfigObject::store_new_key_value() - current key = " << current_key << "New value = " << configMap.value(current_key);

    // Now write the new map to disk
    write_map_to_disk();

}

void ConfigObject::delete_key_value() {
    QString key = pDialog->ui->config_ComboBox->currentText();
    qDebug() << "ConfigObject::delete_key_value() - key is " << key;
    long sz = configMap.remove(key);
    if ( sz != 1 ) {
        qDebug() << "ConfigObject::delete_key_value() failed";
        QApplication::quit();
    }

    // Clear the combo box - the key value has been deleted
    pDialog->ui->config_ComboBox->clear();
    qDebug() << "ConfigObject::delete_key_value() - deleted " << sz << " key/value pair";
    pDialog->ui->messageLabel->setText("Key/Value pair deleted");

    debug_display_map();
    write_map_to_disk();
}

void ConfigObject::debug_display_map() {
    // Print the current map
    QMap<QString, QString>::const_iterator i = configMap.constBegin();
    int j = 0;
    while (i != configMap.constEnd()) {
        qDebug() << "Map[" << j << "]:" << i.key() << ": " << i.value();
        ++i; ++j;
    }
}

void ConfigObject::write_map_to_disk() {
    // Serialize the map to the disk file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "ConfigObject constructor failed opening filename";
        QApplication::quit();
    }

        QTextStream out(&file);
        QMap<QString, QString>::const_iterator i = configMap.constBegin();
        int j = 0;
        while (i != configMap.constEnd()) {
            out << i.key() << ',' << i.value() << Qt::endl;
            qDebug() << "Map[" << j << "]:" << i.key() << ": " << i.value();
            ++i; ++j;
        }
    file.close();
}

QString ConfigObject::get_value_from_key(QString key) {
    return configMap.value(key);
}

/**      This is configdialog.cpp
 ***************************************************
 ***************************************************
 */

ConfigDialog::ConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);
    qDebug() << "ConfigDialog constructor entered - parent pointer = " << parent;
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

void ConfigDialog::set_conf_obj_pointer(QObject *p) {
    config_obj_p = p;
}

void ConfigDialog::on_config_buttonBox_clicked()
{
    // This is the "Cancel" button in the config dialog
    qDebug() << "on_config_buttonBox_clicked() - exiting";
    ConfigObject *cop = static_cast<ConfigObject *>(config_obj_p);
    cop->debug_display_map();
}

void ConfigDialog::on_config_LineEdit_textEdited(const QString &arg1)
{
    // Generated when any text is edited in the line edit widget
    qDebug() << "on_config_LineEdit_textEdited(const QString &arg1) entered: " << arg1;
}


void ConfigDialog::on_config_ComboBox_currentIndexChanged(int index)
{
    // Selection has been made in the config dialog "Key" Combo Box
    ConfigObject *cop = static_cast<ConfigObject *>(config_obj_p);
    cop->key_value_changed(index);
}


void ConfigDialog::on_config_buttonBox_accepted()
{
    // Store the new value to this map (key) entry
    qDebug() << "User pressed OK";
    ConfigObject *cop = static_cast<ConfigObject *>(config_obj_p);
    cop->store_new_key_value();
}


void ConfigDialog::on_delete_key_pbutton_clicked()
{
    ConfigObject *cop = static_cast<ConfigObject *>(config_obj_p);
    cop->delete_key_value();
}
