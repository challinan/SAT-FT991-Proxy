#include "mainwindow.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    // Prepend date and time with milliseconds
    qSetMessagePattern("[%{time h:mm:ss.zzz}] %{%{endif}%{message}");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
