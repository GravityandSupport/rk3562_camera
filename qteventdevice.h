#ifndef QTEVENTDEVICE_H
#define QTEVENTDEVICE_H

#include <QObject>
#include "event_device.h"

class QtEventDevice : public QObject, public EventDevice
{
    Q_OBJECT
public:
    virtual void onMessage(const EventMsg& msg) override;

    explicit QtEventDevice(QObject *parent = nullptr);

signals:
    void messageReceived(const QString& topic, const QString& payload);
};

#endif // QTEVENTDEVICE_H
