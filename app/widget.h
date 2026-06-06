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
    quint32 port;
    QByteArray frameBuf;
    quint32 curFrameId;
    quint32 curFrameSize;
    quint32 recvSize;
};

#endif // WIDGET_H
