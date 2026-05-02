#ifndef QTEVENTDEVICE_H
#define QTEVENTDEVICE_H

#include <QObject>

class QtEventDevice : public QObject
{
    Q_OBJECT
public:
    explicit QtEventDevice(QObject *parent = nullptr);

signals:

};

#endif // QTEVENTDEVICE_H
