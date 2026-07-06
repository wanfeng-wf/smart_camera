#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPixmap>
#include <QByteArray>
#include <QTimer>
#include <QElapsedTimer>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void tcpReadSlot();
    void udpReadSlot();



    void on_upButton_clicked();

    void on_downButton_clicked();

    void on_leftButton_clicked();

    void on_rightButton_clicked();

    private:
    Ui::Widget *ui;
    QTcpSocket *tSocket;
    QUdpSocket *uSocket;
    QTimer *helloTimer;
    QTimer *debugTimer;
    quint32 port;
    QByteArray frameBuf;
    QByteArray packetRecv;
    quint32 curFrameId;
    quint32 curFrameSize;
    quint32 recvSize;
    quint32 packetCount;
    quint32 recvPacketCount;
    QElapsedTimer frameTimer;
    quint32 showCount;
    quint32 displayCount;
    quint32 timeoutCount;
    quint32 udpPacketCount;
    quint32 invalidPacketCount;
};

#endif // WIDGET_H
