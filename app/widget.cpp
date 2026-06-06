#include "widget.h"
#include "ui_widget.h"
#include <QtEndian>
#include <cstring>

static quint32 read_u32(const char *p)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(p));
}

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    uSocket(nullptr),
    helloTimer(nullptr),
    curFrameId(0),
    curFrameSize(0),
    recvSize(0)
{
    ui->setupUi(this);

    tSocket = new QTcpSocket;
    tSocket->connectToHost("106.12.26.151", 8000);

    connect(tSocket,&QTcpSocket::connected,[=]()
    {
        qDebug()<< "连接服务器成功"    ;

        QJsonObject obj;
        obj.insert("cmd","get_video_data");
        obj.insert("appid","1000");
        obj.insert("deviceid","0001");

        tSocket->write(QJsonDocument(obj).toJson());
        tSocket->flush();
    });
    connect(tSocket,&QTcpSocket::readyRead,this,&Widget::tcpReadSlot);
    connect(tSocket,&QTcpSocket::disconnected,[=]()
    {
        qDebug() << "服务器断开连接";
    });
}

Widget::~Widget()
{
    delete ui;
}

void Widget::tcpReadSlot()
{
    QByteArray ba = tSocket->readAll();
    QJsonObject obj = QJsonDocument::fromJson(ba).object();
    if(obj.value("cmd").toString()== "reply_port_info")
    {
        port = obj.value("port").toInt();

        //初始化QUdpsocket
        if (uSocket == nullptr) {
            uSocket = new QUdpSocket(this);
            connect(uSocket,&QUdpSocket::readyRead,this,&Widget::udpReadSlot);
        }

        if (helloTimer == nullptr) {
            helloTimer = new QTimer(this);
            connect(helloTimer, &QTimer::timeout, [=]() {
                uSocket->writeDatagram(QByteArray("hello"), QHostAddress("106.12.26.151"), port);
            });
        }

        uSocket->writeDatagram(QByteArray("hello"), QHostAddress("106.12.26.151"), port);
        helloTimer->start(1000);
        qDebug() << "发送UDP hello, port =" << port;
    }
    else {
          qDebug() <<"返回服务器数据有误"  ;
    }
}

void Widget::udpReadSlot()
{
    QByteArray latestFrame;

    while (uSocket->hasPendingDatagrams()) {
        QByteArray ba;
        ba.resize(uSocket->pendingDatagramSize());
        uSocket->readDatagram(ba.data(), ba.size());

        if (ba.size() >= 4 &&
            uchar(ba[0]) == 0xff && uchar(ba[1]) == 0xd8 &&
            uchar(ba[ba.size() - 2]) == 0xff && uchar(ba[ba.size() - 1]) == 0xd9) {
            latestFrame = ba;
            continue;
        }

        if (ba.size() < 20) {
            continue;
        }

        quint32 magic = read_u32(ba.constData());
        quint32 frameId = read_u32(ba.constData() + 4);
        quint32 frameSize = read_u32(ba.constData() + 8);
        quint32 offset = read_u32(ba.constData() + 12);
        quint32 dataSize = read_u32(ba.constData() + 16);

        if (magic != 0x53435631 || ba.size() != int(20 + dataSize)) {
            continue;
        }

        if (offset == 0) {
            curFrameId = frameId;
            curFrameSize = frameSize;
            recvSize = 0;
            frameBuf.clear();
            frameBuf.resize(frameSize);
        }

        if (frameId != curFrameId || offset != recvSize || offset + dataSize > quint32(frameBuf.size())) {
            continue;
        }

        memcpy(frameBuf.data() + offset, ba.constData() + 20, dataSize);
        recvSize += dataSize;

        if (recvSize >= curFrameSize) {
            latestFrame = frameBuf;
            recvSize = 0;
        }
    }

    if (!latestFrame.isEmpty()) {
        QPixmap pix;
        if (pix.loadFromData(latestFrame)) {
            ui->label->setPixmap(pix);
            if (helloTimer != nullptr) {
                helloTimer->stop();
            }
        }
    }
}

void Widget::on_upButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd","control");
    obj.insert("action","up");
    obj.insert("appid","1000");
    obj.insert("deviceid","0001");

    tSocket->write(QJsonDocument(obj).toJson());
    tSocket->flush();
    qDebug() << "send control up";
}

void Widget::on_downButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd","control");
    obj.insert("action","down");
    obj.insert("appid","1000");
    obj.insert("deviceid","0001");

    tSocket->write(QJsonDocument(obj).toJson());
    tSocket->flush();
    qDebug() << "send control down";
}

void Widget::on_leftButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "control");
    obj.insert("action", "left");
    obj.insert("appid", "1000");
    obj.insert("deviceid", "0001");

    tSocket->write(QJsonDocument(obj).toJson());
    tSocket->flush();
    qDebug() << "send control left";
}

void Widget::on_rightButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd","control");
    obj.insert("action","right");
    obj.insert("appid","1000");
    obj.insert("deviceid","0001");

    tSocket->write(QJsonDocument(obj).toJson());
    tSocket->flush();
    qDebug() << "send control right";
}
