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
Calculation::Calculation(DataScanSocket *tcpsocket, QString &fip, ushort fport, int max_run, QObject *parent )
    :QObject(parent), m_running(false),  m_saving(false),  m_readygo(false),
      m_objlabel(1), m_featuredim(0), m_ecnum(0), m_samplesize(0), m_max_run(max_run), m_haverun(0),
      dsocket(tcpsocket), m_classifier(new Classifier()),
      m_feedbackSocket(NULL), m_fipadd(fip), m_fport(fport), m_savepath(new QString())
{
    // Note: default training data for most 10 runs, and 160 event labels in a run !!! and 10 downsampling
    qDebug() << "new for double";
    m_trainData = new double[10 * 160 * tcpsocket->basicinfo.EegChannelNum * (1000 * tcpsocket->basicinfo.SamplingRate/1000 / 10)];
    if (m_trainData==NULL)
        qDebug() << "new for m_trainData failed!";
    m_classTag = new double[10 * 160];
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

void Calculation::sendCmd2user(const char command[])
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_0);
    out.setByteOrder(QDataStream::BigEndian);  // bigendian on net
    out.writeRawData(command,1);
    m_feedbackSocket->write(block);
    m_feedbackSocket->waitForBytesWritten();
}

void Calculation::discardData()
{
    if (m_readygo && m_samplesize>0)
    {
        m_samplesize -= m_ecnum;
        emit Printstatus(".. OK, The Latest data has been discarded!");
    }
}

void Calculation::setObj(int objlabel)
{
    emit Printstatus("object: " + QString::number(objlabel));
    m_objlabel = objlabel;
}

void Calculation::startTrain()
{    
    sendCmd2user("A");
    emit Printstatus(".. tRAINing... ..");
    if (m_readygo && m_haverun <= m_max_run)
    {
        // tell user UI's caption
        sendCmd2user("B");
        emit Printstatus(".. tRAINing... ..");

        /* training a new model here */
        m_classifier->setParam();
        m_classifier->train(m_samplesize, m_featuredim, m_classTag, m_trainData);
        emit Printstatus(".. tRAINing is over..");
        sendCmd2user("S");
        // emit Printstatus("训练的performance");
        // compare change of training performance
        // PSUDOCODE
        /*if change < 0 // go worse
            m_samplesize -= m_ecnum;
        else
            go on next run*/
    }
    else if (m_haverun > m_max_run)
    {
        emit Printstatus(".. exceeds the max run ..");
    }
    else
    {
        emit Printstatus(".. Can't train now!' ..");
    }
}

void Calculation::startTest()
{
    if (m_readygo)
    {
        emit Printstatus(".. tTESTing... ..");
        // Note:
        // test data also append to m_trainData! m_samplesize has been updated to
        // this is very important, when a test is over, the test data is discarded.

        double* predict_label = new double[m_ecnum];
        m_classifier->test(m_ecnum, m_featuredim, m_classTag+(m_samplesize-m_ecnum),
                           m_trainData+(m_samplesize-m_ecnum)*m_featuredim, predict_label);
        /*
         *do something with predict_label, i.e. send feedback(result) to users
         */
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_0);
        out.setByteOrder(QDataStream::BigEndian);  // bigendian on net
        char cmd[2] = "F";
        out.writeRawData(cmd, 1);
        for (int i=0; i<m_ecnum; ++i)
        {
            out << predict_label[i];
        }

        m_feedbackSocket->write(block);
        m_feedbackSocket->waitForBytesWritten();   // after this, this tcpsocket would send data
        //end feedback, then delete the temporary space

        emit Printstatus(".. tESTing is over..");
        sendCmd2user("S");
        delete[] predict_label;

        m_samplesize -= m_ecnum;       // the test data is discarded now
    }
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
    m_running = false;
    QThread *curthread = this->thread();
    //qDebug() << "--try to end thread--\n";
    emit Printstatus("..try to end thread..");
    if (curthread->isRunning())
    {
        curthread->quit();
        curthread->wait();  // wait until the thread return from run()
    }
}

void Calculation::calc()
{
    //****************** connect to user socket *********************//
    m_feedbackSocket = new QTcpSocket();     // send feedback to users
    QHostAddress hostAddr(m_fipadd);
    m_feedbackSocket->connectToHost(hostAddr,m_fport);   // connect to userface server
    if (!m_feedbackSocket->waitForConnected(5000))
    {
        emit Printstatus("..fail to connect to userface server..");
        delete m_feedbackSocket;
        m_feedbackSocket = NULL;
        return;
    }
    emit Printstatus("..start read data..");
    // using for write sth. to socket
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_0);

    //****************** initialize variable *********************//

    // about channels and time span
    const qint32 channelnum = dsocket->basicinfo.EegChannelNum + 1;
    const qint32 blockSamnum = dsocket->basicinfo.BlockPnts;
    const int experimenttime = 5 * 60 * dsocket->basicinfo.SamplingRate;  // 5min -> **ms

    // allocation of memory --> 10 for default trials(runs), 160 for default event numbers in a trial(run)!!!
    double *m_rawData = new double[(dsocket->basicinfo.EegChannelNum+1) * experimenttime];
    if (m_rawData==NULL)
        qDebug() << "new for m_rawData failed!";
    int *m_labelList = new int[10 * 160];              // store labels of each sample in m_trainData
    if (m_labelList==NULL)
        qDebug() << "new for m_labelList failed!";
    int *m_eclatency = new int[10 * 160];              // latency (time) of event class in the recorded data
    if (m_eclatency==NULL)
        qDebug() << "new for m_eclatency failed!";

    // invariants for the loop
    int curclm = 0;    //m_rawdata is channel*samplepoints(time), curclm indicates which time we get currently.(for one trial)
    int eventclass, preec = 0;
    AcqMessage *tmpMsq = new AcqMessage();
    //m_classifier = new Classifier();
    std::ofstream ofile;                  // ofstream for saving file

    // For save training samples after preprocessing and segmentation
//    std::ofstream ofsam("F:\\my_cs\\program-related\\Qtprogramming\\bin\\Netreader\\sample.txt");

    // important flags
    m_running = true;
    bool r_inTrial = false;              // true for, a trial is going on
    bool r_justOverTrial = false;        // a run is over just now while a new one has not yet begun

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
                    if (r_inTrial)
                    {
                        /* r_inTrial is true && eventclass is 253 indicates OVER */
                        if(eventclass==253)
                        {
                            emit Printstatus("..TRIAL OVER..");
                            sendCmd2user("A");
                            r_inTrial = false;
                            r_justOverTrial = true;
                        }
                        else
                        {
                            /* record trial data and corresponding event class label which should not be 0*/
                            if (eventclass && eventclass!=preec) // && eventclass!=250
                            {
                                m_labelList[m_ecnum] = eventclass;
                                m_eclatency[m_ecnum] = curclm;
                                ++m_ecnum;
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
                        /* r_inTrial is false && eventclass is 255 indicates BEGIN */
                        if (255 == eventclass)
                        {                        
                            // get the object label when a new trial(run) has started
                            emit GetObj();

                            //TEST
                            emit Printstatus("TRIAL BEGIN!");
                            // training or testing is not allowed during a run
                            m_readygo = false;
                            r_inTrial = true;
                            curclm = m_ecnum = 0;
                        }
                        else if (252 == eventclass)     // the trial is interrupted!!! reset pointers
                        {
                            m_readygo = false;
                            r_inTrial = false;
                            r_justOverTrial = false;
                            curclm = m_ecnum = 0;
                        }
                    }
                    //if (eventclass!=250)  // 250 for mouse click, which should be neglected
                    preec = eventclass;
                }//endfor
    //                if (r_inTrial && saving && ofile.is_open())
    //                    ofile << "\n";
            }//endif
            else
                qDebug() << "..out of memory for calc..\n";
        }

        /*
         *when a trial is over, r_justOverTrial is true
         *the preprocess will be done
         */
        if (r_justOverTrial)
        {
//            for (int p=0; p<m_ecnum; ++p)
//            {
//                ofile << m_labelList[p] << ":" << m_eclatency[p] << "\n";
//            }


            emit Printstatus("..preprocess..");
            ++m_haverun;

            emit Printstatus("%TEST% " + QString::number(m_haverun) + "/TEST/");

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
            unsigned int m_featuredim = (timebeforeonset + timeafteronset) * dsocket->basicinfo.EegChannelNum;
            if ((m_featuredim%downsample) != 0)  // be sure of feature dimension after downsample
            {
                m_featuredim /= downsample;
                ++m_featuredim;
            }
            else
                m_featuredim /= downsample;
            int lenpre = 200 * dsocket->basicinfo.SamplingRate / 1000;  // segment for baseline correct
            pps.Segmentation(m_rawData, curclm, m_trainData+m_samplesize*m_featuredim, m_classTag+m_samplesize, \
                             m_objlabel, m_eclatency, m_ecnum, m_labelList, \
                             lenpre, timebeforeonset, timeafteronset, \
                             dsocket->basicinfo.EegChannelNum, channelnum-1,downsample);
            m_samplesize += m_ecnum;
            emit Printstatus("..segmentation end..");

            // preprocess done, indicates that train or test is avaiable now
            m_readygo = true;
//                if (ofsam)
//                {
//                    for (int i=0; i<m_ecnum; ++i)
//                    {
//                        ofsam << m_classTag[i] << ": ";
//                        for (unsigned int j=0; j<m_featuredim; ++j)
//                        {
//                            ofsam << *(m_trainData + i*m_featuredim + j) << " ";
//                        }
//                        ofsam << "\n";
//                    }
//                    ofsam.close();
//                }


            // in case for preprocessing during a trial
            r_justOverTrial = false;  // don't miss it
            out << 'S';
            m_feedbackSocket->write(block);
            m_feedbackSocket->waitForBytesWritten();
            emit Printstatus(".. NOW Go oN TO the nEXT TriaL ..");

        }// end if (r_justOverTrial)
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

