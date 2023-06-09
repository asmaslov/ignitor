#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QActionGroup>
#include <QSignalMapper>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QSerialPort>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include "ltr/include/ltrapi.h"
#include "ltr35.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    struct TimingUi {
        QLabel *index;
        QSpinBox *rpm;
        QSpinBox *timing;
        QLineEdit *value;
    };

    struct TimingRecord {
        int rpm;
        int timing;
    };

    static constexpr char timingsFileExtension[] = "tim";

    static constexpr int rotorSignalChannel = 0;
    static constexpr double rotorSignalAmplitude = 2.0;

    static constexpr int timerPortSendPeriodMs = 50;
    static constexpr int timerPortAutoReadPeriodMs = 10;
    static constexpr int timerPortReplyTimeoutMs = 500;

private:
    void closeEvent(QCloseEvent* e);
    TimingUi createTimingUi(QGridLayout *layout, QString name, int row);
    void lockShift(bool lock);
    void lockTimings(bool lock);
    bool loadTimingsFile(QString fileName);
    bool saveTimingsFile(QString fileName);

private slots:
    void setPort(const QString &portname);
    void setGeneratorLtr35(const QString &generator);
    void calcValue(int row);
    void calcAllValues();
    void portRead();
    void portSend();
    void portReplyTimeout();
    void on_pushButtonShiftSet_released();
    void on_pushButtonUpdate_released();
    void on_pushButtonGenerate_released();
    void on_pushButtonStop_released();
    void on_checkBoxShiftAutoset_toggled(bool checked);
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered();
    void on_actionWriteMemory_triggered();
    void on_actionExit_triggered();

private:
    Ui::MainWindow *ui;
    QSignalMapper *signalMapperPort;
    QSignalMapper *signalMapperGeneratorLtr35;
    QSignalMapper *signalMapperValue;
    QActionGroup *groupPort;
    QList<TimingUi> timingsUi;
    QSerialPort serial;
    QTimer timerPortSend;
    QTimer timerPortAutoRead;
    QTimer timerPortReply;
    int cmd;
    int recordIdx;
    QMutex mutexRequest;
    QSemaphore semaphoreTransmitComplete;
    QActionGroup *groupGenerator;
    TLTR ltrServer;
    QThread threadLtr35;
    QString timingsFileName;
    QScopedPointer<Ltr35> ltr35;

};

#endif // MAINWINDOW_H
