#ifndef CALCULATION_H
#define CALCULATION_H

#include <QObject>
#include <fstream>

class DataScanSocket;
class QThread;
class Classifier;
class QTcpSocket;
class QString;

class Calculation : public QObject
{    
    Q_OBJECT
private slots:
    void calc();
    void stoprunning();
    void startsave(const QString&);
    //void changeClassifier();          // change the classifier to use
    void discardData();                 // coersively discard the data of the LATEST run
    void setObj(int objlabel);
    void startTrain();
    void startTest();

signals:
    void Printstatus(const QString& );
    void GetObj();
    void GetPara();

public:
    Calculation(DataScanSocket *tcpsocket, QString& fip, ushort fport, int max_run, QObject *parent = 0 );
    ~Calculation();
    bool m_running;

private:
    void sendCmd2user(const char command[]);
    bool           m_saving;
    bool           m_readygo;       // ensure that preprocessing have been done before training or testing
    int            m_objlabel;
    int            m_featuredim;    // feature dimension
    int            m_ecnum;         // event number in each run
    int            m_samplesize;
    int            m_max_run;       // max number of runs used for training
    int            m_haverun;       // "valid" training runs of data

    DataScanSocket *dsocket;
    double         *m_trainData;    // store data of several trials which have been preprocessed, samples * features
    double         *m_classTag;     // class tag for each sample
    Classifier     *m_classifier;   // classifier used
    QTcpSocket     *m_feedbackSocket;  // send result of classification to users
    QString        m_fipadd;
    ushort         m_fport;   
    std::ofstream  m_ofile;          // ofstream for saving file

//    void removeEog_filter();
};
#endif // CALCULATION_H
