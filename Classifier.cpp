#include "Classifier.h"
#include <iostream>

Classifier::Classifier(): model(0)
{}

void Classifier::setParam()
{
    param.svm_type = C_SVC;
    param.kernel_type = LINEAR;
    param.degree = 3;
    param.gamma = 0;
    param.coef0 = 0;
    param.nu = 0.5;
    param.cache_size = 100;
    param.eps = 1e-3;
    param.p = 0.1;
    param.shrinking = 1;
    param.probability = 0;
    param.nr_weight = 0;  // NULL
    param.C = 0.031250;
    param.weight = 0;    // NULL  
}

void Classifier::train(int samplesize, int featuredim, double *classtag, double *data)
{    
    // data must first be transfered to standard style for libsvm, i.e. 1:value1 2:value2 ..
    struct svm_node* stdData = new svm_node[samplesize * (featuredim+1)];  // 1 for end flag "-1"
    int i, j, index;
    int cur = 0;
    for (i=0; i<samplesize; ++i)
    {
        index = 0;
        for (j=0; j<featuredim; ++j)
        {
            cur = i*featuredim + j;
            stdData[cur].index = ++index;
            stdData[cur].value = data[cur];
        }
        stdData[cur+1].index = -1;   // -1 indicates the end of a sample feature, DON'T MISS IT!!!
    }

    struct svm_problem myprob;
    myprob.l = samplesize;
    myprob.y = classtag;
    myprob.x = new svm_node*[samplesize];
    for (int i=0; i<samplesize; ++i)
        myprob.x[i] = &stdData[i*featuredim];

    // check parameter first, here because we need problem
    const char *error_msg;
    error_msg = svm_check_parameter(&myprob, &param);
    if (error_msg)
    {
        std::cerr << "ERROR: " << error_msg << "\n";
        return;
    }
    else
        std::cout << "check over!\n";

    // training
    model = svm_train(&myprob, &param);
    myprob.x = 0;
    myprob.y = 0;
    delete[] stdData;
    svm_destroy_param(&param);
}

void Classifier::test(int testsize, int featuredim, double *classtag, double *data, double* predict_tag)
{
    // this code epoch shows how to usgin function svm_predict
    if (classtag==0)
        classtag = 0;
//    struct svm_node* testdata = new svm_node[testsize * (featuredim+1)];
//    int i, j, index;
//    for(i=0; i<testsize; ++i)
//    {
//        // feature data must be first be transfered to standard style for libsvm
//        index = 0;
//        for (j=0; j<featuredim; ++j)
//        {
//            testdata[j].index = ++index;
//            testdata[j].value = data[i*featuredim+j];
//        }
//        testdata[featuredim].index = -1;
//        predict_tag[i] = svm_predict(model, testdata);
//    }

    // calculate geometric margin for test samples
    double geo_margin = 0;
    int i = 0, j = 0, k = 0;
    for (i=0; i<testsize; ++i)
    {
        geo_margin = 0;
        for (j=0; j<(*model->nSV); ++j)
        {
            double tmp = 0;
            for (k=0; k<featuredim; ++k)
            {
                tmp += data[i*featuredim + k] * model->SV[j]->value;
                tmp *= model->sv_coef[j][1];
            }
            geo_margin += tmp;
        }
        predict_tag[i] = geo_margin - *model->rho;

    }
}
