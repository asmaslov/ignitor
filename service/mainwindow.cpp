#include "mainwindow.h"
#include "ui_ignitor.h"
#include "remote.h"
#include "cdi.h"
#include <QAction>
#include <QCloseEvent>
#include <QSerialPortInfo>
#include <QtEndian>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , signalMapperPort(new QSignalMapper(this))
  , signalMapperGenerator(new QSignalMapper(this))
  , signalMapperValue(new QSignalMapper(this))
  , serial(new QSerialPort)
  , cmd(REMOTE_PACKET_CMD_GET_SHIFT)
  , recordIdx(0)
  , ltr35(new Ltr35)
{
    TLTR ltrCrate;
    BYTE csn[LTR_CRATES_MAX][LTR_CRATE_SERIAL_SIZE];
    WORD mid[LTR_MODULES_PER_CRATE_MAX];
    QString crateSerial;
    int slotNumber;

    ui->setupUi(this);
    groupPort = new QActionGroup(ui->menuPort);
    groupGenerator = new QActionGroup(ui->menuGenerator);
    LTR_Init(&ltrServer);

    ltr35->moveToThread(&threadLtr35);
    threadLtr35.start();

    connect(signalMapperPort, SIGNAL(mapped(const QString &)), this, SLOT(setPort(const QString &)));
    foreach (QSerialPortInfo info,  QSerialPortInfo::availablePorts()) {
        QAction *node = new QAction(info.portName(), ui->menuPort);
        node->setCheckable(true);
        groupPort->addAction(node);
        signalMapperPort->setMapping(node, info.portName());
        connect(node, SIGNAL(triggered()), signalMapperPort, SLOT(map()));
        ui->menuPort->addAction(node);
    }

    connect(signalMapperGenerator, SIGNAL(mapped(const QString &)), this, SLOT(setGenerator(const QString &)));
    memset(csn, 0, sizeof(BYTE) * LTR_CRATES_MAX * LTR_CRATE_SERIAL_SIZE);
    if (LTR_IsOpened(&ltrServer) != LTR_OK) {
        LTR_OpenSvcControl(&ltrServer, LTRD_ADDR_DEFAULT, LTRD_PORT_DEFAULT);
    }
    if (LTR_IsOpened(&ltrServer) == LTR_OK)
    {
        LTR_GetCrates(&ltrServer, &csn[0][0]);
        for (int i = 0; i < LTR_CRATES_MAX; i++)
        {
            if (strlen((const char*)csn[i]) != 0)
            {
                LTR_Init(&ltrCrate);
                crateSerial = QString((const char*)csn[i]);
                if (LTR_OpenCrate(&ltrCrate, LTRD_ADDR_DEFAULT, LTRD_PORT_DEFAULT,
                                  LTR_CRATE_IFACE_UNKNOWN, crateSerial.toStdString().c_str()) == LTR_OK)
                {
                    LTR_GetCrateModules(&ltrCrate, &mid[0]);
                    for (int j = 0; j < LTR_MODULES_PER_CRATE_MAX; j++)
                    {
                        if (mid[j] != 0)
                        {
                            slotNumber = j + 1;
                            if (LTR_MID_LTR35 == mid[j])
                            {
                                if (ltr35->open(crateSerial, slotNumber))
                                {
                                    ltr35->close();
                                    QString nodeName = QString("%1:%2").arg(crateSerial).arg(slotNumber);
                                    QAction *node = new QAction(nodeName, ui->menuPort);
                                    node->setCheckable(true);
                                    groupGenerator->addAction(node);
                                    signalMapperGenerator->setMapping(node, nodeName);
                                    connect(node, SIGNAL(triggered()), signalMapperGenerator, SLOT(map()));
                                    ui->menuGenerator->addAction(node);
                                }
                            }
                        }
                    }
                    LTR_Close(&ltrCrate);
                }
            }
        }
        LTR_Close(&ltrServer);
    }

    ui->spinBoxSpeedSet->setMaximum(CDI_RPM_MAX);
    ui->spinBoxSpeedSet->setSingleStep(CDI_RPM_STEP);
    ui->spinBoxShiftSet->setMinimum(CDI_TIMING_UNDER_LOW);
    ui->spinBoxShiftSet->setMaximum(CDI_VALUE_MAX);

    for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
        timingsUi.append(createTimingUi(ui->gridLayoutTimings, QString("%1").arg(i + 1), i + 1));
    }
    lockTimings(true);
    lockShift(true);
    connect(ui->spinBoxShiftSet, SIGNAL(valueChanged(int)), this, SLOT(calcAllValues()));
    connect(signalMapperValue, SIGNAL(mapped(int)), this, SLOT(calcValue(int)));
    adjustSize();

    connect(&timerPortSend, SIGNAL(timeout()), this, SLOT(portSend()));
    connect(&timerPortAutoRead, SIGNAL(timeout()), this, SLOT(portRead()));
    connect(&timerPortReply, SIGNAL(timeout()), this, SLOT(portReplyTimeout()));
}

MainWindow::~MainWindow() {
    delete ui;
    delete groupGenerator;
    delete groupPort;
    delete signalMapperGenerator;
    delete signalMapperPort;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    timerPortSend.stop();
    timerPortAutoRead.stop();
    timerPortReply.stop();
    if (serial.isOpen()) {
        serial.close();
    }
    if (ltr35->isReady()) {
        if (ltr35->isBusy()) {
            QMetaObject::invokeMethod(ltr35.data(), "stop", Qt::QueuedConnection);
        }
        while (ltr35->isBusy()) {
            QThread::yieldCurrentThread();
        }
        ltr35->setupOutputsOff();
        ltr35->close();
    }
    threadLtr35.quit();
    threadLtr35.wait();
    e->accept();
}

MainWindow::TimingUi MainWindow::createTimingUi(QGridLayout *layout, QString name, int row) {
    TimingUi uiTimings;
    int column = 0;

    uiTimings.index = new QLabel(name);
    uiTimings.index->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(uiTimings.index, row, column++);

    uiTimings.rpm = new QSpinBox();
    uiTimings.rpm->setMaximum(CDI_RPM_MAX);
    uiTimings.rpm->setSingleStep(CDI_RPM_STEP);
    layout->addWidget(uiTimings.rpm, row, column++);

    uiTimings.timing = new QSpinBox();
    uiTimings.timing->setMinimum(CDI_TIMING_UNDER_LOW);
    uiTimings.timing->setMaximum(CDI_TIMING_OVER_HIGH);
    connect(uiTimings.timing, SIGNAL(valueChanged(int)), signalMapperValue, SLOT(map()));
    signalMapperValue->setMapping(uiTimings.timing, row);
    layout->addWidget(uiTimings.timing, row, column++);

    uiTimings.value = new QLineEdit();
    uiTimings.value->setReadOnly(true);
    layout->addWidget(uiTimings.value, row, column++);

    return uiTimings;
}

void MainWindow::lockShift(bool lock) {
    ui->spinBoxShiftSet->setEnabled(!lock);
    ui->pushButtonShiftSet->setEnabled(!lock);
}

void MainWindow::lockTimings(bool lock) {
    for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
        timingsUi[i].index->setEnabled(!lock);
        timingsUi[i].rpm->setEnabled(!lock);
        timingsUi[i].timing->setEnabled(!lock);
        timingsUi[i].value->setEnabled(!lock);
    }
    ui->pushButtonUpdate->setEnabled(!lock);
}

void MainWindow::setPort(const QString &portname) {
    if (serial.isOpen()) {
        serial.close();
    }
    timerPortSend.stop();
    timerPortAutoRead.stop();
    timerPortReply.stop();
    semaphoreTransmitComplete.acquire(semaphoreTransmitComplete.available());
    semaphoreTransmitComplete.release();
    mutexRequest.lock();
    recordIdx = 0;
    cmd = REMOTE_PACKET_CMD_GET_SHIFT;
    mutexRequest.unlock();
    serial.setPortName(portname);
    if (serial.open(QIODevice::ReadWrite)) {
        serial.setBaudRate(REMOTE_BAUDRATE);
        serial.setDataBits(QSerialPort::Data8);
        serial.setParity(QSerialPort::NoParity);
        serial.setStopBits(QSerialPort::OneStop);
        serial.setFlowControl(QSerialPort::NoFlowControl);
        timerPortSend.start(timerPortSendPeriodMs);
        timerPortAutoRead.start(timerPortAutoReadPeriodMs);
        ui->statusbar->showMessage(QString("Connected to %1").arg(portname));
    } else {
        ui->statusbar->showMessage(QString("Port %1 not available").arg(portname));
    }
}

void MainWindow::setGenerator(const QString &generator) {
    if (ltr35->isReady()) {
        if (ltr35->isBusy()) {
            QMetaObject::invokeMethod(ltr35.data(), "stop", Qt::QueuedConnection);
        }
        while (ltr35->isBusy()) {
            QThread::yieldCurrentThread();
        }
        ltr35->close();
    }
    ltr35->open(generator.split(':')[0], generator.split(':')[1].toInt());
    ui->spinBoxSpeedSet->setEnabled(true);
    ui->pushButtonGenerate->setEnabled(true);
    ui->pushButtonStop->setEnabled(true);
}

void MainWindow::calcValue(int row) {
    int timing = timingsUi[row - 1].timing->value();
    int shift = ui->spinBoxShiftSet->value();
    if ((shift - timing >= 0) && (shift - timing <= CDI_VALUE_MAX)) {
        timingsUi[row - 1].value->setText(QString("%1").arg(shift - timing));
    } else {
        timingsUi[row - 1].value->setText(QString(""));
    }
}

void MainWindow::calcAllValues() {
    for (int i = 1 ; i <= CDI_TIMING_RECORD_SLOTS; i++) {
        calcValue(i);
    }
}

void MainWindow::portRead() {
    uint8_t data[REMOTE_REPLY_PACKET_LEN];

    if (serial.isOpen()) {
        if (serial.bytesAvailable() >= REMOTE_REPLY_PACKET_LEN) {
            timerPortReply.stop();
            serial.read(reinterpret_cast<char *>(&data[REMOTE_REPLY_PACKET_PART_HEADER]), 1);
            if (REMOTE_HEADER == data[REMOTE_REPLY_PACKET_PART_HEADER]) {
                serial.read(reinterpret_cast<char *>(&data[REMOTE_REPLY_PACKET_PART_CMD]), REMOTE_REPLY_PACKET_LEN - 1);
                uint8_t crc = 0;
                for (int i = REMOTE_REPLY_PACKET_PART_HEADER; i < REMOTE_REPLY_PACKET_PART_CRC; i++) {
                    crc += data[i];
                }
                if (crc == data[REMOTE_REPLY_PACKET_PART_CRC]) {
                    if (REMOTE_PACKET_CMD_GET_RPM == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        QString rpm = QString("%1").arg(qFromLittleEndian<qint16>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]));
                        if (ui->lineEditSpeedReal->text() != rpm) {
                            ui->lineEditSpeedReal->setText(rpm);
                        }
                    } else if (REMOTE_PACKET_CMD_GET_RECORD == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        uint8_t idx = qFromLittleEndian<qint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]);
                        uint16_t rpm = qFromLittleEndian<qint16>(&data[REMOTE_REPLY_PACKET_PART_VALUE_2]);
                        uint8_t timing = qFromLittleEndian<qint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_1]);
                        timingsUi[idx].rpm->setValue(rpm);
                        timingsUi[idx].timing->setValue(timing);
                        timingsUi[idx].value->setText(QString("%1").arg(ui->spinBoxShiftSet->value() - timing));
                        mutexRequest.lock();
                        recordIdx++;
                        if (CDI_TIMING_RECORD_SLOTS == recordIdx) {
                            ui->statusbar->showMessage("Timings updated");
                            lockTimings(false);
                            recordIdx = 0;
                            cmd = REMOTE_PACKET_CMD_GET_RPM;
                        }
                        mutexRequest.unlock();
                    } else if (REMOTE_PACKET_CMD_GET_SHIFT == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        uint8_t shift = qFromLittleEndian<qint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]);
                        ui->spinBoxShiftSet->setValue(shift);
                        lockShift(false);
                        mutexRequest.lock();
                        recordIdx = 0;
                        cmd = REMOTE_PACKET_CMD_GET_RECORD;
                        mutexRequest.unlock();
                    } else if (REMOTE_PACKET_CMD_SET_RECORD == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        uint8_t idx = qFromLittleEndian<qint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]);
                        mutexRequest.lock();
                        recordIdx = idx + 1;
                        if (CDI_TIMING_RECORD_SLOTS == recordIdx) {
                            recordIdx = 0;
                            cmd = REMOTE_PACKET_CMD_GET_RECORD;
                            ui->statusbar->showMessage("Verifying timings");
                        }
                        mutexRequest.unlock();
                    } else if (REMOTE_PACKET_CMD_SET_SHIFT == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        cmd = REMOTE_PACKET_CMD_GET_RECORD;
                    } else {
                        ui->statusbar->showMessage("Unknown");
                    }
                    semaphoreTransmitComplete.release();
                } else {
                    timerPortReply.start(timerPortReplyTimeoutMs);
                }
            } else {
                timerPortReply.start(timerPortReplyTimeoutMs);
            }
        }
    }
}

void MainWindow::portSend() {
    uint8_t packet[REMOTE_REPLY_PACKET_LEN];

    if (semaphoreTransmitComplete.available() > 0) {
        semaphoreTransmitComplete.acquire(semaphoreTransmitComplete.available());
        packet[REMOTE_REPLY_PACKET_PART_HEADER] = REMOTE_HEADER;
        mutexRequest.lock();
        packet[REMOTE_REPLY_PACKET_PART_CMD] = cmd;
        if (REMOTE_PACKET_CMD_GET_RECORD == cmd) {
            packet[REMOTE_REPLY_PACKET_PART_VALUE_0] = recordIdx;
        } else if (REMOTE_PACKET_CMD_SET_RECORD == cmd) {
            packet[REMOTE_REPLY_PACKET_PART_VALUE_0] = recordIdx;
            packet[REMOTE_REPLY_PACKET_PART_VALUE_1] = timingsUi[recordIdx].value->text().toInt();
            qToLittleEndian(timingsUi[recordIdx].rpm->value(), &packet[REMOTE_REPLY_PACKET_PART_VALUE_2]);
        } else if (REMOTE_PACKET_CMD_SET_SHIFT == cmd) {
            packet[REMOTE_REPLY_PACKET_PART_VALUE_0] = ui->spinBoxShiftSet->value();
        }
        mutexRequest.unlock();
        uint8_t crc = 0;
        for (int i = REMOTE_REPLY_PACKET_PART_HEADER; i < REMOTE_REPLY_PACKET_PART_CRC; i++) {
            crc += packet[i];
        }
        packet[REMOTE_REPLY_PACKET_PART_CRC] = crc;
        timerPortReply.start(timerPortReplyTimeoutMs);
        serial.write(reinterpret_cast<char *>(packet), REMOTE_REPLY_PACKET_LEN);
    }
}

void MainWindow::portReplyTimeout() {
    timerPortReply.stop();
    semaphoreTransmitComplete.release();
}

void MainWindow::on_pushButtonShiftSet_released()
{
    mutexRequest.lock();
    recordIdx = 0;
    cmd = REMOTE_PACKET_CMD_SET_SHIFT;
    mutexRequest.unlock();
    ui->statusbar->showMessage("Writing new timings");
}

void MainWindow::on_pushButtonUpdate_released() {
    bool allOk = true;

    for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
        bool ok;
        timingsUi[i].value->text().toInt(&ok);
        allOk &= ok;
    }
    if (allOk) {
        lockTimings(true);
        mutexRequest.lock();
        recordIdx = 0;
        cmd = REMOTE_PACKET_CMD_SET_RECORD;
        mutexRequest.unlock();
        ui->statusbar->showMessage("Writing new timings");
    } else {
        ui->statusbar->showMessage("Timings wrong value");
    }
}

void MainWindow::on_pushButtonGenerate_released() {
    if (ltr35->isReady()) {
        if (!ltr35->isBusy()) {
            if (!ltr35->setupRotorChannel(rotorSignalChannel)) {
                return;
            }
        }
        if (ui->spinBoxSpeedSet->value() != 0) {
            if (ltr35->setupRotorSignal(rotorSignalAmplitude, (double)ui->spinBoxSpeedSet->value() / 60.0)) {
                QMetaObject::invokeMethod(ltr35.data(), "start", Qt::QueuedConnection);
            }
        }
    }
}

void MainWindow::on_pushButtonStop_released() {
    if (ltr35->isReady()) {
        if (ltr35->isBusy()) {
            QMetaObject::invokeMethod(ltr35.data(), "stop", Qt::QueuedConnection);
        }
    }
}

void MainWindow::on_checkBoxShiftAutoset_toggled(bool checked)
{
    ui->pushButtonShiftSet->setEnabled(!checked);
    //TODO: Enable auto shift set by timer
}
