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
        QSpinBox *shift;
        QLineEdit *value;
    };

    static constexpr int timerPortSendPeriodMs = 100;
    static constexpr int timerPortAutoReadPeriodMs = 20;
    static constexpr int timerPortReplyTimeoutMs = 500;

private:
    void closeEvent(QCloseEvent* e);
    TimingUi createTimingUi(QGridLayout *layout, QString name, int row);
    void lockTimings(bool lock);

private slots:
    void setPort(const QString &portname);
    void setGenerator(const QString &generator);
    void calcValue(int row);
    void portRead();
    void portSend();
    void portReplyTimeout();
    void on_pushButtonUpdate_released();
    void on_pushButtonGenerate_released();

private:
    Ui::MainWindow *ui;
    QSignalMapper *signalMapperPort;
    QSignalMapper *signalMapperGenerator;
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
    QScopedPointer<Ltr35> ltr35;

};

#endif // MAINWINDOW_H
