#ifndef FT991A_PROXY_H
#define FT991A_PROXY_H

#include <QObject>
#include <QWidget>
#include <QList>
#include <QString>

class ft991a_proxy
{
public:
    ft991a_proxy();

private:
    QList<QString> ft_commands;
};

#endif // FT991A_PROXY_H
