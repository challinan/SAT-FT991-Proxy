#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    /*
     * Prevent another instance from starting
     */
    const QString serverName = "SAT-CIV-Proxy-K1AY";

    QLocalServer server;

    /*
     * First try to claim the server name.
     */
    if (!server.listen(serverName)) {

        /*
         * We couldn't claim it. See if another instance
         * is actually listening.
         */
        QLocalSocket socket;
        socket.connectToServer(serverName);

        if (socket.waitForConnected(200)) {

            QMessageBox::information(
                nullptr,
                "Already Running",
                "SAT CI-V Proxy is already running."
                );

            return 0;
        }

        /*
         * Nobody answered, so this is probably a stale
         * socket left behind by a crashed instance.
         */
        QLocalServer::removeServer(serverName);

        /*
         * Try once more.
         */
        if (!server.listen(serverName)) {

            QMessageBox::critical(
                nullptr,
                "Startup Error",
                "Unable to create the application instance lock."
                );

            return 1;
        }
    }

    // Prepend date and time with milliseconds
    qSetMessagePattern("[%{time h:mm:ss.zzz}] %{%{endif}%{message}");

    MainWindow w;
    w.show();
    return a.exec();
}
