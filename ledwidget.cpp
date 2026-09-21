#include "ledwidget.h"
#include <QStyleOption>
#include <QPainter>

LedWidget::LedWidget(QWidget *parent)
    : QWidget(parent), m_isOn(false), m_ledColor(Qt::lightGray) {
    // Keep it perfectly square to ensure it stays a circle
    setFixedSize(m_size, m_size);
    updateStyle();
}

void LedWidget::setIsOn(bool on) {
    if (m_isOn == on) return;
    m_isOn = on;
    updateStyle();
    emit stateChanged(m_isOn);
}

void LedWidget::setLedColor(const QColor &color) {
    if (m_ledColor == color) return;
    m_ledColor = color;
    updateStyle();
    emit colorChanged(m_ledColor);
}

void LedWidget::toggle() {
    setIsOn(!m_isOn);
}

void LedWidget::updateStyle() {
    QString radius = QString::number(m_size / 2);

    if (m_isOn) {
        // Dynamically grab the hex colors from our QColor object
        QString centerColor = m_ledColor.lighter(150).name(); // Bright core
        QString baseColor = m_ledColor.name();               // True color
        QString borderColor = m_ledColor.darker(130).name();  // Dark edge ring

        setStyleSheet(QString(
                          "background-color: qradialgradient(cx:0.3, cy:0.3, radius:1, fx:0.3, fy:0.3, stop:0 %1, stop:1 %2);"
                          "border: 2px solid %3;"
                          "border-radius: %4px;"
                          ).arg(centerColor, baseColor, borderColor, radius));
    } else {
        // Standard "OFF" dark gray appearance
        setStyleSheet(QString(
                          "background-color: qradialgradient(cx:0.3, cy:0.3, radius:1, fx:0.3, fy:0.3, stop:0 #bdc3c7, stop:1 #7f8c8d);"
                          "border: 2px solid #596364;"
                          "border-radius: %1px;"
                          ).arg(radius));
    }
}

void LedWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Boilerplate code required to make QWidget subclasses respect stylesheets
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}