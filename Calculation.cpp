#include "Calculation.h"
#include "DataScanSocket.h"
#include "PreProcess.h"
#include "Classifier.h"
#include <fstream>
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QTcpSocket>
#include <QByteArray>
#include <QDataStream>
#include <QHostAddress>
#include <QMessageBox>

// default para should not be explicit in definition here
// the  initialization better do seperately, I suppose
Calculation::Calculation(DataScanSocket *tcpsocket, QString &fip, ushort fport, QObject *parent )
    :QObject(parent), m_running(true), m_objlabel(1),m_saving(false), m_train(false), m_test(false),
      dsocket(tcpsocket), m_samplesize(0), m_trials(0), m_classifier(new Classifier()),
      m_feedbackSocket(NULL), m_fipadd(fip), m_fport(fport), m_savepath(new QString())
{
    // Note: default training data for most 10 runs, and 200 event labels in a run !!!
    m_trainData = new double[10 * 200 * tcpsocket->basicinfo.EegChannelNum * (1000 * tcpsocket->basicinfo.SamplingRate/1000)];
    if (m_trainData==NULL)
        qDebug() << "new for m_trainData failed!";
    m_classTag = new double[10 * 200];
    if (m_classTag==NULL)
        qDebug() << "new for m_classTag failed!";
}

Calculation::~Calculation()
{
    if (m_trainData!=NULL)
        delete[] m_trainData;
    if (m_classTag!=NULL)
        delete[] m_classTag;
    if (m_classifier!=NULL)
        delete m_classifier;
}

void Calculation::setPara(int traintrials)
{
    if (traintrials<=0)
    {
        QMessageBox msgBox;
        msgBox.setText("训练次数应该为大于 0！！！");
        msgBox.exec();
    }
    else
    {        
        m_trials = traintrials;
        emit Printstatus("trials: " + QString::number(m_trials));
    }
}

void Calculation::setObj(int objlabel)
{
    emit Printstatus("object: " + QString::number(objlabel));
    m_objlabel = objlabel;
}

void Calculation::startTrain()
{
    emit Printstatus(".. tRAINing... ..");
    m_train = true;
    m_test  = false;
    emit GetPara();     // send for setting m_trials
}

void Calculation::startTest()
{
    emit Printstatus(".. tTESTing... ..");
    m_train = false;
    m_test  = true;
}

void Calculation::startsave(const QString &path)
{
    if (!m_saving)
    {
        emit Printstatus("..start saving..");
        m_saving = true;
        *m_savepath = path;
    }
    else
    {
        emit Printstatus(".. stop saving ..");
        m_saving = false;
    }
}

void Calculation::stoprunning()
{
    if(m_running)
    {
        m_running = false;
        QThread *curthread = this->thread();
        //qDebug() << "--try to end thread--\n";
        emit Printstatus("..try to end thread..");
        curthread->quit();
        curthread->wait();  // wait until the thread return from run()
    }
}

void Calculation::calc()
{
    //****************** connect to user socket *********************//
    QTcpSocket *m_feedbackSocket = new QTcpSocket();     // send feedback to users
    QHostAddress hostAddr(m_fipadd);
    m_feedbackSocket->connectToHost(hostAddr,m_fport);   // connect to userface server
    if (!m_feedbackSocket->waitForConnected(5000))
    {
        emit Printstatus("..fail to connect to userface server..");
        delete m_feedbackSocket;
        return;
    }
    emit Printstatus("..start read data..");


    //****************** initialize variable *********************//

    // about channels and time span
    const qint32 channelnum = dsocket->basicinfo.EegChannelNum + 1;
    const qint32 blockSamnum = dsocket->basicinfo.BlockPnts;
    const int experimenttime = 5 * 60 * dsocket->basicinfo.SamplingRate;  // 5min -> **ms

    // allocation of memory --> 10 for default trials(runs), 200 for default event numbers in a trial(run)!!!
    double *m_rawData = new double[(dsocket->basicinfo.EegChannelNum+1) * experimenttime];
    if (m_rawData==NULL)
        qDebug() << "new for m_rawData failed!";
    int *m_labelList = new int[10 * 200];              // store labels of each sample in m_trainData
    if (m_labelList==NULL)
        qDebug() << "new for m_labelList failed!";
    int *m_eclatency = new int[10 * 200];              // latency (time) of event class in the recorded data
    if (m_eclatency==NULL)
        qDebug() << "new for m_eclatency failed!";

    // invariants for the loop
    int curclm = 0;    //m_rawdata is channel*samplepoints(time), curclm indicates which time we get currently.(for one trial)
    int ecnum = 0;     // number of events of one trial, should be reset to 0 before next trial
    int eventclass, preec = 0;
    AcqMessage *tmpMsq = new AcqMessage();
    //m_classifier = new Classifier();
    std::ofstream ofile;                  // ofstream for saving file

    // For save training samples after preprocessing and segmentation
    std::ofstream ofsam("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\sample.txt");

    // important flags
    m_running = true;
    bool m_inTrial = false;              // true for, a trial is going on
    bool m_justOverTrial = false;        // true for, a trial is over just now and a new trial haven't yet begun

    // debug flags
    bool first = true;
    bool second = true;
    bool third = true;

    //****************** start experiment flow *********************//
    while (m_running)
    {
        if (first)
        {
            qDebug() << "..enter loop..\n";
            first = false;
        }
        if (m_saving)
        {
            if (second)
            {
                qDebug() << "..start save..\n";
                second = false;
            }
            // close old file whatever
            if (ofile.is_open())
            {
                ofile.close();
                ofile.clear();
            }
            QByteArray tba = m_savepath->toLatin1();
            ofile.open(tba.data());       // QString -> char*
            if (!ofile.is_open())
            {
                emit Printstatus("..fail to open file..");
            }
        }

        if (dsocket->msgque.isEmpty())
        {
            /* no data so far, sleep 40ms to wait for */
            this->thread()->msleep(40);
            //continue;
        }
        else
        {
            if (third)
            {
                qDebug() << "..read data..\n";
                third = false;
            }
            /* read data to tmp buffer */
            dsocket->msgque.getMessage(*tmpMsq);

            /* AcqMessage: samplepoints*channel */
            if (curclm < experimenttime)
            {
                for (int rw=0; rw<blockSamnum; ++rw)
                {
                    eventclass = tmpMsq->pbody.at(rw*channelnum+channelnum-1);
                    //TEST
                    if (eventclass != 0 && eventclass!=preec)
                        emit Printstatus(QString::number(eventclass));
                    /* using eventclass to judge if the trial should begin or over */
                    if (m_inTrial)
                    {
                        /* m_inTrial is true && eventclass is 253 indicates OVER */
                        if(eventclass==253)
                        {
                            //TEST
                            emit Printstatus("..TRIAL OVER..");
                            m_inTrial = false;
                            m_justOverTrial = true;
                        }
                        else
                        {
                            /* record trial data and corresponding event class label which should not be 0*/
                            if (eventclass && eventclass!=preec) // && eventclass!=250
                            {
                                m_labelList[ecnum] = eventclass;
                                m_eclatency[ecnum] = curclm;
                                ++ecnum;
                            }
                            for (int cl=0; cl<channelnum; ++cl)
                            {
                                *(m_rawData+experimenttime*cl+curclm) = tmpMsq->pbody.at(rw*channelnum+cl);
//                                 if (m_saving && ofile.isopen())
//                                     ofile << *(m_rawData+experimenttime*cl+curclm) << " ";
                            }
//                            if (m_saving && ofile.is_open())
//                                ofile << "\n";
                            ++curclm;
                        }
                    }
                    else
                    {
                        /* m_inTrial is false && eventclass is 255 indicates BEGIN */
                        if (eventclass==255)
                        {
                            // get the object label when a new trial(run) has started
                            emit GetObj();

                            //TEST
                            emit Printstatus("TRIAL BEGIN!");
                            m_inTrial = true;
                        }
                    }
                    //if (eventclass!=250)  // 250 for mouse click, which should be neglected
                    preec = eventclass;
                }//endfor
    //                if (m_inTrial && saving && ofile.is_open())
    //                    ofile << "\n";
            }//endif
            else
                qDebug() << "..out of memory for calc..\n";
        }

        /* when a trial is over, then do work, m_justOverTrial is true */
        if (m_justOverTrial)
        {
//            for (int p=0; p<ecnum; ++p)
//            {
//                ofile << m_labelList[p] << ":" << m_eclatency[p] << "\n";
//            }

            if ((m_test || m_train))
            {
                emit Printstatus("..preprocess..");
                --m_trials;

                emit Printstatus("%TEST% " + QString::number(m_trials) + "/TEST/");

                /* preprocess raw data */
                PreProcess pps(3);

                // remove EOG and DC
                pps.removeEog(m_rawData, channelnum, curclm, channelnum);
                emit Printstatus("..removeEog end..");

                // bandpass filtering using ButterWorth
                pps.filterPoint(m_rawData, channelnum, curclm, channelnum);
                emit Printstatus("..filter end..");

                // get segments
                int downsample  = 10;      // downsampling rate
                int timebeforeonset = 200, timeafteronset = 800;   // (ms), timebeforeonset can be negative when after onset actually
                timebeforeonset = dsocket->basicinfo.SamplingRate * timebeforeonset / 1000;  // transfer time to numbers of sample points
                timeafteronset =dsocket->basicinfo.SamplingRate * timeafteronset / 1000;
                unsigned int featuredim = (timebeforeonset + timeafteronset) * dsocket->basicinfo.EegChannelNum;
                if ((featuredim%downsample) != 0)  // be sure of feature dimension after downsample
                {
                    featuredim /= downsample;
                    ++featuredim;
                }
                else
                    featuredim /= downsample;
                int lenpre = 200 * dsocket->basicinfo.SamplingRate / 1000;  // segment for baseline correct
                pps.Segmentation(m_rawData, curclm, m_trainData+m_samplesize*featuredim, m_classTag+m_samplesize, \
                                 m_objlabel, m_eclatency, ecnum, m_labelList, \
                                 lenpre, timebeforeonset, timeafteronset, \
                                 dsocket->basicinfo.EegChannelNum, channelnum-1,downsample);
                emit Printstatus("..segmentation end..");

//                if (ofsam)
//                {
//                    for (int i=0; i<ecnum; ++i)
//                    {
//                        ofsam << m_classTag[i] << ": ";
//                        for (unsigned int j=0; j<featuredim; ++j)
//                        {
//                            ofsam << *(m_trainData + i*featuredim + j) << " ";
//                        }
//                        ofsam << "\n";
//                    }
//                    ofsam.close();
//                }
                // NOTE: m_samplesize should be updated after every trial !!!
                m_samplesize += ecnum;

                if (m_train && m_trials==0)
                {
                    /* training a new model here */
                    m_classifier->setParam();
                    m_classifier->train(m_samplesize, featuredim, m_classTag, m_trainData);
                    emit Printstatus(".. tRAINing is over..");
                }

                if (m_test)
                {
                    // test data also append to m_trainData! m_samplesize has been updated to
//                    double* predict_label = new double[m_samplesize - ecnum];
//                    m_classifier->test(ecnum, featuredim, m_classTag+(m_samplesize-ecnum),
//                                m_trainData+(m_samplesize-ecnum)*featuredim, predict_label);
                    /*
                     *do something with predict_label, i.e. send feedback(result) to users
                     */
                    QByteArray block;
                    QDataStream out(&block, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_5_0);
                    //out.writeRawData((char*)predict_label, (m_samplesize-ecnum) * sizeof(double));
                    //TEST
                    out << 0.52345 << -0.7421;
                    out.setByteOrder(QDataStream::BigEndian);  // bigendian on net

                    m_feedbackSocket->write(block);
                    m_feedbackSocket->waitForBytesWritten();   // after this, this tcpsocket would send data
                    //end feedback, then delete the temporary space

                    emit Printstatus(".. tESTing is over..");
                    //delete[] predict_label;
                    m_samplesize = 0;          // this is very important, when a test is over, all old data is discarded.
                }
            }//endif (m_test || m_train)

            // reset
            curclm = ecnum = 0;
            m_justOverTrial = false;  // don't miss it

            //TEST
            emit Printstatus(".. NOW Go oN TO nEXT TriaL ..");
        }
    }//endwhile

    /* release resources */
    if (ofile!=NULL)
    {
        ofile.close();
    }
    delete   m_feedbackSocket;
    delete   tmpMsq;
    delete[] m_rawData;
    delete[] m_labelList;
    delete[] m_eclatency;
}

