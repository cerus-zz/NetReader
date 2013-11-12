#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H
#include <QQueue>
#include <QMutexLocker>
#include "AcqMessage.h"
//#include "DataScanSocket.h"
class MessageQueue
{
public:
    MessageQueue()
        :mMutex() {}
    ~MessageQueue()
    {
        msgQue.~QQueue();
        mMutex.~QMutex();
    }
    void addMessage(AcqMessage& amsg, int maxsize)
    {
        QMutexLocker locker(&mMutex);
        /* maxsize <=0 indicates that no sie limit */
        if (maxsize && msgQue.size()>=maxsize)
            msgQue.dequeue();
        msgQue.enqueue(amsg);
    }
    bool getMessage(AcqMessage& amsg)
    {
        QMutexLocker locker(&mMutex);
        if (!msgQue.isEmpty())
        {
            amsg = msgQue.head();
            msgQue.dequeue();
        }
        else
            return false;
        return true;
    }
    bool isEmpty()
    {
        QMutexLocker locker(&mMutex);
        return msgQue.isEmpty();
    }
    int getsize()
    {
        QMutexLocker locker(&mMutex);
        return msgQue.size();
    }
private:
    QQueue<AcqMessage> msgQue;
    QMutex mMutex;
};
#endif // MESSAGEQUEUE_H
