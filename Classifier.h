#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#include "svm.h"

class Classifier
{
    struct svm_parameter param;
    struct svm_model *model;
public:
    Classifier();
    void setParam();
    void train(int samplesize, int featuredim, double* classtag, double *data);
    void test(int testsize, int featuredim, double* classtag, double *data, double* predict_tag);

};
#endif // CLASSIFIER_H
