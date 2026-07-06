#include "widget.h"
#include "ui_widget.h"
#include <QtEndian>
#include <cstring>

#define UDP_DATA_SIZE 1200
#define FRAME_TIMEOUT_MS 200

static quint32 read_u32(const char *p)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(p));
}

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    uSocket(nullptr),
    helloTimer(nullptr),
    debugTimer(nullptr),
    curFrameId(0),
    curFrameSize(0),
    recvSize(0),
    packetCount(0),
    recvPacketCount(0),
    showCount(0),
    displayCount(0),
    timeoutCount(0),
    udpPacketCount(0),
    invalidPacketCount(0)
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
            uSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1024 * 1024);
            connect(uSocket,&QUdpSocket::readyRead,this,&Widget::udpReadSlot);
        }

        if (debugTimer == nullptr) {
            debugTimer = new QTimer(this);
            connect(debugTimer, &QTimer::timeout, [=]() {
                qDebug() << "app video status"
                         << "udpPackets =" << udpPacketCount
                         << "curFrameId =" << curFrameId
                         << "curFrameSize =" << curFrameSize
                         << "recvPackets =" << recvPacketCount
                         << "totalPackets =" << packetCount
                         << "complete =" << showCount
                         << "display =" << displayCount
                         << "timeout =" << timeoutCount
                         << "invalid =" << invalidPacketCount;
            });
            debugTimer->start(3000);
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
    quint32 latestFrameId = 0;

    while (uSocket->hasPendingDatagrams()) {
        QByteArray ba;
        ba.resize(uSocket->pendingDatagramSize());
        uSocket->readDatagram(ba.data(), ba.size());
        udpPacketCount++;

        if (ba.size() >= 4 &&
            uchar(ba[0]) == 0xff && uchar(ba[1]) == 0xd8 &&
            uchar(ba[ba.size() - 2]) == 0xff && uchar(ba[ba.size() - 1]) == 0xd9) {
            latestFrame = ba;
            continue;
        }

        if (ba.size() < 20) {
            invalidPacketCount++;
            continue;
        }

        quint32 magic = read_u32(ba.constData());
        quint32 frameId = read_u32(ba.constData() + 4);
        quint32 frameSize = read_u32(ba.constData() + 8);
        quint32 offset = read_u32(ba.constData() + 12);
        quint32 dataSize = read_u32(ba.constData() + 16);

        if (magic != 0x53435631 || ba.size() != int(20 + dataSize) ||
            offset + dataSize > frameSize || offset % UDP_DATA_SIZE != 0) {
            invalidPacketCount++;
            continue;
        }

        if (curFrameSize > 0 && frameId == curFrameId &&
            frameTimer.elapsed() > FRAME_TIMEOUT_MS) {
            timeoutCount++;
            qDebug() << "drop timeout frame"
                     << "frameId =" << curFrameId
                     << "size =" << curFrameSize
                     << "recvPackets =" << recvPacketCount
                     << "totalPackets =" << packetCount
                     << "timeoutCount =" << timeoutCount;
            curFrameSize = 0;
            recvSize = 0;
            recvPacketCount = 0;
            frameBuf.clear();
            packetRecv.clear();
            continue;
        }

        if (frameId != curFrameId) {
            if (frameId < curFrameId) {
                continue;
            }
            if (curFrameSize > 0 && recvPacketCount < packetCount) {
                qDebug() << "drop incomplete frame"
                         << "oldFrameId =" << curFrameId
                         << "newFrameId =" << frameId
                         << "recvPackets =" << recvPacketCount
                         << "totalPackets =" << packetCount;
            }
            curFrameId = frameId;
            curFrameSize = frameSize;
            recvSize = 0;
            packetCount = (frameSize + UDP_DATA_SIZE - 1) / UDP_DATA_SIZE;
            recvPacketCount = 0;
            frameBuf.clear();
            frameBuf.resize(frameSize);
            packetRecv.clear();
            packetRecv.resize(packetCount);
            packetRecv.fill(0);
            frameTimer.restart();
        }

        quint32 packetIndex = offset / UDP_DATA_SIZE;
        if (frameSize != curFrameSize || packetIndex >= packetCount) {
            continue;
        }

        if (packetRecv[packetIndex] == 0) {
            memcpy(frameBuf.data() + offset, ba.constData() + 20, dataSize);
            packetRecv[packetIndex] = 1;
            recvSize += dataSize;
            recvPacketCount++;
        }

        if (recvSize >= curFrameSize && recvPacketCount >= packetCount) {
            latestFrame = frameBuf;
            latestFrameId = curFrameId;
            showCount++;
            if (showCount % 30 == 0) {
                qDebug() << "recv complete frame"
                         << "frameId =" << curFrameId
                         << "size =" << curFrameSize
                         << "packets =" << packetCount
                         << "showCount =" << showCount;
            }
            curFrameSize = 0;
            recvSize = 0;
            recvPacketCount = 0;
        }
    }

    if (!latestFrame.isEmpty()) {
        QPixmap pix;
        if (pix.loadFromData(latestFrame)) {
            ui->label->setPixmap(pix);
            displayCount++;
            if (displayCount % 30 == 0) {
                qDebug() << "display frame"
                         << "frameId =" << latestFrameId
                         << "size =" << latestFrame.size()
                         << "pix =" << pix.width() << "x" << pix.height()
                         << "displayCount =" << displayCount;
            }
        } else {
            qDebug() << "load jpeg failed"
                     << "frameId =" << latestFrameId
                     << "size =" << latestFrame.size()
                     << "head =" << QString::number(uchar(latestFrame[0]), 16)
                     << QString::number(uchar(latestFrame[1]), 16)
                     << "tail =" << QString::number(uchar(latestFrame[latestFrame.size() - 2]), 16)
                     << QString::number(uchar(latestFrame[latestFrame.size() - 1]), 16);
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
