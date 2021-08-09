#ifndef LTR35_H
#define LTR35_H

#include "ltr/include/ltr35api.h"

#include <QObject>
#include <QSemaphore>
#include <QMutex>
#include <QQueue>
#include <QVector>
#include <QList>
#include <QTimer>

class Ltr35 : public QObject
{
    Q_OBJECT

public:
    enum SignalShape {
        SIGNAL_SHAPE_SINUS,
        SIGNAL_SHAPE_TRIANGLE,
        SIGNAL_SHAPE_SAW,
        SIGNAL_SHAPE_IMPULSE,
        SIGNAL_SHAPE_CONST,
        SIGNAL_SHAPE_ROTOR
    };

    static constexpr int sendSystemTimeoutMs = 20000;

    static constexpr int samplesCycleTotal = LTR35_MAX_POINTS_PER_PAGE;
    static constexpr int samplesLoadedTotal = (128 * 1024 * 1024);

    struct Signal {
        uint channel;
        SignalShape shape;
        double amplitude;
        double frequency;
        double offset;
        QVector<double> samples;
    };

public:
    Ltr35();
    ~Ltr35();

public:
    bool open(const QString &crateSerialNumber, const int slotNumber);
    bool isReady();
    bool close();
    bool setupRotorChannel(int channel);
    bool setupRotorSignal(double amplitude, double frequency);
    bool setupOutputsOff();
    bool generateSamples(bool cycle);
    bool isBusy();

    static int lcm(QVector<int> numbers, int max = INT_MAX);
template<typename T>
    static QVector<int> sortedIdx(const QVector<T> &data);

public slots:
    bool start();
    bool stop();

private slots:
    void dataSender();

private:
    TLTR35 ltr35;
    QList<Signal> sigs;
    QSemaphore busy;
    QSemaphore cancel;
    DWORD dataFrameSize;
    INT dataSentSize;
    QVector<DWORD> words;
    QVector<double> mixed;
    QTimer timer;
    int timeoutMs;
    int internalBufferReloadCount;
    int internalBufferReloadIdx;
    int internalBufferPartIdx;
    bool ringStart;

};

#endif // LTR35_H
