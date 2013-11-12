#include "AcqMessage.h"
#include "DataScanSocket.h"
#include <QtNetwork/QTcpSocket>
#include <QByteArray>
#include <QDataStream>
#include <QHostAddress>
#include <QDebug>
#include <QFile>
#include <fstream>
#include <QString>
#include <QThread>

DataScanSocket::DataScanSocket(const QString& serverIpAddress, const ushort& serverPort, QObject *parent)
    : QTcpSocket(parent), BasicInfoSize(28), mipAddress(serverIpAddress), mPort(serverPort), maxquesize(10000)
{
    connect(this, SIGNAL(connected()), this, SLOT(prelusion()),Qt::QueuedConnection);
//    connect(this, SIGNAL(readyread()), this, SLOT(receivedata()));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError)),
                this, SLOT(error()));
}

DataScanSocket::~DataScanSocket()
{
    msgque.~MessageQueue();
}

void DataScanSocket::connectToServer()
{
    QHostAddress hostAddr(mipAddress);
    connectToHost(hostAddr, mPort);
    /* this step cannot be missed, after this, this tcpsocket would try to connect to server */
    if (!waitForConnected())
    {        
        emit PrintStatus("The socket is not connected");
    }
    else
        emit PrintStatus("connection is established");
    //qDebug() << "Status: " << state() << "\n";
}

void DataScanSocket::closeConnection()
{
    emit PrintStatus("CLOSING the connection");
    if (state()==ConnectedState)      // if the connect has been built up
    {
        /* stop acquire data from device */
        sendRequest(ServerControlCode, StopAcquisition);
        /* stop sending data */
        sendRequest(ClientControlCode, RequestStopData);
        /* tell server to close the connection, so disconnectFromHost() is not needed then */
        sendRequest(GeneralControlCode, CloseUpConnection);
    //    disconnectFromHost();
        /* waitForDisconnected() is not allowed in UnconnectedState, so it should be protected */
        if (state()==ConnectedState && waitForDisconnected())
        {
            //qDebug() << "Status: " << state() << "\n";
            emit PrintStatus("connected State");
        }
    }
    else
    {
        emit PrintStatus("the socket is not connected");
        QThread *curthread = this->thread();
        curthread->quit();
        curthread->wait();
    }
}

void DataScanSocket::error()
{
    //qDebug() << "Error: " <<  this->errorString() << "\n";
    emit PrintStatus(this->errorString());
}

bool DataScanSocket::sendRequest(short ctrlcode, short reqnum)
{
    char id[5] = "CTRL";
    AcqMessage* message = new AcqMessage(id, ctrlcode, reqnum, 0);
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
//    out.setVersion(QDataStream::Qt_5_0);
    out.writeRawData(message->chId,4);
    out.setByteOrder(QDataStream::BigEndian);  // bigendian on net
    out << quint16(message->Code) << quint16(message->Request) << qint32(message->Size);

    write(block);
    waitForBytesWritten();   // after this, this tcpsocket would send data
    qDebug() << "--request sended--\n";
    return true;
}

void DataScanSocket::receiveData()
{    
    QDataStream in(this);
//    in.setVersion(QDataStream::Qt_4_3);

    AcqMessage *tmpMsg = new AcqMessage();

    if (bytesAvailable() > 0)
    {
        in.readRawData(tmpMsg->chId,4);
        tmpMsg->chId[4] = '\0';
        /* just these three fields are transfered from server in big-endian!! */
        in.setByteOrder(QDataStream::BigEndian);
        in >> tmpMsg->Code >> tmpMsg->Request >> tmpMsg->Size;

        /* judge if this is a data packet */
        if (tmpMsg->isDataPacket() && tmpMsg->Size>0 && (bytesAvailable()>=12+tmpMsg->Size) )
        {
            /* Debug:
            * this is for debug
            */
            std::ofstream ofs("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\data.txt", std::ios_base::app);
            if (!ofs)
                qDebug() << "file open failed\n";
            /*end*/

            in.setByteOrder(QDataStream::LittleEndian);
            // gaining data
            qint16 point_16;
            qint32 point_32;

            for (int rw=0; rw<basicinfo.BlockPnts; ++rw)
            {
                for (int cl=0; cl<basicinfo.EegChannelNum; ++cl)
                {
                    if (basicinfo.DataSize==2)
                    {
                        in >> point_16;
                        tmpMsg->pbody.push_back((double)((qint32)point_16 * basicinfo.fResolution));
                        ofs << (double)((qint32)point_16 * basicinfo.fResolution) << " ";
                    }
                    else
                    {
                        in >> point_32;
                        tmpMsg->pbody.push_back((double)(point_32 * basicinfo.fResolution));
                        ofs << (double)(point_32 * basicinfo.fResolution) << " ";
                    }
                }
                /* this is for event label channel, note that the label would not be more than 255*/
                if (basicinfo.DataSize==2)
                {
                    in >> point_16;
                    tmpMsg->pbody.push_back(point_16 & (0x00ff));
                    ofs << (point_16 & (0x00ff)) << "\n";
                }
                else
                {
                    in >> point_32;
                    tmpMsg->pbody.push_back(point_32 & (0x000000ff));
                    ofs << (point_32 & (0x000000ff)) << "\n";
                }

            }
            ofs << "\n";
            /* add the new message to the queue, but we hold a threshold size for this queue */
            /* otherwise, old messages would be get rid of                                   */
            msgque.addMessage(*tmpMsg, maxquesize);

            ofs.close();
        }
        else
            return;
    }

}

void DataScanSocket::prelusion()
{
    // status: "Connected"
    /* gain basic info from server */
    sendRequest(ClientControlCode, RequestBasicInfo);
    /* waitint for 40ms after sending request, since server send 40ms data once */
    AcqMessage msg;
    while (waitForReadyRead())
    {
        /* accept basic info */
        QDataStream in(this);

        if (bytesAvailable() < (12+BasicInfoSize))
            continue;
        else
        {
            in.readRawData(msg.chId,4);
            in.setByteOrder(QDataStream::BigEndian);     // !network BigEndian
            in >> msg.Code >> msg.Request >> msg.Size;
            in.setByteOrder(QDataStream::LittleEndian);  // change back
            if (msg.Code==DataType_InfoBlock && msg.Request==InfoType_BasicInfo
                    && msg.Size==BasicInfoSize)
            {
                in.setFloatingPointPrecision(QDataStream::SinglePrecision);
                in >> basicinfo.size         >> basicinfo.EegChannelNum
                   >> basicinfo.EventChannel >> basicinfo.BlockPnts
                   >> basicinfo.SamplingRate >> basicinfo.DataSize
                   >> basicinfo.fResolution;
            }
            /*Debug:
             *  be sure of head infomation
             */
            QFile ofs("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\data.txt");
            QTextStream out;
            ofs.open(QIODevice::WriteOnly);
            if (ofs.exists())
            {
                out.setDevice(&ofs);
                out << msg.Code << " " << msg.Request << " " << msg.Size << ";; ";
                out << basicinfo.size << " ;"<< basicinfo.EegChannelNum << " ;"
                    << basicinfo.EventChannel << " ;" << basicinfo.BlockPnts << " ;"
                    << basicinfo.SamplingRate << " ;" << basicinfo.DataSize << " ;"
                    << basicinfo.fResolution << "\n\n";

                ofs.close();
            }
            else
                qDebug() << "file open failed\n";
            /*end*/
            break;       // have gained basic infomation, so don't wait here
        }
    }
    /* */
    connect(this, SIGNAL(readyRead()), this, SLOT(receiveData()));

    /* send request for data */
    sendRequest(ServerControlCode, StartAcquisition);
    sendRequest(ClientControlCode, RequestStartData);
}

