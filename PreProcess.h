#ifndef PREPROCESS_H
#define PREPROCESS_H

class PreProcess
{
public:
    explicit PreProcess(int order);
    ~PreProcess();
    void removeEog(double *sig, int bufferlength, int row, int column, int eogrow);
    void filterPoint(double *sig, int bufferlength, int row, int length, int eogrow);
    void Segmentation(double *sig, int bufferlength,                       // raw data and its length
                      double *result, double *class_tag, int objLabel,    // result after preprocess; class_tag is 1 if a sample is object, otherwise -1
                      int *latency, int &eventnum, int *ecLabel,       // latencyfor the onsets of each event
                      int lenpre200,                                  // sample points' length of transfered from 200ms before onset
                      int lenbeforeonset, int lenafteronset,          // sample points' length of an epoch, using onset as datum
                      int channels, int EOGchannel,                   // number of choose channels, including EOG channel!
                      int downsample);                                // rate for down-sampling
private:
    double *coa;
    double *cob;
    int order;
};
#endif // PREPROCESS_H
