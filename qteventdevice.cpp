#include "qteventdevice.h"

QtEventDevice::QtEventDevice(QObject *parent) : QObject(parent)
{

}

void QtEventDevice::onMessage(const EventMsg& msg){
    emit messageReceived(QString::fromStdString(msg.topic), QString::fromStdString(msg.payload));
}
