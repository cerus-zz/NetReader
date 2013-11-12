#TEMPLATE      = app
#QT           += network \
#               widgets
TEMPLATE =  app
QT       += network \
            widgets
#CONFIG  += console
HEADERS += \
    DataScanSocket.h \
    MessageQueue.h \
    AcqMessage.h \
    ScanReader.h \
    Calculation.h \
    PreProcess.h \
    svm.h \
    Classifier.h

SOURCES += \
    DataScanSocket.cpp \
    main.cpp \
    ScanReader.cpp \
    Calculation.cpp \
    PreProcess.cpp \
    svm.cpp \
    Classifier.cpp
