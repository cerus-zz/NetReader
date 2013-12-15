#ifndef DATASCANSOCKET_H
#define DATASCANSOCKET_H

#include "MessageQueue.h"
#include <QString>
#include <QByteArray>
#include <QtNetwork/QTcpSocket>

#include <QQueue>
#include "AcqMessage.h"


class AcqBasicInfo
{
public:
    qint32 size;                 // header size
    qint32 EegChannelNum;        //
    qint32 EventChannel;         // indice of event label
    qint32 BlockPnts;            // number of sample points in every block
    qint32 SamplingRate;         // sampling rate
    qint32 DataSize;             // sizeof(data)
    float  fResolution;          // resolution to map raw data from double to int
    AcqBasicInfo() {}
    bool Validation()
    {
        return (BlockPnts!=0 && SamplingRate!=0 && DataSize!=0 && EegChannelNum!=0);
    }
};


/*
 * Note that Qt requirs that the QTcpSocket be in the same thread
 * it is used in. So it cannot be declared as a class variable.
 * It must be in the run thread.
 */
class AcqMessage;
class DataScanSocket : public QTcpSocket
{
    Q_OBJECT

public:
    DataScanSocket(const QString& serverIpAddress, const ushort& serverPort, QObject *parent=0);
    ~DataScanSocket();

    /* public variable */
    AcqBasicInfo basicinfo;      // basic infomation about setting of Neuroscan
    MessageQueue msgque;         // queue for messages
    //QQueue<AcqMessage> TestQue;    

private slots:    
//    void connectionClosedByServer();
    void error();
    void prelusion();
    void receiveData();
    void connectToServer();
    void closeConnection();

signals:
    void PrintStatus(const QString&);
    void fastquit();

private:    
    bool sendRequest(short ctrlcode, short reqnum);    
    const QString mipAddress;   // ip address of server
    const ushort mPort;         // port of server
    const int BasicInfoSize;    // byte size of variables in AcqBasicInfo: 28
    const int maxquesize;       // max size of msgque, more datapackage is byond endurance: 100 (40s)
    AcqMessage *tmpMsg;
    bool Msghead;               // whether to the head of massage, "false" for "the head is gained but body not"
    int prelabel;

    // command control mode
    enum {GeneralControlCode=1, ServerControlCode, ClientControlCode};
    // Description for GeneralControl
    enum {RequestVersion=1, CloseUpConnection};
    // Description for ServerControl
    enum {StartAcquisition=1, StopAcquisition};
    // Description for ClientControl
    enum {RequestEDFHeader=1, RequestASTSetupFile, RequestStartData, RequestStopData, RequestBasicInfo};
    // type of requested data
    enum {DataType_InfoBlock=1, DataType_EegData, InfoType_BasicInfo};
    // sizeof(datatype)
    enum {DataTypeRaw16bit=1, DataTypeRaw32bit};

};
#endif // DATASCAN_H
