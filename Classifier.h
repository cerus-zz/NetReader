#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#include "svm.h"

class Classifier
{        
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
    double AUCofROC(double *score, double *classtag, int size, double& ap);

private:
    struct svm_parameter param;
    struct svm_model *model;

    struct Obj
    {
        double score;
        double label;
    };
    void quick_sort(Obj *tmp, int low, int high);
};

#endif // CLASSIFIER_H
