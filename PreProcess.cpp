#include "PreProcess.h"
#include <QDebug>

PreProcess::PreProcess(int order)
    :coa(0), cob(0), order(order)
{
    coa = new double[order];
    cob = new double[order];

    coa[0] = 1;     coa[1] = -1.8294; coa[2] = 0.8299;
    cob[3] = 0.085; cob[4] = 0;       cob[5] = -0.085;
}

PreProcess::~PreProcess()
{
    delete[] coa;
    delete[] cob;
}

void PreProcess::removeEog(double *sig, int row, int column, int eogrow)
{
    int i, j;
    double meanEOG = 0;
    for (i=0; i<column; ++i)
        meanEOG += sig[column*(eogrow-1)+i];
    meanEOG /= column * 1.0;
    double temp = 0, imd;
    for (i=0; i<column; ++i)
    {
        imd = sig[column*(eogrow-1)+i] - meanEOG; // EOG not changed
        temp += imd * imd;
    }

    /* for every nonEog channel do */
    for (i=0; i<row; ++i)
    {
        if (i==eogrow-1)
            continue;
        double meanEEG = 0.0;
        for (j=0; j<column; ++j)
            meanEEG += sig[column*i+j];
        meanEEG /= column * 1.0;
        double b = 0.0;
        /* remove DC */
        for (j=0; j<column; ++j)
        {
            imd = sig[column*i+j] - meanEEG;  // EEG not changed
            b += imd * (sig[column*(eogrow-1)+j] - meanEOG);
        }

        b /= temp;
        for (j=0; j<column; ++j)
        {
            sig[column*i+j] -= b * sig[column*(eogrow-1)+j] + (meanEOG - meanEEG);
        }
    }
}
void PreProcess::filterPoint(double *sig, int row, int length, int eogrow)
{
    /* this is bandpass filtering */
    int i, j;
    if (order!=0 && coa!=0 && cob!=0)
    {
        /* store original signal data for filtering*/
        double *originsig = new double[order];
        /* do filtering for every EEG channel */
        for (j=0; j<row; ++j)
        {
            if (j==eogrow-1)
                continue;
            /* store the first sample point, all other points should subtract it */
            double sig0 = *(sig+j*length);
            /* filter in forward direction */
            for (i=0; i<length; ++i)
            {
                *(sig+j*length+i) -= sig0;
                originsig[i%order] = *(sig+j*length+i);
                *(sig+i) = cob[0] * (*(sig+j*length+i));
                if (i-1>=0)
                    *(sig+j*length+i) += cob[1] * originsig[(i-1)%order] - coa[1] * (*(sig+j*length+i-1));
                if (i-2>=0)
                    *(sig+j*length+i) += cob[2] * originsig[(i-2)%order] - coa[2] * (*(sig+j*length+i-2));
            }

            /* filter in backward direction */
            sig0 = *(sig+j*length+length-1);
            for (i=length-1; i>=0; --i)
            {
                *(sig+j*length+i) -= sig0;
                originsig[i%order] = *(sig+j*length+i);
                *(sig+j*length+i) = cob[0] * (*(sig+j*length+i));
                if (i+1<length)
                    *(sig+j*length+i) += cob[1] * originsig[(i+1)%order] - coa[1] * (*(sig+j*length+i+1));
                if (i+2<length)
                    *(sig+j*length+i) += cob[2] * originsig[(i+2)%order] - coa[2] * (*(sig+j*length+i+2));
            }
        }
        delete[] originsig;
    }
}

void PreProcess::Segmentation(double *sig, int siglenth,           // raw data and its length
                  double *result, double *class_tag, int objLabel,    // result after preprocess; class_tag is 1 if a sample is object, otherwise -1
                  int *latency, int eventnum, int *ecLabel,       // latencyfor the onsets of each event
                  int lenpre200,                                  // sample points' length of transfered from 200ms before onset
                  int lenbeforeonset, int lenafteronset,          // sample points' length of an epoch, using onset as datum
                  int channels, int EOGchannel,                   // number of choose channels, including EOG channel!
                  int downsample)                                 // rate for down-sampling
{
    int featurelen = (lenbeforeonset + lenafteronset) * channels;  //length or dimension of features of a sample
    if (featurelen%downsample != 0)   // be sure of feature dimension after downsample
    {
        featurelen /= downsample;
        ++featurelen;
    }
    else
        featurelen /= downsample;

    int sample = 0;     // number of samples processed
    int i, j, ch, onset;
    double meanofpre200;
    qDebug() << eventnum << "..seg start..\n";
    for (i=0; i<eventnum; ++i)  // for every event
    {
        qDebug() << "!!" << i << "!!\n";
        if (ecLabel[i]==250)   // 250 for mouse click
            continue;
        /* attach class tag to each sample, 1 & -1 as default tags for binary classification situation here */
        if (ecLabel[i] ==objLabel)
            class_tag[sample] = 1;
        else
            class_tag[sample] = -1;

        /* processing... */
        onset = latency[i];
        int offset = 0;    // offset : number of sampling points processed for a certain sample
        for (ch=0; ch<channels; ++ch)
        {
            if (ch==EOGchannel)
                continue;
            // 200ms before onset, used for correct baseline of an epoch
            meanofpre200 = 0;
            for (j=onset-lenpre200; j<onset && j>=0; ++j)
                meanofpre200 += *(sig+ch*siglenth+j);
            meanofpre200 /= lenpre200;

            // downsampling
            for (j=onset-lenbeforeonset; j<onset+lenafteronset && j>=0; j += downsample)
            {
                *(result+sample*featurelen+offset) = (*(sig+ch*siglenth+j-1)) - meanofpre200; // correct baseline
                ++offset;
            }
        }
        ++sample;
    }
}
