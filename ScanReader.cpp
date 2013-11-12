#include "ScanReader.h"
#include "DataScanSocket.h"
#include "Calculation.h"
//#include <QVector>
#include <QThread>
#include <QWidget>
#include <QPushButton>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QDebug>
#include <QApplication>
#include <QList>

ScanReader::ScanReader(QWidget *parent)
    :QDialog(parent), dsocket(NULL), calcprocess(NULL), tcpThread(NULL), calcThread(NULL),
     mipAddress("10.14.86.111"), mPort(4000)
{
    setWindowTitle(tr("ScanReader"));
    resize(QSize(839,575));                    // change size of mainframe, or override sizeHint()

//    /* add button */
//    QPushButton *startButton = new QPushButton(tr("start"));
//    QPushButton *stopButton = new QPushButton(tr("stop"));
//    QPushButton *connectButton = new QPushButton(tr("connect"));
//    QPushButton *saveButton = new QPushButton(tr("save"));
//    QPushButton *disconButton = new QPushButton(tr("disconnect"));
//    QPushButton *testButton = new QPushButton(tr("TEST"));
//    QObject::connect(connectButton, SIGNAL(clicked()), this, SLOT(connect()));
//    QObject::connect(startButton, SIGNAL(clicked()), this, SLOT(start()));
//    QObject::connect(saveButton, SIGNAL(clicked()), this, SIGNAL(save()));
//    QObject::connect(stopButton, SIGNAL(clicked()), this, SIGNAL(stop()));
//    QObject::connect(disconButton, SIGNAL(clicked()), this, SIGNAL(disconnect()));
//    QObject::connect(testButton, SIGNAL(clicked()), this, SLOT(test()));
//    QDialogButtonBox *btnBox = new QDialogButtonBox(Qt::Vertical);
//    btnBox->addButton(startButton, QDialogButtonBox::ActionRole);
//    btnBox->addButton(connectButton, QDialogButtonBox::ActionRole);
//    btnBox->addButton(saveButton, QDialogButtonBox::ActionRole);
//    btnBox->addButton(stopButton, QDialogButtonBox::ActionRole);
//    btnBox->addButton(disconButton, QDialogButtonBox::ActionRole);
//    btnBox->addButton(testButton, QDialogButtonBox::ActionRole);

//    /* add label and edit */
//    QLabel *ipAddressLabel = new QLabel(tr("IP Address"));
//    QLineEdit *ipAddressEdit = new QLineEdit;
//    QLabel *portLabel = new QLabel(tr("Port"));
//    QLineEdit *portEdit = new QLineEdit;
//    portEdit->setText("4000");

//    /* set layout */
//    QFormLayout *formlayout = new QFormLayout;
//    formlayout->addRow(ipAddressLabel, ipAddressEdit);
//    formlayout->addRow(portLabel, portEdit);

//    QHBoxLayout*hboxlayout = new QHBoxLayout;
//    hboxlayout->addLayout(formlayout);
//    hboxlayout->addWidget(btnBox);
//    setLayout(hboxlayout);
      //**************
      //left column
      //**************
      QVBoxLayout *vboxleft = new QVBoxLayout;
      vboxleft->addWidget(createConnectGBox());
      vboxleft->addWidget(createExpGBox());
      vboxleft->addWidget(createOtherGBox());
      vboxleft->layout()->setContentsMargins(0,0,10,0);

      //**************
      //right column
      //**************
      // right up, for testbrowsr
      QVBoxLayout *subvboxright = new QVBoxLayout;
      QLabel *L_caption = new QLabel(tr("连接即实验流程状态显示"));
      QTextBrowser *Brw_status = new QTextBrowser();
      Brw_status->setObjectName("Brw_status");
      subvboxright->addWidget(L_caption);
      subvboxright->addWidget(Brw_status);
      subvboxright->setContentsMargins(0,0,0,30);

      // right down, for buttons
      QGridLayout *subgridright = new QGridLayout;
      QPushButton *Btn_connect_scan = new QPushButton(tr("连接Scan"));
      QPushButton *Btn_disconnect_scan = new QPushButton(tr("断开Scan连接"));
      QPushButton *Btn_start = new QPushButton(tr("开始接受信号数据"));
      QPushButton *Btn_stop = new QPushButton(tr("停止接受信号数据"));
      QPushButton *Btn_savedata = new QPushButton(tr("保存数据"));
      QPushButton *Btn_train = new QPushButton(tr("训练"));
      QPushButton *Btn_predict = new QPushButton(tr("预测"));
      subgridright->addWidget(Btn_connect_scan,0,0);
      subgridright->addWidget(Btn_disconnect_scan,0,1);
      subgridright->addWidget(Btn_start,1,0);
      subgridright->addWidget(Btn_stop,1,1);
      subgridright->addWidget(Btn_savedata,2,0);
      subgridright->addWidget(Btn_train,3,0);
      subgridright->addWidget(Btn_predict,3,1);

      QVBoxLayout *vboxright = new QVBoxLayout;
      vboxright->addLayout(subvboxright);
      vboxright->addLayout(subgridright);
      vboxright->layout()->setContentsMargins(10,0,0,0);

      //set slots and signals
      QObject::connect(Btn_connect_scan, SIGNAL(clicked()), this, SLOT(connect()));
      QObject::connect(Btn_start, SIGNAL(clicked()), this, SLOT(start()));
      QObject::connect(Btn_savedata, SIGNAL(clicked()), this, SIGNAL(save()));
      QObject::connect(Btn_stop, SIGNAL(clicked()), this, SIGNAL(stop()));
      QObject::connect(Btn_disconnect_scan, SIGNAL(clicked()), this, SIGNAL(disconnect()));

      //****************************
      //global layout for mainframe
      //****************************
      QHBoxLayout*hboxlayout = new QHBoxLayout;
      hboxlayout->addLayout(vboxleft);
      hboxlayout->addLayout(vboxright);
      hboxlayout->layout()->setContentsMargins(QMargins(45,20,45,20));
      setLayout(hboxlayout);
}

QGroupBox *ScanReader::createConnectGBox()
{
    QGroupBox *connectBox = new QGroupBox(tr("网络连接相关参数"));

    QFormLayout *form1 = new QFormLayout;

    form1->setVerticalSpacing(10);
    form1->setHorizontalSpacing(40);
    form1->layout()->setContentsMargins(QMargins(20,20,20,20));

    QLabel *L_scan_ip = new QLabel(tr("Scan的IP地址"));
    QLineEdit *Led_scan_ip = new QLineEdit;
    Led_scan_ip->setText("10.14.86.111");
    Led_scan_ip->setObjectName("Led_scan_ip");     // set name of this widget, which can be searched by the name
    form1->addRow(L_scan_ip, Led_scan_ip);

    QLabel *L_scan_port = new QLabel(tr("scan的端口号"));
    QLineEdit *Led_scan_port = new QLineEdit;
    Led_scan_port->setText("4000");
    Led_scan_port->setObjectName("Led_scan_port");
    form1->addRow(L_scan_port, Led_scan_port);

    QLabel *L_user_ip = new QLabel(tr("用户界面的IP地址"));
    QLineEdit *Led_user_ip = new QLineEdit;
    Led_user_ip->setObjectName("Led_user_ip");
    form1->addRow(L_user_ip, Led_user_ip);

    QLabel *L_user_port = new QLabel(tr("用户界面的端口号"));
    QLineEdit *Led_user_port = new QLineEdit;
    Led_user_port->setObjectName("Led_user_port");
    form1->addRow(L_user_port, Led_user_port);

    connectBox->setLayout(form1);
    return connectBox;
}

QGroupBox *ScanReader::createExpGBox()
{
    QGroupBox *expBox = new QGroupBox(tr("实验流程相关参数"));

    QFormLayout *form2 = new QFormLayout;

    form2->setVerticalSpacing(10);
    form2->setHorizontalSpacing(40);
    form2->layout()->setContentsMargins(QMargins(20,20,20,20));

    QLabel *L_train_round = new QLabel(tr("训练轮数"));
    QSpinBox *Spn_train_round = new QSpinBox;
    form2->addRow(L_train_round, Spn_train_round);

    QLabel *L_object_label = new QLabel(tr("本轮目标标签号"));
    QLineEdit *Led_object_label = new QLineEdit;
    form2->addRow(L_object_label, Led_object_label);

    expBox->setLayout(form2);

    QObject::connect(Spn_train_round, SIGNAL(valueChanged(int)), calcprocess, SLOT(setPara(int)));
    QObject::connect(Led_object_label, SIGNAL(textEdited(QString)), calcprocess, SLOT(setObj(QString)));
    return expBox;
}

QGroupBox *ScanReader::createOtherGBox()
{
    QGroupBox *otherBox = new QGroupBox(tr("其他参数"));

    QFormLayout *form3 = new QFormLayout;

    form3->setVerticalSpacing(10);
    form3->setHorizontalSpacing(40);
    form3->layout()->setContentsMargins(QMargins(20,20,20,20));

    QLabel *L_save_path = new QLabel(tr("数据保存路径"));
    QLineEdit *Led_save_path = new QLineEdit;
    form3->addRow(L_save_path, Led_save_path);

    otherBox->setLayout(form3);
    return otherBox;
}

void ScanReader::start()
{        
    if (dsocket &&tcpThread->isRunning() && calcprocess==NULL)  //
    {
        QString fadd = (this->findChild<QLineEdit *>("Led_user_ip"))->text();
        ushort fport = (this->findChild<QLineEdit *>("Led_user_port"))->text().toUShort();

        calcprocess = new Calculation(dsocket,1,fadd, fport);
        qDebug() << "MARKED 1";
        calcThread = new QThread();
        qDebug() << "MARKED 2";
        calcprocess->moveToThread(calcThread);
        qDebug() << "MARKED 3";
        QObject::connect(calcThread, SIGNAL(started()), calcprocess, SLOT(calc()), Qt::QueuedConnection);
        /* here should use Qt::DirectConnection, otherwise, the slots just don't response */
        QObject::connect(this, SIGNAL(save()), calcprocess, SLOT(statesave()), Qt::DirectConnection);
        QObject::connect(this, SIGNAL(stop()), calcprocess, SLOT(stoprunning()), Qt::DirectConnection);
        QObject::connect(calcThread, SIGNAL(finished()), this, SLOT(PrintCalcStop()), Qt::QueuedConnection);
        // signals for updating contents of QTextBrower
        QObject::connect(calcprocess, SIGNAL(PrintStatus(const QString&)),this, SLOT(Printstatus(const QString&)));

        calcThread->start();
    }
    else
    {
        (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- connect to Scan first!!!\n");
    }
}

//ScanReader::~ScanReader()
//{
//    if (dsocket!=NULL)
//        delete dsocket;
//    if (calcprocess!=NULL)
//        delete calcprocess;
//    if (tcpThread!=NULL)
//        delete tcpThread;
//    if (calcThread)
//        delete calcThread;
//}

void ScanReader::connect()
{
    if (dsocket==NULL)
    {
        (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- CONNECTING!\n");
        mipAddress = (this->findChild<QLineEdit *>("Led_scan_ip"))->text();
        mPort = (this->findChild<QLineEdit *>("Led_scan_port"))->text().toUShort();

        dsocket = new DataScanSocket(mipAddress, mPort);
        tcpThread = new QThread();
        dsocket->moveToThread(tcpThread);

        QObject::connect(tcpThread, SIGNAL(started()), dsocket, SLOT(connectToServer()),Qt::DirectConnection);
        QObject::connect(dsocket, SIGNAL(disconnected()), tcpThread, SLOT(quit()),Qt::DirectConnection);
        QObject::connect(this, SIGNAL(disconnect()), dsocket, SLOT(closeConnection()), Qt::QueuedConnection);
        QObject::connect(tcpThread, SIGNAL(finished()), this, SLOT(PrintTcpStop()), Qt::QueuedConnection);

        // signals for updating contents of QTextBrower
        QObject::connect(dsocket, SIGNAL(PrintStatus(const QString&)),this, SLOT(Printstatus(const QString&)));

        tcpThread->start();
    }
}

void ScanReader::PrintTcpStop()
{
    if (dsocket)
    {
        //dsocket->~DataScanSocket();
        dsocket = NULL;
        tcpThread->~QThread();
        tcpThread = NULL;
        (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- the socket thread is finished!\n");
    }
}

void ScanReader::PrintCalcStop()
{
    if (calcprocess)
    {
        /* must destroy the object in case for mem leak */
        calcprocess->~Calculation();
        calcprocess = NULL;
        calcThread->~QThread();
        calcThread = NULL;
        (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- the calc thread is finished!\n");
    }
}

void ScanReader::Printstatus(const QString& status)
{
    (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- " + status + "\n");
}

void ScanReader::test()
{
    (this->findChild<QTextBrowser *>("Brw_status"))->insertPlainText("-- tcpThread is finished\n");
}
