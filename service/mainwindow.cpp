#include "mainwindow.h"
#include "ui_ignitor.h"
#include "remote.h"
#include "cdi.h"
#include <QAction>
#include <QCloseEvent>
#include <QSerialPortInfo>
#include <QtEndian>
#include <QMessageBox>
#include <QFileDialog>

#include <QDebug>

constexpr char MainWindow::timingsFileExtension[];

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , signalMapperPort(new QSignalMapper(this))
  , signalMapperGeneratorLtr35(new QSignalMapper(this))
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

    connect(signalMapperGeneratorLtr35, SIGNAL(mapped(const QString &)), this, SLOT(setGeneratorLtr35(const QString &)));
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
                                    signalMapperGeneratorLtr35->setMapping(node, nodeName);
                                    connect(node, SIGNAL(triggered()), signalMapperGeneratorLtr35, SLOT(map()));
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

    ui->spinBoxSpeedSet->setMinimum(CDI_RPM_MIN);
    ui->spinBoxSpeedSet->setMaximum(CDI_RPM_MAX);
    ui->spinBoxSpeedSet->setSingleStep(CDI_RPM_STEP);
    ui->spinBoxShiftSet->setMinimum(CDI_TIMING_UNDER_LOW);
    ui->spinBoxShiftSet->setMaximum(CDI_VALUE_MAX);

    for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
        timingsUi.append(createTimingUi(ui->gridLayoutTimings, QString("%1").arg(i + 1), i + 1));
    }
    lockTimings(true);
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
    delete signalMapperGeneratorLtr35;
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
    uiTimings.rpm->setMinimum(CDI_RPM_MIN);
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
    ui->pushButtonShiftSet->setEnabled(!lock);
    ui->checkBoxShiftAutoset->setEnabled(!lock);
}

void MainWindow::lockTimings(bool lock) {
    ui->pushButtonUpdate->setEnabled(!lock);
    ui->actionWriteMemory->setEnabled(!lock);
}

bool MainWindow::loadTimingsFile(QString fileName) {
    QFile file(fileName);
    bool result = false;
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray byteArray = file.readAll();
        result = !byteArray.isEmpty();
        QDataStream stream(byteArray);
        stream.setVersion(QDataStream::Qt_5_6);
        int shift;
        TimingRecord timings[CDI_TIMING_RECORD_SLOTS];
        stream >> shift
               >> timings[0].rpm >> timings[0].timing
               >> timings[1].rpm >> timings[1].timing
               >> timings[2].rpm >> timings[2].timing
               >> timings[3].rpm >> timings[3].timing
               >> timings[4].rpm >> timings[4].timing
               >> timings[5].rpm >> timings[5].timing
               >> timings[6].rpm >> timings[6].timing
               >> timings[7].rpm >> timings[7].timing
               >> timings[8].rpm >> timings[8].timing
               >> timings[9].rpm >> timings[9].timing
               >> timings[10].rpm >> timings[10].timing;
        ui->spinBoxShiftSet->setValue(shift);
        for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
            timingsUi[i].rpm->setValue(timings[i].rpm);
            timingsUi[i].timing->setValue(timings[i].timing);
        }
    } else {
        QMessageBox::critical(this, tr("Error opening file"), tr("Unable to open file"));
    }
    return result;
}

bool MainWindow::saveTimingsFile(QString fileName) {
    QFile file(fileName);
    bool result = false;
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_6);
        int shift = ui->spinBoxShiftSet->value();
        TimingRecord timings[CDI_TIMING_RECORD_SLOTS];
        for (int i = 0; i < CDI_TIMING_RECORD_SLOTS; i++) {
            timings[i].rpm = timingsUi[i].rpm->value();
            timings[i].timing = timingsUi[i].timing->value();
        }
        stream << shift
               << timings[0].rpm << timings[0].timing
               << timings[1].rpm << timings[1].timing
               << timings[2].rpm << timings[2].timing
               << timings[3].rpm << timings[3].timing
               << timings[4].rpm << timings[4].timing
               << timings[5].rpm << timings[5].timing
               << timings[6].rpm << timings[6].timing
               << timings[7].rpm << timings[7].timing
               << timings[8].rpm << timings[8].timing
               << timings[9].rpm << timings[9].timing
               << timings[10].rpm << timings[10].timing;
        result = file.write(byteArray);
        file.close();
    } else {
        QMessageBox::critical(this, tr("Error opening file"), tr("Unable to open file"));
    }
    return result;
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

void MainWindow::setGeneratorLtr35(const QString &generator) {
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
                    if (REMOTE_PACKET_CMD_GET_RPS == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        uint8_t rps = qFromLittleEndian<quint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]);
                        QString rpm = QString("%1").arg(rps * 60);
                        if (ui->lineEditSpeedReal->text() != rpm) {
                            ui->lineEditSpeedReal->setText(rpm);
                        }
                    } else if (REMOTE_PACKET_CMD_GET_RECORD == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        uint8_t idx = qFromLittleEndian<quint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_0]);
                        uint8_t rps = qFromLittleEndian<quint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_1]);
                        uint8_t timing = qFromLittleEndian<quint8>(&data[REMOTE_REPLY_PACKET_PART_VALUE_2]);
                        timingsUi[idx].rpm->setValue(rps * 60);
                        timingsUi[idx].timing->setValue(timing);
                        timingsUi[idx].value->setText(QString("%1").arg(ui->spinBoxShiftSet->value() - timing));
                        mutexRequest.lock();
                        recordIdx++;
                        if (CDI_TIMING_RECORD_SLOTS == recordIdx) {
                            ui->statusbar->showMessage("Timings loaded");
                            lockTimings(false);
                            if (ui->checkBoxShiftAutoset->isChecked()) {
                                cmd = REMOTE_PACKET_CMD_SET_SHIFT;
                            } else {
                                cmd = REMOTE_PACKET_CMD_GET_RPS;
                            }
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
                            ui->statusbar->showMessage("Timings updated");
                            lockTimings(false);
                            if (ui->checkBoxShiftAutoset->isChecked()) {
                                cmd = REMOTE_PACKET_CMD_SET_SHIFT;
                            } else {
                                cmd = REMOTE_PACKET_CMD_GET_RPS;
                            }
                        }
                        mutexRequest.unlock();
                    } else if (REMOTE_PACKET_CMD_SET_SHIFT == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        ui->statusbar->showMessage("Shift updated");
                        ui->spinBoxShiftSet->setEnabled(true);
                        if (!ui->checkBoxShiftAutoset->isChecked()) {
                            mutexRequest.lock();
                            cmd = REMOTE_PACKET_CMD_GET_RPS;
                            mutexRequest.unlock();
                        }
                    } else if (REMOTE_PACKET_CMD_SAVE_MEM == data[REMOTE_REPLY_PACKET_PART_CMD]) {
                        ui->statusbar->showMessage("EEPROM data updated");
                        if (!ui->checkBoxShiftAutoset->isChecked()) {
                            mutexRequest.lock();
                            cmd = REMOTE_PACKET_CMD_GET_RPS;
                            mutexRequest.unlock();
                        }
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
            packet[REMOTE_REPLY_PACKET_PART_VALUE_1] = timingsUi[recordIdx].rpm->value() / 60;
            packet[REMOTE_REPLY_PACKET_PART_VALUE_2] = timingsUi[recordIdx].timing->value();
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
    ui->spinBoxShiftSet->setEnabled(false);
    mutexRequest.lock();
    cmd = REMOTE_PACKET_CMD_SET_SHIFT;
    mutexRequest.unlock();
    ui->statusbar->showMessage("Writing new shift");
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
    mutexRequest.lock();
    cmd = REMOTE_PACKET_CMD_SET_SHIFT;
    mutexRequest.unlock();
}

void MainWindow::on_actionOpen_triggered()
{
    QString openDir = ".";
    if (!timingsFileName.isEmpty())
    {
        openDir = QFileInfo(timingsFileName).absoluteDir().path();
    }
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open timings"), openDir, QString("%1 (*.%2)").arg(tr("Timings file")).arg(timingsFileExtension));
    if (!fileName.isEmpty())
    {
        if (loadTimingsFile(fileName))
        {
            timingsFileName = fileName;
            setWindowTitle(QCoreApplication::applicationName() + " - " + timingsFileName);
        }
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (QFile::exists(timingsFileName))
    {
        saveTimingsFile(timingsFileName);
    }
    else
    {
        QString fileName = QFileDialog::getSaveFileName(this,
            tr("Save timings"), ".", QString("%1 (*.%2)").arg(tr("Timings file")).arg(timingsFileExtension));
        if (!fileName.isEmpty())
        {
            if (!fileName.endsWith(QString(".%1").arg(timingsFileExtension)))
            {
                fileName.append(QString(".%1").arg(timingsFileExtension));
            }
            if (saveTimingsFile(fileName))
            {
                timingsFileName = fileName;
                setWindowTitle(QCoreApplication::applicationName() + " - " + timingsFileName);
            }
        }
    }
}

void MainWindow::on_actionSaveAs_triggered()
{
    QString openDir = ".";
    if (!timingsFileName.isEmpty())
    {
        openDir = QFileInfo(timingsFileName).absoluteDir().path();
    }
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save timings"), openDir, QString("%1 (*.%2)").arg(tr("Timings file")).arg(timingsFileExtension));
    if (!fileName.isEmpty())
    {
        if (!fileName.endsWith(QString(".%1").arg(timingsFileExtension)))
        {
            fileName.append(QString(".%1").arg(timingsFileExtension));
        }
        if (saveTimingsFile(fileName))
        {
            timingsFileName = fileName;
            setWindowTitle(QCoreApplication::applicationName() + " - " + timingsFileName);
        }
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionWriteMemory_triggered()
{
    if (QMessageBox::Yes == QMessageBox::question(this, tr("Attention"), tr("Write data to EEPROM?"), QMessageBox::Yes | QMessageBox::No)) {
        mutexRequest.lock();
        cmd = REMOTE_PACKET_CMD_SAVE_MEM;
        mutexRequest.unlock();
        ui->statusbar->showMessage("Writing data to EEPROM");
    }
}
