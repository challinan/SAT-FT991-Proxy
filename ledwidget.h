#pragma once

#include <QWidget>
#include <QColor>


class LedWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool isOn READ isOn WRITE setIsOn NOTIFY stateChanged DESIGNABLE true)

    // This exposes a color picker directly to the Qt Designer Property Editor
    Q_PROPERTY(QColor ledColor READ ledColor WRITE setLedColor NOTIFY colorChanged DESIGNABLE true)

public:
    explicit LedWidget(QWidget *parent = nullptr);

    bool isOn() const { return m_isOn; }
    QColor ledColor() const { return m_ledColor; }
    void paintEvent(QPaintEvent *event);

public slots:
    void setIsOn(bool on);
     void setLedColor(const QColor &color);
    void toggle();

signals:
    void stateChanged(bool on);
    void colorChanged(const QColor &color);

private:
    void updateStyle();
    bool m_isOn;
    QColor m_ledColor;
    const int m_size = 20; // Default size (diameter)
};

