/********************************************************************************
** Form generated from reading UI file 'configdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONFIGDIALOG_H
#define UI_CONFIGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_ConfigDialog
{
public:
    QDialogButtonBox *config_buttonBox;
    QComboBox *config_ComboBox;
    QLineEdit *config_LineEdit;
    QLabel *config_param_label;
    QPushButton *delete_key_pbutton;
    QLabel *keyLabel;
    QLabel *valueLabel;
    QLabel *messageLabel;

    void setupUi(QDialog *ConfigDialog)
    {
        if (ConfigDialog->objectName().isEmpty())
            ConfigDialog->setObjectName("ConfigDialog");
        ConfigDialog->resize(637, 173);
        config_buttonBox = new QDialogButtonBox(ConfigDialog);
        config_buttonBox->setObjectName("config_buttonBox");
        config_buttonBox->setGeometry(QRect(230, 130, 171, 32));
        config_buttonBox->setOrientation(Qt::Horizontal);
        config_buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        config_ComboBox = new QComboBox(ConfigDialog);
        config_ComboBox->setObjectName("config_ComboBox");
        config_ComboBox->setGeometry(QRect(30, 66, 271, 32));
        config_LineEdit = new QLineEdit(ConfigDialog);
        config_LineEdit->setObjectName("config_LineEdit");
        config_LineEdit->setGeometry(QRect(340, 70, 251, 21));
        config_param_label = new QLabel(ConfigDialog);
        config_param_label->setObjectName("config_param_label");
        config_param_label->setGeometry(QRect(30, 16, 281, 16));
        delete_key_pbutton = new QPushButton(ConfigDialog);
        delete_key_pbutton->setObjectName("delete_key_pbutton");
        delete_key_pbutton->setGeometry(QRect(100, 130, 100, 32));
        keyLabel = new QLabel(ConfigDialog);
        keyLabel->setObjectName("keyLabel");
        keyLabel->setGeometry(QRect(40, 53, 58, 16));
        valueLabel = new QLabel(ConfigDialog);
        valueLabel->setObjectName("valueLabel");
        valueLabel->setGeometry(QRect(343, 53, 58, 16));
        messageLabel = new QLabel(ConfigDialog);
        messageLabel->setObjectName("messageLabel");
        messageLabel->setGeometry(QRect(460, 140, 151, 20));

        retranslateUi(ConfigDialog);
        QObject::connect(config_buttonBox, &QDialogButtonBox::accepted, ConfigDialog, qOverload<>(&QDialog::accept));
        QObject::connect(config_buttonBox, &QDialogButtonBox::rejected, ConfigDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(ConfigDialog);
    } // setupUi

    void retranslateUi(QDialog *ConfigDialog)
    {
        ConfigDialog->setWindowTitle(QCoreApplication::translate("ConfigDialog", "Dialog", nullptr));
        config_param_label->setText(QCoreApplication::translate("ConfigDialog", "Select Configuration Parameter", nullptr));
        delete_key_pbutton->setText(QCoreApplication::translate("ConfigDialog", "Delete Key", nullptr));
        keyLabel->setText(QCoreApplication::translate("ConfigDialog", "Key", nullptr));
        valueLabel->setText(QCoreApplication::translate("ConfigDialog", "Value", nullptr));
        messageLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class ConfigDialog: public Ui_ConfigDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGDIALOG_H
