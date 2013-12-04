#include "PreProcess.h"
#include <QDebug>

PreProcess::PreProcess(int order)
    :order(order)
{
    coa = new double[order+1];
    cob = new double[order+1];

    coa[0] = 1;     coa[1] = -1.8294; coa[2] = 0.8299;
    cob[0] = 0.085; cob[1] = 0;       cob[2] = -0.085;
}

PreProcess::~PreProcess()
{
    delete[] coa;
    delete[] cob;
}

// Note that bufferlenght will always be more than the valid length ,i.e. "column" here.
//  ______________________________________
// |#####################|                |
// |#####################|                |
// ````````````````````````````````````````
// |<-valid data length->|                |
// |<-            buffer length         ->|
//
// { #: valid data }
void PreProcess::removeEog(double *sig , int bufferlength, int row, int column, int eogrow)
{
    qDebug() << "into removeEog\n";
    int i, j;
    double meanEOG = 0;
    for (i=0; i<column; ++i)
        meanEOG += sig[bufferlength*(eogrow-1)+i];
    meanEOG /= (column * 1.0);
    double temp = 0, imd;
    for (i=0; i<column; ++i)
    {
        imd = sig[bufferlength*(eogrow-1)+i] - meanEOG; // EOG not changed
        temp += imd * imd;
    }

    /* for every nonEog channel do */
    for (i=0; i<row; ++i)
    {
        if (i==eogrow-1)
            continue;
        double meanEEG = 0.0;
        for (j=0; j<column; ++j)
            meanEEG += sig[bufferlength*i+j];
        meanEEG /= (column * 1.0);
        double b = 0.0;
        /* remove DC */
        for (j=0; j<column; ++j)
        {
            imd = sig[bufferlength*i+j] - meanEEG;  // EEG not changed
            b += imd * (sig[bufferlength*(eogrow-1)+j] - meanEOG);
        }

        b /= temp;
        for (j=0; j<column; ++j) // remove EOG: eeg := eeg - b*eog - ( meanEOG - b*meanEEG )
        {
            sig[bufferlength*i+j] -= b * sig[bufferlength*(eogrow-1)+j] + (meanEOG - b*meanEEG);
        }
    }
    qDebug() << "out removeEog\n";
}

// bufferlength is the length of the buffer - sig, length is the valid length of data
void PreProcess::filterPoint(double *sig, int bufferlength, int row, int length, int eogrow)
{
    qDebug() << "in filterPoint\n";
    /* this is bandpass filtering */
    int i, j;
    if (NULL != coa && NULL != cob)
    {
        /* store original signal data for filtering*/
        double *originsig = new double[order];
        /* do filtering for every EEG channel */
        for (j=0; j<row; ++j)
        {
            if (j==eogrow-1)
                continue;
            /* store the first sample point, all other points should subtract it */
            double sig0 = *(sig+j*bufferlength);
            /* filter in forward direction */
            for (i=0; i<length; ++i)
            {
                *(sig+j*bufferlength+i) -= sig0;
                originsig[i%order] = *(sig+j*bufferlength+i);
                *(sig+j*bufferlength+i) = cob[0] * (*(sig+j*bufferlength+i));
                if (i-1>=0)
                    *(sig+j*bufferlength+i) += cob[1] * originsig[(i-1)%order] - coa[1] * (*(sig+j*bufferlength+i-1));
                if (i-2>=0)
                    *(sig+j*bufferlength+i) += cob[2] * originsig[(i-2)%order] - coa[2] * (*(sig+j*bufferlength+i-2));
            }

            /* filter in backward direction */
            sig0 = *(sig+j*bufferlength+length-1);
            for (i=length-1; i>=0; --i)
            {
                *(sig+j*bufferlength+i) -= sig0;
                originsig[i%order] = *(sig+j*bufferlength+i);
                *(sig+j*bufferlength+i) = cob[0] * (*(sig+j*bufferlength+i));
                if (i+1<length)
                    *(sig+j*bufferlength+i) += cob[1] * originsig[(i+1)%order] - coa[1] * (*(sig+j*bufferlength+i+1));
                if (i+2<length)
                    *(sig+j*bufferlength+i) += cob[2] * originsig[(i+2)%order] - coa[2] * (*(sig+j*bufferlength+i+2));
            }
        }
        delete[] originsig;
    }
    qDebug() << "out of filterPoint\n";
}

// Note buffer length is needed here, not valid data length
void PreProcess::Segmentation(double *sig, int bufferlength,      // raw data and its length
                  double *result, double *class_tag, int objLabel,// result after preprocess; class_tag is 1 if a sample is object, otherwise -1
                  int *latency, int& eventnum, int *ecLabel,       // latencyfor the onsets of each event
                  int lenpre200,                                  // sample points' length of transfered from 200ms before onset
                  int lenbeforeonset, int lenafteronset,          // sample points' length of an epoch, using onset as datum
                  int channels, int EOGchannel,                   // number of choose channels, including EOG channel!
                  int downsample)                                 // rate for down-sampling
{
    qDebug() << "in Segmentation\n";
    int featurelen = (lenbeforeonset + lenafteronset) * (channels-1);  //length or dimension of features of a sample
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
    int ecnum = 0;              // event number without mouse click (250)
    for (i=0; i<eventnum; ++i)  // for every event
    {
        if (ecLabel[i]==250)   // 250 for mouse click
            continue;
        ecnum++;
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
            if (ch==EOGchannel-1)
                continue;
            // 200ms before onset, used for correct baseline of an epoch
            meanofpre200 = 0;
            for (j=onset-lenpre200; j<onset && j>=0; ++j)
                meanofpre200 += *(sig+ch*bufferlength+j-1);
            meanofpre200 /= lenpre200;

            // downsampling
            for (j=onset-lenbeforeonset; j<onset+lenafteronset && j>=0; j += downsample)
            {
                *(result+sample*featurelen+offset) = (*(sig+ch*bufferlength+j-1)) - meanofpre200; // correct baseline
                ++offset;
            }
        }
        ++sample;
    }

    eventnum = ecnum;
    qDebug() << "real event number" << QString::number(eventnum) << "\n"
                <<"out of Segmentation\n";
}
