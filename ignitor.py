import sys
import threading
import asyncio
import serial, time
import serial.tools.list_ports
from ui_ignitor import Ui_MainWindow
from PyQt5 import QtCore, QtGui, QtWidgets
from DEBUG import *

def isint(value):
    try:
        int(value)
        return True
    except ValueError:
        return False

class MainWindow(QtWidgets.QMainWindow):
    ui = Ui_MainWindow()
    ser = serial.Serial()
    timerPortRead = QtCore.QTimer()
    timerPortReadTimeoutMs = 50
    timerPortRequest = QtCore.QTimer()
    requestIdx = DEBUG_PACKET_IDX_GET_TIMING
    timerportWriteTimeoutMs = 100
    eventRequestComplete = asyncio.Event()
    eventWriteComplete = asyncio.Event()

    def exitApprove(self):
        return QtWidgets.QMessageBox.Yes == QtWidgets.QMessageBox.question(self, 'Exit', 'Are you sure?', QtWidgets.QMessageBox.Yes| QtWidgets.QMessageBox.No)

    def setPort(self, name):
        if self.ser.isOpen():
            self.ser.close()
        self.eventRequestComplete.set()
        self.eventWriteComplete.set()
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
            self.timerPortRead.start(self.timerPortReadTimeoutMs)
            self.timerPortRequest.start(self.timerportWriteTimeoutMs)
            self.ui.statusbar.showMessage('Connected to ' + name)

    def portRead(self):
        if self.ser.isOpen() and self.ser.inWaiting() >= DEBUG_REPLY_PACKET_LEN:
            data = self.ser.read(DEBUG_REPLY_PACKET_LEN)
            #print('-> ' + ' '.join('0x{:02X}'.format(x) for x in data))
            crc = 0
            for i in range (0, len(data) - 1):
                crc = crc + data[i]
            if (crc & 0xFF) == data[DEBUG_REPLY_PACKET_PART_CRC]:
                if DEBUG_HEADER == data[DEBUG_REPLY_PACKET_PART_HEADER]:
                    if DEBUG_PACKET_IDX_TYPE_GET == (data[DEBUG_REPLY_PACKET_PART_IDX] & DEBUG_PACKET_IDX_TYPE_MASK) >> DEBUG_PACKET_IDX_TYPE_SHFT:
                        self.eventRequestComplete.set()
                        if DEBUG_PACKET_IDX_GET_RPM == data[DEBUG_REPLY_PACKET_PART_IDX]:
                            speed = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_CRC], 'little')
                            self.ui.lineEditSpeed.setText(str(speed))
                        elif DEBUG_PACKET_IDX_GET_TIMING == data[DEBUG_REPLY_PACKET_PART_IDX]:
                            angle = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_CRC], 'little')
                            self.ui.lineEditAngle.setText(str(angle))
                    else:
                        self.eventWriteComplete.set()
                        if DEBUG_PACKET_IDX_SET_TIMING == data[DEBUG_REPLY_PACKET_PART_IDX]:
                            self.ui.statusbar.showMessage('Acknowledged angle')
                        elif DEBUG_PACKET_IDX_SET_LED == data[DEBUG_REPLY_PACKET_PART_IDX]:
                            self.ui.statusbar.showMessage('Acknowledged led')
                        else:
                            self.ui.statusbar.showMessage('Unknown')

    def portRequest(self):
        if self.eventRequestComplete.is_set() or self.eventWriteComplete.is_set():
            if self.ser.isOpen():
                self.eventRequestComplete.clear()
                packet = bytearray()
                packet.append(DEBUG_HEADER)
                packet.append(self.requestIdx)
                if (DEBUG_PACKET_IDX_GET_TIMING == self.requestIdx):
                    self.requestIdx = DEBUG_PACKET_IDX_GET_RPM
                    idx = 0
                    #TODO: Request all timings
                    packet.append(idx)
                else:
                    packet.append(0)
                packet.append(0)
                packet.append(0)
                packet.append(0)
                crc = 0
                for one in packet:
                    crc = crc + one
                packet.append(crc & 0xFF)
                #print('<- ' + ' '.join('0x{:02X}'.format(x) for x in packet))
                self.ser.write(packet)
            else:
                self.ui.statusbar.showMessage('Port not open')
        else:
            self.ui.statusbar.showMessage('Port awaiting answer')

    def portWrite(self, idx, data):
        if self.eventWriteComplete.is_set():
            self.eventRequestComplete.wait()
            if self.ser.isOpen():
                self.eventWriteComplete.clear()
                packet = bytearray()
                packet.append(DEBUG_HEADER)
                packet.append(idx)
                value = data.to_bytes(4, 'little')
                for v in value:
                    packet.append(v)
                crc = 0
                for one in packet:
                    crc = crc + one
                packet.append(crc & 0xFF)
                #print('<- ' + ' '.join('0x{:02X}'.format(x) for x in packet))
                self.ser.write(packet)
            else:
                self.ui.statusbar.showMessage('Port not open')
        else:
            self.ui.statusbar.showMessage('Port awaiting answer')

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
        self.timerPortRead.timeout.connect(self.portRead)
        self.timerPortRequest.timeout.connect(self.portRequest)

    def closeEvent(self, event):
        if self.exitApprove():
            if self.ser.isOpen():
                self.ser.close()
            event.accept()
        else:
            event.ignore()

    def on_pushButtonUpdate_released(self):
        #TODO: Write all timings
        if isint(self.ui.lineEditAngle.text()):
            angle = abs(int(self.ui.lineEditAngle.text()))
            self.portWrite(DEBUG_PACKET_IDX_SET_TIMING, angle)
        else:
            self.ui.statusbar.showMessage('Angle wrong value')

    def on_checkBoxLed_stateChanged(self, arg):
        if (QtCore.Qt.Checked == arg):
            self.portWrite(DEBUG_PACKET_IDX_SET_LED, True)
        else:
            self.portWrite(DEBUG_PACKET_IDX_SET_LED, False)

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
