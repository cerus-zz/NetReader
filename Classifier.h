#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#include "svm.h"

class Classifier
{
    struct svm_parameter param;
    struct svm_model *model;
public:
    Classifier();
    ~Classifier();
    void setParam();
    void train(int samplesize, int featuredim, double* classtag, double *data);
    double test(int testsize, int featuredim, double* classtag, double *data, double* predict_tag);
    int save_model(const char *model_file_name);
    void load_model(const char *model_file_name);
    double dot(double *x1, double *x2, int size);
    double rbf(double *x1, double *x2, int size);
};
#endif // CLASSIFIER_H
