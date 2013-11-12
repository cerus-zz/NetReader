#ifndef ACQMESSAGE_H
#define ACQMESSAGE_H

#include <QVector>

class AcqMessage
{
public:
    char* chId;// = new char[5];
    quint16 Code;     //short
    quint16 Request;  //short
    qint32 Size;
    QVector<double> pbody;
    AcqMessage() { chId = new char[5]; }
    AcqMessage(char* id, short c, short req, int size)
    {
        chId = new char[5];
        for (int i=0; i<4; ++i)
            chId[i] = id[i];
        Code = c;
        Request = req;
        Size = size;
    //        pbody = NULL;
    }

//    /* copy constructor */
//    AcqMessage(const AcqMessage &rvl)
//    {
//        for (int i=0; i<4; ++i)
//            chId[i] = rvl.chId[i];
//        Code    = rvl.Code;
//        Request = rvl.Request;
//        Size    = rvl.Size;
//        pbody   = rvl.pbody;
//    }
//    /* assignment operator */
//    AcqMessage& operator =(const AcqMessage& rvl)
//    {
//        for (int i=0; i<4; ++i)
//            chId[i] = rvl.chId[i];
//        Code    = rvl.Code;
//        Request = rvl.Request;
//        Size    = rvl.Size;
//        pbody   = rvl.pbody;
//        return *this;
//    }

    bool isCtrlPacket()
    {
        if (chId[0]=='C' && chId[1]=='T' && chId[2]=='R' && chId[3]=='L')
            return true;
        else
            return false;
    }

    bool isDataPacket()
    {
        if (chId[0]=='D' && chId[1]=='A' && chId[2]=='T' && chId[3]=='A')
            return true;
        else
            return false;
    }
};

#endif // ACQMESSAGE_H
