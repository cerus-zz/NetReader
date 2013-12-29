#include "Calculation.h"
#include "DataScanSocket.h"
#include "PreProcess.h"
#include "Classifier.h"
#include <sstream>
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
      m_objlabel(1), m_featuredim(0), m_samplesize(0), m_max_run(max_run),
      dsocket(tcpsocket), m_classifier(new Classifier()),
      m_feedbackSocket(NULL), m_fipadd(fip), m_fport(fport), m_ofile(NULL)
{
    // by default here, a feature vector includes 200ms before and 800ms after onset
    // downsampling rate for 10.
    int downsample  = 10;      // downsampling rate
    int timebeforeonset = 200, timeafteronset = 800;   // (ms), timebeforeonset can be negative when after onset actually
    timebeforeonset = dsocket->basicinfo.SamplingRate * timebeforeonset / 1000;  // transfer time to numbers of sample points
    timeafteronset =dsocket->basicinfo.SamplingRate * timeafteronset / 1000;
    // EXCLUDing the EOG channel ! ! !
    m_featuredim = (timebeforeonset + timeafteronset) * (dsocket->basicinfo.EegChannelNum-1);
    if ((m_featuredim%downsample) != 0)  // be sure of feature dimension after downsampling
    {
        m_featuredim /= downsample;
        ++m_featuredim;
    }
    else
        m_featuredim /= downsample;
//    qDebug() << "m_featuredim is :" << m_featuredim << "!!!!!!!!!!!!!!!!!!!!!!!\n";
//    m_featuredim = 6000;
    // Note: default training data for most 10 runs, and 250 event labels in a run !!! and 10 downsampling
    qDebug() << "new for double";
    m_trainData = new double[10 * 250 * m_featuredim];
    if (m_trainData==NULL)
        qDebug() << "new for m_trainData failed!";
    m_classTag = new double[10 * 250];
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
    if (m_readygo && m_ecnums.size()>0)
    {
        m_samplesize -= *(m_ecnums.end()-1);
        m_ecnums.pop_back();
        emit Printstatus(".. OK, The Latest data has been discarded!\n.. the remaining Training RUN is: " + QString::number(m_ecnums.size()) + ".. ");
    }
    else
    {
        emit Printstatus(".. the remaining Training RUN is: " + QString::number(m_ecnums.size()) + ".. ");
    }
}

void Calculation::startTrain()
{    
    if (m_readygo)
    {
        if (0 == m_ecnums.size())
        {
            emit Printstatus("..NO training data!..");
            return;
        }
        if (m_ecnums.size() > m_max_run)
        {
            // the run over the m_max_runTH run will not be trained
            // and will be covered by next run
            m_samplesize -= *(m_ecnums.end()-1);
            m_ecnums.pop_back();
            emit Printstatus(".. exceeds the max run, the new run WON'T be trained ..");
        }

        // tell user UI's caption
        sendCmd2user("B");
        emit Printstatus(".. tRAINing... ..");

        /* training a new model here */
        m_classifier->setParam();
        qDebug() << "******when train, m_featuredim is " << m_featuredim << "\n";
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
    else
    {
        emit Printstatus(".. Can't train now!' ..");
    }
}

void Calculation::startTest()
{
    if (m_readygo)
    {
        if (0==m_ecnums.size())
        {
            emit Printstatus(".. no test data ..");
            return;
        }
        emit Printstatus(".. tTESTing... ..");
        sendCmd2user("C");
        // Note:
        // test data also append to m_trainData! m_samplesize has been updated to
        // this is very important, when a test is over, the test data is discarded.
        int ecnum = *(m_ecnums.end()-1);
        qDebug() << QString::number(ecnum) << "YYY\n";
        double* predict_label = new double[ecnum];
        double accuracy = m_classifier->test(ecnum, m_featuredim, m_classTag+(m_samplesize-ecnum),
                           m_trainData+(m_samplesize-ecnum)*m_featuredim, predict_label);
        if (-1 == accuracy)
        {
            emit Printstatus("Train First!!!");
            sendCmd2user("A");
        }
        else
        {
            emit Printstatus(".. test accuracy is: " + QString::number(accuracy) + " ..");
            double ap;
            double auc = m_classifier->AUCofROC(predict_label, m_classTag+(m_samplesize-ecnum), ecnum, ap);
            emit Printstatus(".. test AUC is: " + QString::number(auc) + " ..");
            emit Printstatus(".. result AP is: " + QString::number(ap) + " ..");
            //do something with predict_label, i.e. send feedback(result) to users
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_5_0);
            out.setByteOrder(QDataStream::BigEndian);  // bigendian on net
            char cmd[2] = "F";
            out.writeRawData(cmd,1);

            for (int i=0; i<ecnum; ++i)
            {
                out << predict_label[i];
            }

            m_feedbackSocket->write(block);
            m_feedbackSocket->waitForBytesWritten();   // after this, this tcpsocket would send data
            //end feedback, then delete the temporary space

            emit Printstatus(".. tESTing is over..");
        }
        delete[] predict_label;      
    }
}

void Calculation::startsave(const QString &path)
{
    if (!m_saving)
    {
        emit Printstatus("..start saving..");
        m_saving = true;
        m_ofile.open(path.toStdString().c_str());    // a non-existed file will be built by default
    }
    else
    {
        m_ofile.close();
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
    m_feedbackSocket = new QTcpSocket();
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

    //****************** initialize variable *********************//

    // about channels and time span
    const qint32 channelnum = dsocket->basicinfo.EegChannelNum;  // NOT including EVENT channel!!!!!
    const qint32 blockSamnum = dsocket->basicinfo.BlockPnts;
    const int experimenttime = 3 * 60 * dsocket->basicinfo.SamplingRate;  // 3min -> **s

    // allocation of memory --> 10 for default trials(runs), 160 for default event numbers in a trial(run)!!!
    double *m_rawData = new double[(channelnum+1) * experimenttime];  // 1 for event channel
    if (m_rawData==NULL)
        qDebug() << "new for m_rawData failed!";
    int *m_labelList = new int[300];              // store labels of each sample in m_trainData
    if (m_labelList==NULL)
        qDebug() << "new for m_labelList failed!";
    int *m_eclatency = new int[300];              // latency (time) of event class in the recorded data
    if (m_eclatency==NULL)
        qDebug() << "new for m_eclatency failed!";

    //invariants for the loop
    int curclm = 0;    //m_rawdata is channel*samplepoints(time), curclm indicates which time we get currently.(for one trial)
    int ecnum = 0;     // event number of current run
    int eventclass, preec = 0;
    int runs = 0;
    AcqMessage *tmpMsq = new AcqMessage();
    //m_classifier = new Classifier();
    PreProcess *pps = new PreProcess(3);

    // For save training samples after preprocessing and segmentation
    std::ofstream ofsam("F:\\sample.txt", std::ios_base::app);

    // important flags
    m_running = true;
    bool r_inTrial = false;              // true for, a trial is going on
    bool r_justOverTrial = false;        // a run is over just now while a new one has not yet begun

    // debug flags
    bool first = true;
    bool second_on_off = true;
    bool third = true;

    //****************** start experiment flow *********************//
    while (m_running)
    {
        if (first)
        {
            qDebug() << "..enter loop..\n";
            first = false;
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
                    //  event channel is the LAST channel
                    eventclass = tmpMsq->pbody.at(rw*(channelnum+1)+channelnum);
                    /* using eventclass to judge if the trial should begin or over */
                    if (r_inTrial)
                    {
                        // show labels in real time
                        if (eventclass != 0 && eventclass!=preec)
                        {
                            emit Printstatus(QString::number(eventclass));
                        }
                        /* r_inTrial is true && eventclass is 253 indicates OVER */
                        if(eventclass==253)
                        {
                            emit Printstatus("..TRIAL OVER..");
                            sendCmd2user("A");
                            r_inTrial = false;
                            r_justOverTrial = true;
                            second_on_off = true;
                            continue;
                        }
                        else if (252 == eventclass)     // the trial is interrupted!!! reset pointers
                        {
                            m_readygo = false;
                            r_inTrial = false;
                            r_justOverTrial = false;
                            second_on_off = true;
                            continue;
                        }
                        else
                        {
                            // record event label and latency
                            if (eventclass!=0 && eventclass!=preec)
                            {
                                m_labelList[ecnum] = eventclass;
                                m_eclatency[ecnum] = curclm+1;
                                ++ecnum;
                            }
                            // record trial data, including event channel for saving data completely.
                            for (int cl=0; cl<1+channelnum; ++cl)
                            {
                                *(m_rawData+experimenttime*cl+curclm) = tmpMsq->pbody.at(rw*(1+channelnum)+cl);
                            }
                            if (m_saving && m_ofile.is_open())
                            {
                                for (int cl=0; cl<1+channelnum; ++cl)
                                {
                                    m_ofile << *(m_rawData+experimenttime*cl+curclm) << " ";
                                }
                                m_ofile << "\n";
                            }
                            ++curclm;
                        }
                    }
                    else
                    {
                        /* r_inTrial is false && eventclass is 255 indicates BEGIN */
                        if (255 == eventclass)
                        {
                            // get the object label when a new trial(run) has started
                            //emit GetObj();

                            //TEST
                            emit Printstatus("TRIAL BEGIN!");
                            // training or testing is not allowed during a run
                            m_readygo = false;
                            r_inTrial = true;
                            curclm = ecnum = 0;
                        }
                        else if (second_on_off && eventclass!=0 && eventclass<200)
                        {
                            // note that, here we think that if a label, which is not equal to any of
                            // {252(break), 253(over),255(start)}, must be an object label of a run
                            // if it doesn't emerge during a run, i.e. r_inTrial is true.
                            second_on_off = false;
                            m_objlabel = eventclass;
                            emit ShowObj(m_objlabel);
                            emit Printstatus("OBJ Label: " + QString::number(m_objlabel));
                        }

                    }
                    preec = eventclass;

                }//endfor

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
            if (m_ofile.is_open())
            {
                for (int p=0; p<ecnum; ++p)
                {
                    m_ofile << m_labelList[p] << ":" << m_eclatency[p] << "\n";
                }
            }
            ++runs;
            emit Printstatus("..IT iS RUN: " + QString::number(runs)+ " .." );
            emit Printstatus("..preprocess..");

            /* preprocess raw data */
            // remove EOG and DC, note that EOG channel is the last one by default!
            pps->removeEog(m_rawData, experimenttime, channelnum, curclm, channelnum);
            emit Printstatus("..removeEog end.. curclm is :" + QString::number(curclm));

            // bandpass filtering using ButterWorth
            pps->filterPoint(m_rawData, experimenttime, channelnum, curclm, channelnum);
            emit Printstatus("..filter end..");

            // get segments

            // limit of buffersize, 10 runs data will be stored in memory
            // if the number of valid runs is equal or more than 10,
            // the 10th run will be discarded for the newest run.
            if (m_ecnums.size() >= 10)
            {
                m_samplesize -= *(m_ecnums.end()-1);       // the test data is discarded now
                m_ecnums.pop_back();
            }
            int downsample  = 10;      // downsampling rate
            int timebeforeonset = 200, timeafteronset = 800;   // (ms), timebeforeonset can be negative when after onset actually
            timebeforeonset = dsocket->basicinfo.SamplingRate * timebeforeonset / 1000;  // transfer time to numbers of sample points
            timeafteronset =dsocket->basicinfo.SamplingRate * timeafteronset / 1000;
            int lenpre = 200 * dsocket->basicinfo.SamplingRate / 1000;  // segment for baseline correct

            pps->Segmentation(m_rawData, experimenttime, m_trainData+m_samplesize*m_featuredim, m_classTag+m_samplesize, \
                             m_objlabel, m_eclatency, ecnum, m_labelList, \
                             lenpre, timebeforeonset, timeafteronset, \
                             dsocket->basicinfo.EegChannelNum, dsocket->basicinfo.EegChannelNum, downsample);
            m_samplesize += ecnum;
            m_ecnums.push_back(ecnum);
            emit Printstatus("..segmentation end ..\n.. training RUN: " + QString::number(m_ecnums.size())
                             + " (+" + QString::number(*(m_ecnums.end()-1)) + ") ..");

            // preprocess done, indicates that train or test is avaiable now
            m_readygo = true;

            // TEST: output samples (feature vectors)****************************
//            if (ofsam)
//            {
//                int havesize = m_samplesize*m_featuredim;
//                for (int i=0; i<ecnum; ++i)
//                {
//                    ofsam << m_classTag[i] << " ";
//                    for (int j=0; j<m_featuredim; ++j)
//                    {
//                        ofsam << *(m_trainData + havesize + i*m_featuredim + j) << " ";
//                    }
//                    ofsam << "\n";
//                }
//            }
            //********************************************************************

            // in case for preprocessing during a trial
            r_justOverTrial = false;  // don't miss it
            emit Printstatus(".. NOW Go oN TO the nEXT TriaL ..");

        }// end if (r_justOverTrial)
    }//endwhile

    /* release resources */
    if (m_ofile.is_open())
    {
        m_ofile.close();
    }
    m_feedbackSocket->disconnectFromHost();
    delete m_feedbackSocket;
    //m_feedbackSocket->waitForDisconnected(3000);

    if (ofsam.is_open())
    {
        ofsam.close();
    }
    delete   tmpMsq;
    delete   pps;
    delete[] m_rawData;
    delete[] m_labelList;
    delete[] m_eclatency;
}

