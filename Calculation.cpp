#include "Calculation.h"
#include "DataScanSocket.h"
#include "PreProcess.h"
#include "Classifier.h"
#include <fstream>
#include <iostream>
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QTcpSocket>
#include <QByteArray>
#include <QDataStream>
#include <QHostAddress>

// default para should not be explicit in definition here
// the  initialization better do seperately, I suppose
Calculation::Calculation(DataScanSocket *tcpsocket, int trials, QString &fip, ushort fport, QObject *parent )
    :QObject(parent), m_running(true), m_objlabel(1),m_saving(false), m_train(0), m_test(false),
      dsocket(tcpsocket), m_samplesize(0), m_trials(trials), m_classifier(new Classifier()),
      m_feedbackSocket(NULL), m_fipadd(fip), m_fport(fport)
{
    m_trainData = new double[trials * 160 * tcpsocket->basicinfo.EegChannelNum * (1000 * tcpsocket->basicinfo.SamplingRate/1000)];
    if (m_trainData==NULL)
        qDebug() << "new for m_trainData failed!";
    m_classTag = new double[trials * 160];
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
    m_trials = traintrials;
}

void Calculation::setObj(QString objlabel)
{
    m_objlabel = objlabel.toInt();
}

void Calculation::statesave()
{
    m_saving = true;
    //qDebug() << "--start saving--\n";
    emit Printstatus("..start saving..");
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
    QTcpSocket *m_feedbackSocket = new QTcpSocket();    // send feedback to users
    QHostAddress hostAddr(m_fipadd);
    m_feedbackSocket->connectToHost(hostAddr,m_fport);   // connect to userface server
    if (!m_feedbackSocket->waitForDisconnected(5000))
    {
        emit Printstatus("..fail to connect to userface server..");
        delete m_feedbackSocket;
        return;
    }
    emit Printstatus("..start read data..");

    // about channels and time span
    const qint32 channelnum = dsocket->basicinfo.EegChannelNum + 1;
    const qint32 blockSamnum = dsocket->basicinfo.BlockPnts;
    const int experimenttime = 5 * 60 * dsocket->basicinfo.SamplingRate;  // 5min -> **ms

    // allocation of memory
    double *m_rawData = new double[(dsocket->basicinfo.EegChannelNum+1) * experimenttime];
    if (m_rawData==NULL)
        qDebug() << "new for m_rawData failed!";
    int *m_labelList = new int[m_trials * 160];             // store labels of each sample in m_trainData
    if (m_labelList==NULL)
        qDebug() << "new for m_labelList failed!";
    int *m_eclatency = new int[m_trials * 160];              // latency (time) of event class in the recorded data
    if (m_eclatency==NULL)
        qDebug() << "new for m_eclatency failed!";

    // invariants for the loop
    int curclm = 0;    //m_rawdata is channel*samplepoints(time), curclm indicates which time we get currently.(for one trial)
    int ecnum = 0;     // number of events of one trial, should be reset to 0 before next trial
    int eventclass, preec;
    AcqMessage *tmpMsq = new AcqMessage();
    //m_classifier = new Classifier();
    QThread * curthread = this->thread();
    std::ofstream ofile("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\data.txt");
    std::ofstream ofsam("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\sample.txt");

    // important flags
    m_running = true;
    bool m_inTrial = false;
    bool m_justOverTrial = false;

    // debug flags
    bool first = true;
    bool second = true;
    bool third = true;

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

                if (!ofile)
                {
                    //qDebug() << "..fail to open file..\n";
                    emit Printstatus("..fail to open file..");
                }
            }
        }

        if (dsocket->msgque.isEmpty())
        {
            /* no data so far, sleep 40ms to wait for */
            curthread->msleep(40);
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
    //            QVector<double>::const_iterator iter = tmpMsq->pbody.begin();
    //            if (curclm < experimenttime)
    //            {
    //                for (int rw=0; rw<blockSamnum; ++rw)
    //                {
    //                    for (int cl=0; cl<channelnum; ++cl)
    //                    {
    //                        *(m_rawdata+experimenttime*cl+curclm) = *iter;
    //                        if (saving && ofile)
    //                            ofile << *iter << " ";
    //                        ++iter;
    //                    }
    //                    if (saving && ofile)
    //                        ofile << "\n";
    //                    ++curclm;
    //                }
    //                if (saving && ofile)
    //                    ofile << "\n";

    //            }//endif
    //            else
    //                qDebug() << "..out of memory for calc..\n";

            if (curclm < experimenttime)
            {
                for (int rw=0; rw<blockSamnum; ++rw)
                {
                    eventclass = tmpMsq->pbody.at(rw*channelnum+channelnum-1);
                    /* using eventclass to judge if the trial should begin or over */
                    if (m_inTrial)
                    {
                        /* m_inTrial is true && eventclass is 253 indicates OVER */
                        if(eventclass==253)
                        {
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
                                 if (m_saving && ofile)
                                     ofile << *(m_rawData+experimenttime*cl+curclm) << " ";
                            }
                            if (m_saving && ofile)
                                ofile << "\n";
                            ++curclm;                           
                        }
                    }
                    else
                    {
                        /* m_inTrial is false && eventclass is 255 indicates BEGIN */
                        if (eventclass==255)
                            m_inTrial = true;
                    }
                    //if (eventclass!=250)  // 250 for mouse click, which should be neglected
                    preec = eventclass;
                }//endfor
    //                if (m_inTrial && saving && ofile)
    //                    ofile << "\n";
            }//endif
            else
                qDebug() << "..out of memory for calc..\n";
        }

        /* when a trial is over, then do work, m_justOverTrial is true */
        if (m_justOverTrial)
        {
            emit Printstatus("..trial is end..");
            for (int p=0; p<ecnum; ++p)
            {
                ofile << m_labelList[p] << ":" << m_eclatency[p] << "\n";
            }           

            m_train = false;
            if (m_test || m_train)
            {
                emit Printstatus("..preprocess..");
                --m_trials;
                /* preprocess raw data */
                PreProcess pps(3);
                pps.removeEog(m_rawData, channelnum, curclm, channelnum);    // remove EOG and DC
                emit Printstatus("..removeEog end..");
                pps.filterPoint(m_rawData, channelnum, curclm, channelnum);  // bandpass filtering using ButterWorth
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

                if (ofsam)
                {
                    for (int i=0; i<ecnum; ++i)
                    {
                        ofsam << m_classTag[i] << ": ";
                        for (unsigned int j=0; j<featuredim; ++j)
                        {
                            ofsam << *(m_trainData + i*featuredim + j) << " ";
                        }
                        ofsam << "\n";
                    }
                    ofsam.close();
                }
                // NOTE: m_samplesize should be updated after every trial
                m_samplesize += ecnum;

                if (m_train && m_trials==0)
                {
                    /* training a new model here */                    
                    m_classifier->setParam();
                    m_classifier->train(m_samplesize, featuredim, m_classTag, m_trainData);
                }

                if (m_test)
                {
                    // test data also append to m_trainData! m_samplesize has been updated to
                    double* predict_label = new double[m_samplesize - ecnum];
                    m_classifier->test(ecnum, featuredim, m_classTag+(m_samplesize-ecnum),
                                m_trainData+(m_samplesize-ecnum)*featuredim, predict_label);
                    /*
                     *do something with predict_label, i.e. send feedback(result) to users
                     */
                    QByteArray block;
                    QDataStream out(&block, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_5_0);
                    out.writeRawData((char*)predict_label, (m_samplesize-ecnum) * sizeof(double));
                    out.setByteOrder(QDataStream::BigEndian);  // bigendian on net

                    m_feedbackSocket->write(block);
                    m_feedbackSocket->waitForBytesWritten();   // after this, this tcpsocket would send data
                    //end feedback, then delete the temporary space

                    delete[] predict_label;
                    m_samplesize = 0;          // this is very important, when a test is over, all old data is discarded.
                }
            }//endif (m_test || m_train)

            // reset
            curclm = ecnum = 0;
            m_justOverTrial = false;  // don't miss it
        }
    }//endwhile

    /* release resources */
    if (ofile)
        ofile.close();
    delete   m_feedbackSocket;
    delete   tmpMsq;
    delete[] m_rawData;
    delete[] m_labelList;
    delete[] m_eclatency;
}

