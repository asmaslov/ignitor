import sys, threading, asyncio, time, serial, serial.tools.list_ports
from datetime import datetime
from functools import partial
from struct import pack
from ui_ignitor import Ui_MainWindow
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtWidgets import QWidget, QMessageBox, QSpinBox, QLineEdit, QHBoxLayout
from h2py_debug import *
from h2py_meter import *
from wx import *

UART_DEBUG = True

def isint(value):
    try:
        int(value)
        return True
    except ValueError:
        return False

class MainWindow(QtWidgets.QMainWindow):
    ui = Ui_MainWindow()
    ser = serial.Serial()
    timings = []
    timerPortSend = QtCore.QTimer()
    timerPortSendPeriodMs = 100
    timerPortAutoRead = QtCore.QTimer()
    timerPortAutoReadPeriodMs = 20
    timerPortReply = QtCore.QTimer()
    timerPortReplyTimeoutMs = 500
    cmd = DEBUG_PACKET_CMD_GET_RECORD
    recordIdx = 0
    requestlock = threading.Lock()
    eventTransmitComplete = asyncio.Event()

    def exitApprove(self):
        return QMessageBox.Yes == QMessageBox.question(self, 'Exit', 'Are you sure?', QMessageBox.Yes| QMessageBox.No)

    def setPort(self, name):
        if self.ser.isOpen():
            self.ser.close()
        self.timerPortSend.stop()
        self.timerPortAutoRead.stop()
        self.timerPortReply.stop()
        self.eventTransmitComplete.set()
        with self.requestlock:
            self.recordIdx = 0
            self.cmd = DEBUG_PACKET_CMD_GET_RECORD
        self.ser.port = str(name)
        self.ser.baudrate = DEBUG_BAUDRATE
        self.ser.parity = serial.PARITY_NONE
        self.ser.stopbits = serial.STOPBITS_ONE
        self.ser.bytesize = serial.EIGHTBITS
        try: 
            self.ser.open()
        except Exception as e:
            self.ui.statusbar.showMessage('Port ' + name + ' not available')
        if self.ser.isOpen():
            self.timerPortSend.start(self.timerPortSendPeriodMs)
            self.timerPortAutoRead.start(self.timerPortAutoReadPeriodMs)
            self.ui.statusbar.showMessage('Connected to ' + name)

    def portRead(self):
        if self.ser.isOpen():
            if self.ser.inWaiting() >= DEBUG_REPLY_PACKET_LEN:
                self.timerPortReply.stop()
                data = bytearray(self.ser.read())
                if DEBUG_HEADER == data[DEBUG_REPLY_PACKET_PART_HEADER]:                
                    data.extend(bytearray(self.ser.read(DEBUG_REPLY_PACKET_LEN - 1)))
                    if UART_DEBUG:
                        print(datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3] + ' -> ' + ' '.join('0x{:02X}'.format(x) for x in data))
                    crc = 0
                    for i in range (0, len(data) - 1):
                        crc = crc + data[i]
                    if (crc & 0xFF) == data[DEBUG_REPLY_PACKET_PART_CRC]:
                        if DEBUG_PACKET_CMD_TYPE_GET == (data[DEBUG_REPLY_PACKET_PART_CMD] & DEBUG_PACKET_CMD_TYPE_MASK) >> DEBUG_PACKET_CMD_TYPE_SHFT:
                            if DEBUG_PACKET_CMD_GET_RPM == data[DEBUG_REPLY_PACKET_PART_CMD]:
                                rpm = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_VALUE_2], 'little')
                                self.ui.lineEditSpeedReal.setText(str(rpm))
                            elif DEBUG_PACKET_CMD_GET_RECORD == data[DEBUG_REPLY_PACKET_PART_CMD]:
                                idx = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_VALUE_1], 'little')
                                rpm = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_2:DEBUG_REPLY_PACKET_PART_CRC], 'little')
                                value = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_1:DEBUG_REPLY_PACKET_PART_VALUE_2], 'little')
                                self.timings[idx]['rpm'].setValue(rpm)
                                self.timings[idx]['value'].setText(str(value))
                                with self.requestlock:
                                    self.recordIdx = self.recordIdx + 1
                                    if METER_TIMING_RECORD_TOTAL_SLOTS == self.recordIdx:
                                        self.recordIdx = 0
                                        self.cmd = DEBUG_PACKET_CMD_GET_RPM
                        else:
                            if DEBUG_PACKET_CMD_SET_RECORD == data[DEBUG_REPLY_PACKET_PART_CMD]:
                                idx = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_VALUE_1], 'little')
                                with self.requestlock:
                                    self.recordIdx = idx + 1
                                    if METER_TIMING_RECORD_TOTAL_SLOTS == self.recordIdx:
                                        self.recordIdx = 0
                                        self.cmd = DEBUG_PACKET_CMD_GET_RECORD
                                        self.ui.statusbar.showMessage('Acknowledged timings')
                            else:
                                self.ui.statusbar.showMessage('Unknown')
                        self.eventTransmitComplete.set()
                    else:
                        self.timerPortReply.start(self.timerPortReplyTimeoutMs)
                else:
                    self.timerPortReply.start(self.timerPortReplyTimeoutMs)

    def portReplyTimeout(self):
        self.timerPortReply.stop()
        self.eventTransmitComplete.set()

    def portSend(self):        
        if self.ser.isOpen():
            if self.eventTransmitComplete.is_set():
                self.eventTransmitComplete.clear()
                packet = bytearray()
                packet.append(DEBUG_HEADER)
                with self.requestlock:
                    packet.append(self.cmd)
                    if DEBUG_PACKET_CMD_GET_RECORD == self.cmd:
                        packet.append(self.recordIdx)
                        packet.append(0)
                        packet.append(0)
                        packet.append(0)              
                    elif DEBUG_PACKET_CMD_GET_RPM == self.cmd:
                        packet.append(0)                    
                        packet.append(0)
                        packet.append(0)
                        packet.append(0)
                    elif DEBUG_PACKET_CMD_SET_RECORD == self.cmd:
                        rpm = self.timings[self.recordIdx]['rpm'].value()
                        value = int(self.timings[self.recordIdx]['value'].text())
                        data = pack('BBH', self.recordIdx, value, rpm)
                        for d in data:
                            packet.append(d)
                    else:
                        packet.append(0)                    
                        packet.append(0)
                        packet.append(0)
                        packet.append(0)
                crc = 0
                for one in packet:
                    crc = crc + one
                packet.append(crc & 0xFF)
                if UART_DEBUG:
                    print(datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3] + ' <- ' + ' '.join('0x{:02X}'.format(x) for x in packet))
                self.timerPortReply.start(self.timerPortReplyTimeoutMs)
                self.ser.write(packet)
        else:
            self.ui.statusbar.showMessage('Port not open')
           
    def calcValue(self, idx, value):
        timing = self.timings[idx]['timing'].value()
        shift = self.timings[idx]['shift'].value()                
        self.timings[idx]['value'].setText(str(shift - timing))

    def __init__(self):
        super(MainWindow, self).__init__()
        self.ui.setupUi(self)
        group = QtWidgets.QActionGroup(self.ui.menuPort)
        self.signalMapper = QtCore.QSignalMapper(self)
        self.signalMapper.mapped[str].connect(self.setPort)
        for port in list(serial.tools.list_ports.comports()):
            node = QtWidgets.QAction(port.device, self.ui.menuPort, checkable = True)
            node.triggered.connect(self.signalMapper.map)
            self.signalMapper.setMapping(node, port.device)
            group.addAction(node)
            self.ui.menuPort.addAction(node)
        for i in range(0, METER_TIMING_RECORD_TOTAL_SLOTS):
            self.timings.append({'rpm' : QSpinBox(),
                              'timing' : QSpinBox(),
                              'shift' : QSpinBox(),
                              'value' : QLineEdit()})
            self.timings[-1]['rpm'].setMaximum(METER_RPM_MAX)
            self.timings[-1]['timing'].setMinimum(METER_TIMING_UNDER_LOW)
            self.timings[-1]['timing'].setMaximum(METER_TIMING_OVER_HIGH)
            self.timings[-1]['timing'].valueChanged.connect(partial(self.calcValue, len(self.timings) - 1))
            self.timings[-1]['shift'].setMaximum(UINT8_MAX)
            self.timings[-1]['shift'].valueChanged.connect(partial(self.calcValue, len(self.timings) - 1))
            self.timings[-1]['value'].setReadOnly(True)
            layout = QHBoxLayout()
            layout.addWidget(self.timings[-1]['rpm'])
            layout.addWidget(self.timings[-1]['timing'])
            layout.addWidget(self.timings[-1]['shift'])
            layout.addWidget(self.timings[-1]['value'])
            widget = QWidget()
            widget.setLayout(layout)
            self.ui.verticalLayoutTimings.addWidget(widget)
        self.adjustSize()
        self.timerPortSend.timeout.connect(self.portSend)
        self.timerPortAutoRead.timeout.connect(self.portRead)
        self.timerPortReply.timeout.connect(self.portReplyTimeout)

    def closeEvent(self, event):
        if self.exitApprove():
            self.timerPortSend.stop()
            self.timerPortAutoRead.stop()
            self.timerPortReply.stop()
            if self.ser.isOpen():
                self.ser.close()
            event.accept()
        else:
            event.ignore()

    def on_pushButtonGenerate_released(self):
        #TODO: Generate and send sample to VISA device
        pass

    def on_pushButtonUpdate_released(self):
        try:
            for i in range(0, METER_TIMING_RECORD_TOTAL_SLOTS):
                value = int(self.timings[i]['value'].text())
                if value < 0:
                    raise ValueError
        except ValueError:
            self.ui.statusbar.showMessage('Timings wrong value')
        else:
            with self.requestlock:
                self.recordIdx = 0
                self.cmd = DEBUG_PACKET_CMD_SET_RECORD

    @QtCore.pyqtSlot(bool)
    def on_actionExit_triggered(self, arg):
        if self.exitApprove():
            if self.ser.isOpen():
                self.ser.close()
            sys.exit()

app = QtWidgets.QApplication(sys.argv)
main = MainWindow()
main.show()
sys.exit(app.exec_())
