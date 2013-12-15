#include "Classifier.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <exception>
#include <QMessageBox>

Classifier::Classifier(): model(NULL)
{}

Classifier::~Classifier()
{
    svm_destroy_param(&param);
    svm_free_and_destroy_model(&model);
}

void Classifier::setParam()
{
    param.svm_type = C_SVC;
    param.kernel_type = LINEAR;
//    param.kernel_type = RBF;
    param.degree = 3;
    param.gamma = 0;
//    param.gamma = 1e-004;  // rbf
    param.coef0 = 0;
    param.nu = 0.5;
    param.cache_size = 100;
    param.eps = 1e-3;
    param.p = 0.1;
    param.shrinking = 1;
    param.probability = 0;
    param.nr_weight = 0;  // NULL
    param.C = 0.031250;
//    param.C = 16;  // rbf
    param.weight = 0;    // NULL  
}

int Classifier::save_model(const char *model_file_name)
{
    model_file_name = NULL;
    return 0;
}

void Classifier::load_model(const char *model_file_name)
{
    model = svm_load_model(model_file_name);
}

void Classifier::train(int samplesize, int featuredim, double *classtag, double *data)
{    
    // data must first be transfered to standard style for libsvm, i.e. 1:value1 2:value2 ..
    qDebug() << "featuredim:" << featuredim << "  samplesize: " << samplesize << "\n";  
    struct svm_node* stdData  = new svm_node[samplesize * (featuredim+1)];  // 1 for end flag "-1"
    int i, j, index;
    int cur = 0;
    for (i=0; i<samplesize; ++i)
    {
        index = 0;
        for (j=0; j<featuredim; ++j)
        {
            cur = i*(featuredim+1) + j;
            stdData[cur].index = ++index;
            stdData[cur].value = data[i*featuredim + j];
        }
        stdData[cur+1].index = -1;   // -1 indicates the end of a sample feature, DON'T MISS IT!!!
    }

    struct svm_problem myprob;
    myprob.l = samplesize;
    myprob.y = classtag;
    myprob.x = new svm_node*[samplesize];
    for (int i=0; i<samplesize; ++i)
        myprob.x[i] = &stdData[i*(featuredim+1)];

    // check parameter first, here because we need problem
    const char *error_msg;
    error_msg = svm_check_parameter(&myprob, &param);
    if (error_msg)
    {
        qDebug() << "ERROR: " << error_msg << "\n";
        return;
    }
    else
        qDebug() << "check over!\n";

    // training
    if (NULL!=model)
    {
        svm_free_and_destroy_model(&model);   // free the former model in case for mem leak
    }
    model = svm_train(&myprob, &param);
    qDebug() << "svm_train just over\n";
    // save model
//    if (-1 == svm_save_model("F:\\model", model))
//        qDebug() << "sth. wrong with saving model\n";

    // this pointers point to non-local object, delete them for safety
    myprob.x = NULL;
    myprob.y = NULL;
    // we don't want to save all train data except the Support vectors
    // but we should transfer the svs to a new mem before we delete the original train data
    svm_node **tmp = model->SV;
    //model->SV = new svm_node*[model->l];
    for (i=0; i<model->l; ++i)
    {
        model->SV[i] = new svm_node[featuredim+1];
        for (j=0; j<featuredim; ++j)
        {
            model->SV[i][j].index = j+1;
            model->SV[i][j].value = (*(tmp + i) + j)->value;
        }
    }
    qDebug() << "just transfer svs\n";
    tmp = NULL;  // don't let "tmp" be a dangling pointer
    delete[] stdData;
//    svm_destroy_param(&param);
}

double Classifier::test(int testsize, int featuredim, double *classtag, double *data, double* predict_tag)
{
    if (NULL == model)
    {
        return -1;
    }
    if (classtag==0)
        classtag = 0;
    if (NULL == predict_tag)
    {
        qDebug() << "predict_tag can't be NULL!\n";
        return -1;
    }
    // this code epoch shows how to using function svm_predict
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
    double *sv_coef = model->sv_coef[0];
    for (i=0; i<testsize; ++i)
    {
        geo_margin = 0;
        for (j=0; j<model->l; ++j)   // model->l is the total number of SVs from all classes
        {
            double tmp = 0;
            for (k=0; k<featuredim; ++k)
            {
                tmp += *(data+i*featuredim + k) * (*(model->SV + j) + k)->value;
//                double d = *(data+i*featuredim + k) - (*(model->SV + j) + k)->value; // rbf
//                tmp += d * d;  // rbf
            }
//            tmp = exp(-param.gamma * tmp);  // rbf
            geo_margin += (tmp * sv_coef[j]);
        }
        predict_tag[i] = geo_margin - *model->rho;
        // Note here!
        // the sequence of support vectors depend on the sequence of labels ([1,-1] or [-1,1]),
        // so if model->label[0] = -1, and the predict_tag[i] > 0,then the sample belongs to -1 class
        // and vice versa. So for convenience, we multiply predict_tag[i] by model->label[0],
        // and then we can just reckon that the bigger the geometric margin, the greater probability that
        // the sample belongs to 1 class!!!
        predict_tag[i] *= model->label[0];
    }

    qDebug() << "test over indeed\n";
    double accuracy = 0.0;
    for ( i=0; i<testsize; ++i)
    {
        if ((predict_tag[i]>0 && classtag[i]>0) || (predict_tag[i]<0 && classtag[i]<0))
            accuracy += 1;
    }
    return accuracy / (testsize * 1.0);
}

double Classifier::dot(double *x1, double *x2, int size)
{
    double res = 0;
    for (int i=0; i<size; ++i)
    {
        res += x1[i] * x2[i];
    }
    return res;
}

double Classifier::rbf(double *x1, double *x2, int size)
{
    double sum = 0, d = 0;
    for (int i=0; i<size; ++i)
    {
        d = x1[i] - x2[i];
        sum += d * d;
    }

    return exp(-param.gamma * sum);
}

void Classifier::quick_sort(Obj *tmp, int low, int high)
{
    int l = low, h = high;
    Obj *lin = new Obj();
    while (l<h)
    {
        while (l <= high && tmp[l].score >= tmp[low].score) ++l;
        while (tmp[h].score < tmp[low].score) --h;
        if (l<h)
        {
            lin->label = tmp[h].label;
            lin->score = tmp[h].score;
            tmp[h].label = tmp[l].label;
            tmp[h].score = tmp[l].score;
            tmp[l].label = lin->label;
            tmp[l].score = lin->score;
        }
    }
    lin->label = tmp[low].label;
    lin->score = tmp[low].score;
    tmp[low].label = tmp[h].label;
    tmp[low].score = tmp[h].score;
    tmp[h].label = lin->label;
    tmp[h].score = lin->score;

    if (low < h)    quick_sort(tmp, low, h-1);
    if (h   < high) quick_sort(tmp, h+1, high);

    delete lin;
}

double Classifier::AUCofROC(double *score, double *classtag, int size)
{        
    Obj    *tmparr= new Obj[size];
    double *roc_x = new double[2+size];
    double *roc_y = new double[2+size];
    double Nnum = 0;   // number of negtive class
    double Pnum = 0;   // number of postive class
    int i = 0;
    for (i=0; i<size; ++i)
    {
        tmparr[i].score = score[i];
        tmparr[i].label = classtag[i];
        if (1==classtag[i])            // for binary classification only
            ++Pnum;
        else
            ++Nnum;
    }

    // SORT score first in descending order!
    quick_sort(tmparr, 0 ,size-1);
    roc_x[0] = roc_y[0] = 0;
    double fp = 0;      // false positive
    double tp = 0;      // true positive
    for (i=1; i<1+size; ++i)
    {
        if (1==tmparr[i-1].label)
            ++tp;
        else
            ++fp;
        roc_x[i] = fp / Nnum;
        roc_y[i] = tp / Pnum;
    }
    roc_x[1+size] = fp / Nnum;
    roc_y[1+size] = tp / Pnum;

    double auc = 0;
    for (i=0; i<1+size; ++i)
    {
        if (roc_x[i+1] != roc_x[i])
        {
            auc += roc_y[i];
        }
    }

    delete[] tmparr;
    delete[] roc_x;
    delete[] roc_y;
    if (0!=Nnum)
    {
        auc /= Nnum;
    }
    else
        auc = 0;
    return auc;
}

