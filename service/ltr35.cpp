#include "ltr35.h"
#include <cmath>
#include <algorithm>

#include <QThread>

using namespace std;

Ltr35::Ltr35()
  : internalBufferReloadCount(1)
{
    LTR35_Init(&ltr35);
    timer.setInterval(sendSystemTimeoutMs);
    connect(&timer, SIGNAL(timeout()), this, SLOT(dataSender()));
}

Ltr35::~Ltr35() {

}

bool Ltr35::open(const QString &crateSerialNumber, const int slotNumber) {
    INT err;

    err = LTR35_Open(&ltr35, LTRD_ADDR_DEFAULT, LTRD_PORT_DEFAULT, crateSerialNumber.toStdString().c_str(), slotNumber);
    if (err != LTR_OK)
    {
        qCritical("Failed to establish communication with the module. Error %d (%s)", err, LTR35_GetErrorString(err));
        return false;
    }
    if (LTR35_MOD_1 != ltr35.ModuleInfo.Modification) {
        qCritical("Unsupported module modification %d.", ltr35.ModuleInfo.Modification);
        return false;
    }
    return true;
}

bool Ltr35::isReady() {
    INT err;

    err = LTR35_IsOpened(&ltr35);
    if (err != LTR_OK)
    {
        return false;
    }
    return true;
}

bool Ltr35::close() {
    INT err;

    if (!setupOutputsOff())
    {
        qCritical("Failed to set zeros on all module outputs");
        return false;
    }
    err = LTR35_Close(&ltr35);
    if (err != LTR_OK)
    {
        qCritical("Failed to complete work with the module. Error %d (%s)", err, LTR35_GetErrorString(err));
        return false;
    }
    return true;
}

bool Ltr35::setupRotorChannel(int channel) {
    INT err;

    for (uint i = 0; i < ltr35.ModuleInfo.DacChCnt; i++)
    {
        ltr35.Cfg.Ch[i].Enabled = (BOOLEAN)false;
    }
    sigs.clear();

    Ltr35::Signal sig;
    sig.channel = channel;
    sig.shape = Ltr35::SignalShape::SIGNAL_SHAPE_ROTOR;
    sig.amplitude = 0;
    sig.frequency = 0;
    sig.offset = 0;
    sigs.append(sig);

    TLTR35_CHANNEL_CONFIG cfgCh;
    cfgCh.Enabled = (BOOLEAN)true;
    cfgCh.Source = LTR35_CH_SRC_SDRAM;
    cfgCh.Output = LTR35_DAC_OUT_FULL_RANGE;
    ltr35.Cfg.Ch[channel] = cfgCh;

    ltr35.Cfg.OutMode = LTR35_OUT_MODE_CYCLE;
    ltr35.Cfg.OutDataFmt = LTR35_OUTDATA_FORMAT_20;
    LTR35_FillOutFreq(&ltr35.Cfg, LTR35_DAC_QUAD_RATE_FREQ_STD, NULL);
    err = LTR35_Configure(&ltr35);
    if (err != LTR_OK)
    {
        qCritical("Failed to set DAC settings: Error %d (%s)", err, LTR35_GetErrorString(err));
        return false;
    }
    return true;
}

bool Ltr35::setupRotorSignal(double amplitude, double frequency) {
    if (Ltr35::SignalShape::SIGNAL_SHAPE_ROTOR == sigs.last().shape) {
        sigs.last().amplitude = amplitude;
        sigs.last().frequency = frequency;
        sigs.last().offset = 0;
        timeoutMs = generateSamples(true);
        if (0 == timeoutMs)
        {
            return false;
        }
        return true;
    }
    return false;
}

bool Ltr35::setupOutputsOff()
{
    INT err;

    sigs.clear();
    for (uint i = 0; i < ltr35.ModuleInfo.DacChCnt; i++)
    {
        ltr35.Cfg.Ch[i].Enabled = (BOOLEAN)false;
    }
    ltr35.Cfg.OutMode = LTR35_OUT_MODE_CYCLE;
    ltr35.Cfg.OutDataFmt = LTR35_OUTDATA_FORMAT_20;
    LTR35_FillOutFreq(&ltr35.Cfg, LTR35_DAC_SINGLE_RATE_FREQ_STD, NULL);
    err = LTR35_Configure(&ltr35);
    if (err != LTR_OK)
    {
        qCritical("Failed to set DAC settings: Error %d (%s)", err, LTR35_GetErrorString(err));
        return false;
    }
    return true;
}

bool Ltr35::generateSamples(bool cycle) {
    INT err;
    int samplesPerChannelMax, samplesPerChannelTotal, samplesPerChannelOnReload;
    QVector<double> samplesPerPeriodCount;
    QVector<int> samplesTotalCount, signalPeriods, samplesIdx;
    volatile bool allSignalsCanFit, signalBufferOverflow;

    timeoutMs = 0;
    samplesPerPeriodCount.resize(sigs.size());
    samplesPerPeriodCount.fill(0);
    samplesTotalCount.resize(sigs.size());
    samplesTotalCount.fill(0);
    signalPeriods.resize(sigs.size());
    signalPeriods.fill(1);
    if (cycle) {
        samplesPerChannelMax = samplesCycleTotal / sigs.size();
    } else {
        samplesPerChannelMax = samplesLoadedTotal / sigs.size();
    }

    //NOTE: Try to find number of periods of each signal to make an integer number of points
    for (int i = 0; i < sigs.size(); i++) {
        if (sigs[i].shape != SIGNAL_SHAPE_CONST) {
            if ((sigs[i].frequency > 0) && ((2.0 * sigs[i].frequency) < ltr35.State.OutFreq)) {
                samplesPerPeriodCount[i] = ltr35.State.OutFreq / sigs[i].frequency;
                while ((round(samplesPerPeriodCount[i] * (double)signalPeriods[i]) != (samplesPerPeriodCount[i] * (double)signalPeriods[i])) &&
                       (((int)round(samplesPerPeriodCount[i] * (double)(signalPeriods[i] + 1))) < samplesPerChannelMax)) {
                    signalPeriods[i]++;
                }
                samplesTotalCount[i] = (int)round(samplesPerPeriodCount[i] * (double)signalPeriods[i]);
            } else {
                qCritical("Insufficient DAC frequency %.3f kHz per channel for signal %.3f kHz", ltr35.State.OutFreq / 1000.0, sigs[i].frequency / 1000.0);
                return 0;
            }
        } else {
            samplesPerPeriodCount[i] = 2;
            samplesTotalCount[i] = 2;
        }
    }

    //NOTE: Try to fit all signals in the buffer to be multiples to the largest one
    samplesIdx = sortedIdx(samplesTotalCount);
    allSignalsCanFit = false;
    signalBufferOverflow = false;
    samplesPerChannelTotal = samplesTotalCount[samplesIdx[0]];
    while (!allSignalsCanFit && !signalBufferOverflow) {
        allSignalsCanFit = true;
        for (int i = 1; i < sigs.size(); i++) {
            if ((samplesPerChannelTotal % samplesTotalCount[samplesIdx[i]]) != 0) {
                allSignalsCanFit = false;
                break;
            }
        }
        if (!allSignalsCanFit) {
            if ((samplesPerChannelTotal * 2) < samplesPerChannelMax) {
                samplesPerChannelTotal *= 2;
            } else {
                signalBufferOverflow = true;
            }
        }
    }

    if (!allSignalsCanFit) {
        //NOTE: Set signal buffer size by the common multiplicator
        samplesPerChannelTotal = lcm(samplesTotalCount, samplesPerChannelMax);
    }

    if (cycle) {
        internalBufferReloadCount = 1;
        samplesPerChannelOnReload = samplesPerChannelTotal;
    } else {
        //NOTE: Adjust signal buffer size to the number of equal parts
        internalBufferReloadCount = (int)ceil((double)(samplesPerChannelTotal * sigs.size()) / (double)samplesCycleTotal);
        while ((samplesPerChannelTotal % internalBufferReloadCount) != 0) {
            internalBufferReloadCount++;
        }
        samplesPerChannelOnReload = samplesPerChannelTotal / internalBufferReloadCount;
        samplesPerChannelTotal = samplesPerChannelOnReload * internalBufferReloadCount;
    }

    //NOTE: Get resulting parameters for each signal
    for (int i = 0; i < sigs.size(); i++) {
        if (signalPeriods[i] < (int)floor((double)samplesPerChannelTotal / samplesPerPeriodCount[i])) {
            signalPeriods[i] = (int)floor((double)samplesPerChannelTotal / samplesPerPeriodCount[i]);
        }
        samplesPerPeriodCount[i] = (double)samplesPerChannelTotal / (double)signalPeriods[i];
        samplesTotalCount[i] = (int)samplesPerPeriodCount[i] * signalPeriods[i];
        sigs[i].frequency = ltr35.State.OutFreq / samplesPerPeriodCount[i];
    }

    //NOTE: Generate samples themselves
    for (int i = 0; i < sigs.size(); i++) {
        sigs[i].samples.resize(samplesPerChannelTotal);
        sigs[i].samples.fill(0);
        switch (sigs[i].shape) {
            case SIGNAL_SHAPE_SINUS:
                for (int k = 0; k < samplesPerChannelTotal; k++) {
                    sigs[i].samples[k] = sigs[i].amplitude * sin(2 * M_PI * (double)k / samplesPerPeriodCount[i]) + sigs[i].offset;
                }
                break;
            case SIGNAL_SHAPE_TRIANGLE:
                for (int k = 0; k < samplesPerChannelTotal; k += (int)round(samplesPerPeriodCount[i])) {
                    for (int n = 0; n < (int)round(samplesPerPeriodCount[i] / 2) && k + n < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n] = 2 * sigs[i].amplitude * (double)n / samplesPerPeriodCount[i] + sigs[i].offset;
                    }
                    for (int n = (int)round(samplesPerPeriodCount[i] / 2); n < (int)round(samplesPerPeriodCount[i]) && k + n < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n] = 2 * sigs[i].amplitude * (1 - (double)n / samplesPerPeriodCount[i]) + sigs[i].offset;
                    }
                }
                break;
            case SIGNAL_SHAPE_SAW:
                for (int k = 0; k < samplesPerChannelTotal; k += (int)round(samplesPerPeriodCount[i])) {
                    for (int n = 0; n < (int)round(samplesPerPeriodCount[i]) && k + n < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n] = sigs[i].amplitude * (double)n / samplesPerPeriodCount[i] + sigs[i].offset;
                    }
                }
                break;
            case SIGNAL_SHAPE_IMPULSE:
                for (int k = 0; k < samplesPerChannelTotal; k += (int)round(samplesPerPeriodCount[i])) {
                    for (int n = 0; n < (int)round(samplesPerPeriodCount[i] / 2) && k + n < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n] = sigs[i].amplitude + sigs[i].offset;
                    }
                    for (int n = (int)round(samplesPerPeriodCount[i] / 2); n < samplesPerPeriodCount[i] && k + n < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n] = sigs[i].offset;
                    }
                }
                break;
            case SIGNAL_SHAPE_CONST:
                for (int k = 0; k < samplesPerChannelTotal; k++) {
                    sigs[i].samples[k] = sigs[i].offset;
                }
                break;
            case SIGNAL_SHAPE_ROTOR:
                //TODO: Make rotor signal
                QVector<double> coil;
                coil.resize((int)round(samplesPerPeriodCount[i] / 40));
                for (int n = 0; n < coil.size(); n++) {
                    coil[n] = n * n * sigs[i].amplitude / (double)(coil.size() * coil.size());
                }
                for (int k = 0; k < samplesPerChannelTotal; k += (int)round(samplesPerPeriodCount[i])) {
                    int roundSamples = (int)round(samplesPerPeriodCount[i]);
                    int offsetLow1 = roundSamples / 8 - coil.size();
                    int offsetHigh2 = roundSamples / 4 - coil.size();
                    int offsetLow2 = roundSamples / 4;
                    int offsetHigh3 = 2 * roundSamples / 4 - coil.size();
                    int offsetLow3 = 2 * roundSamples / 4;
                    int offsetHigh4 = 3 * roundSamples / 4 - coil.size();
                    int offsetLow4 = 3 * roundSamples / 4;
                    int offsetHigh1 = roundSamples - coil.size();
                    for (int n = 0; n < coil.size() && k + n + offsetLow1 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetLow1] = coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetHigh2 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetHigh2] = -coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetLow2 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetLow2] = coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetHigh3 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetHigh3] = -coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetLow3 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetLow3] = coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetHigh4 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetHigh4] = -coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetLow4 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetLow4] = coil[n];
                    }
                    for (int n = 0; n < coil.size() && k + n + offsetHigh1 < samplesPerChannelTotal; n++) {
                        sigs[i].samples[k + n + offsetHigh1] = -coil[n];
                    }
                }
                break;
        }
    }
    dataFrameSize = ltr35.State.SDRAMChCnt * samplesPerChannelOnReload;
    timeoutMs = (int)ceil(1000.0 * (double)samplesPerChannelOnReload / ltr35.State.OutFreq);
    mixed.resize(dataFrameSize);
    mixed.fill(0);
    internalBufferReloadIdx = 0;
    for (int n = 0; n < samplesPerChannelOnReload; n++) {
        for (int i = 0; i < ltr35.State.SDRAMChCnt; i++) {
            mixed[n * ltr35.State.SDRAMChCnt + i] = sigs[i].samples[n];
        }
    }
    words.resize(dataFrameSize);
    words.fill(0);
    err = LTR35_PrepareDacData(&ltr35, mixed.data(), dataFrameSize, LTR35_PREP_FLAGS_VOLT, words.data(), NULL);
    if (err != LTR_OK) {
        qCritical("Data generation error %d (%s)", err, LTR35_GetErrorString(err));
        return 0;
    }
    dataSentSize = LTR35_Send(&ltr35, words.data(), dataFrameSize, sendSystemTimeoutMs);
    if (dataSentSize != (INT)dataFrameSize) {
        if (dataSentSize < 0) {
            qCritical("Error sending data to the module!");
        } else {
            qCritical("Error! The number of samples sent is not equal to the specified number!");
        }
        return 0;
    }
    return timeoutMs;
}

bool Ltr35::start()
{
    INT err;

    if (cancel.available() != 0) {
        qWarning("Module in the process of stopping");
        while (isBusy()) {
            QThread::yieldCurrentThread();
        }
    }
    if (!isBusy()) {
        if (LTR35_OUT_MODE_STREAM == ltr35.Cfg.OutMode) {
            err = LTR35_StreamStart(&ltr35, 0);
            if (err != LTR_OK) {
                qCritical("Failed to start streaming data generation! Error %d (%s)", err, LTR35_GetErrorString(err));
                return false;
            }
            busy.release();
            timer.setInterval(timeoutMs / 2);
            internalBufferPartIdx = 0;
            ringStart = true;
            QMetaObject::invokeMethod(&timer, "start", Qt::QueuedConnection);
            while (!timer.isActive()) {
                QThread::yieldCurrentThread();
            }
        } else {
            err = LTR35_SwitchCyclePage(&ltr35, 0, sendSystemTimeoutMs);
            if (err != LTR_OK) {
                qCritical("Failed to start generating data in circular mode! Error %d (%s)", err, LTR35_GetErrorString(err));
                return false;
            }
            busy.release();
        }
        return true;
    } else {
        if (LTR35_OUT_MODE_CYCLE == ltr35.Cfg.OutMode) {
            err = LTR35_SwitchCyclePage(&ltr35, 0, sendSystemTimeoutMs);
            if (err != LTR_OK) {
                qCritical("Failed to start generating data in circular mode! Error %d (%s)", err, LTR35_GetErrorString(err));
                return false;
            }
            return true;
        }
        return false;
    }
}

bool Ltr35::isBusy() {
    return (busy.available() != 0);
}

bool Ltr35::stop() {
    INT err;

    if (isBusy()) {
        if (cancel.available() != 0) {
            qWarning("Module in the process of stopping");
            return false;
        }
        cancel.release();
        if (timer.isActive()) {
            QMetaObject::invokeMethod(&timer, "stop", Qt::QueuedConnection);
            while (timer.isActive()) {
                QThread::yieldCurrentThread();
            }
        }
        if (ltr35.State.Run) {
            err = LTR35_Stop(&ltr35, 0);
            if (err != LTR_OK) {
                qCritical("Generation stopped with error %d (%s)", err, LTR35_GetErrorString(err));
                cancel.acquire();
                busy.acquire();
                return false;
            }
            cancel.acquire();
            busy.acquire();
            return true;
        }
    }
    qWarning("The module has not been started");
    return false;
}

template<typename T>
QVector<int> Ltr35::sortedIdx(const QVector<T> &data) {
    QVector<int> idx(data.size());
    int x = 0;

    iota(idx.begin(), idx.end(), x++);
    sort(idx.begin(), idx.end(), [&](int i, int j){
        return (data[i] > data[j]);
    });
    return idx;
}

int Ltr35::lcm(QVector<int> numbers, int max) {
    int i;
    bool found;

    int num = *std::max_element(numbers.constBegin(), numbers.constEnd());
    while (num < max) {
        found = true;
        for (i = 0; i < numbers.size(); i++) {
            if (numbers[i] != 0) {
                found &= ((num % numbers[i]) == 0);
            }
        }
        if (found) {
            return num;
        }
        num++;
    }
    return max;
}

void Ltr35::dataSender()
{
    int i, n, nextReloadIdx = 0, samplesPerChannelOnReload, offset;
    DWORD *partToSend;

    if ((internalBufferPartIdx % 2) == 0) {
        partToSend = words.data() + dataFrameSize / 2;
    } else {
        partToSend = words.data();
    }
    if (!ringStart) {
        dataSentSize = LTR35_Send(&ltr35, partToSend, dataFrameSize / 2, sendSystemTimeoutMs);
        if (dataSentSize != (INT)(dataFrameSize / 2)) {
            if (dataSentSize < 0) {
                qCritical("Error sending data to the module!");
            } else {
                qCritical("Error! The number of samples sent is not equal to the specified number!");
            }
        }
    } else {
        ringStart = false;
    }
    samplesPerChannelOnReload = dataFrameSize / ltr35.State.SDRAMChCnt;
    if ((internalBufferPartIdx % 2) == 0) {
        if (internalBufferReloadIdx < (internalBufferReloadCount - 1)) {
            nextReloadIdx = internalBufferReloadIdx + 1;
        } else {
            nextReloadIdx = 0;
        }
        offset = nextReloadIdx * samplesPerChannelOnReload;
        for (n = 0; n < (samplesPerChannelOnReload / 2); n++) {
            for (i = 0; i < ltr35.State.SDRAMChCnt; i++) {
                mixed[n * ltr35.State.SDRAMChCnt + i] = sigs[i].samples[offset + n];
            }
        }
        LTR35_PrepareDacData(&ltr35, mixed.data(), dataFrameSize / 2, LTR35_PREP_FLAGS_VOLT, words.data(), NULL);
        internalBufferReloadIdx = nextReloadIdx;
    }
    else {
        offset = internalBufferReloadIdx * samplesPerChannelOnReload;
        for (n = (samplesPerChannelOnReload / 2); n < samplesPerChannelOnReload; n++) {
            for (i = 0; i < ltr35.State.SDRAMChCnt; i++) {
                mixed[n * ltr35.State.SDRAMChCnt + i] = sigs[i].samples[offset + n];
            }
        }
        LTR35_PrepareDacData(&ltr35, mixed.data() + dataFrameSize / 2, dataFrameSize / 2, LTR35_PREP_FLAGS_VOLT, words.data() + dataFrameSize / 2, NULL);
    }
    if (internalBufferPartIdx < ((internalBufferReloadCount * 2) - 1)) {
        internalBufferPartIdx++;
    } else {
        internalBufferPartIdx = 0;
    }
}
