//#include <QCoreApplication>
#include <QApplication>
#include "DataScanSocket.h"
#include "ScanReader.h"
#include <iostream>
#include <QThread>
#include <QPushButton>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    QString addr("10.14.86.111");
//    ushort pt = 4000;
//    DataScanSocket *dsocket = new DataScanSocket(addr,pt);

//    QThread *mythread = new QThread;
//    dsocket->moveToThread(mythread);

//    QObject::connect(mythread, SIGNAL(started()), dsocket, SLOT(connectToServer()),Qt::QueuedConnection);
//    QObject::connect(dsocket, SIGNAL(disconnected()), mythread, SLOT(quit()),Qt::QueuedConnection);
//    QPushButton *button = new QPushButton("Stop");
//    QObject::connect(button, SIGNAL(clicked()), dsocket, SLOT(closeConnection()));
//    button->show();
//    mythread->start();
    ScanReader newReader;
    newReader.show();
    return a.exec();
}
