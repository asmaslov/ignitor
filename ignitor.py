import sys
import threading
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
    timerPortReadTimeoutMs = 100
    timerPortRequest = QtCore.QTimer()
    timerportWriteTimeoutMs = 500
    triggerActivateStatus = 0
    triggerFireStatus = 0
    triggerSingleStatus = 0
    
    def setPort(self, name):
        self.ser = serial.Serial()
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
        crc = 0
        if self.ser.isOpen() and (self.ser.inWaiting() >= DEBUG_REPLY_PACKET_LEN):
            data = self.ser.read(DEBUG_REPLY_PACKET_LEN)
            print(' '.join('0x{:02X}'.format(x) for x in data))
            for i in range (0, len(data) - 1):
                crc = crc + data[i]
            if (crc & 0xFF) == data[DEBUG_REPLY_PACKET_PART_CRC]:
                if (data[DEBUG_REPLY_PACKET_PART_HEADER] == DEBUG_HEADER):
                    if data[DEBUG_REPLY_PACKET_PART_IDX] == DEBUG_CONTROL_PACKET_IDX_GET_SPEED:
                        speed = int.from_bytes(data[DEBUG_REPLY_PACKET_PART_VALUE_0:DEBUG_REPLY_PACKET_PART_CRC], 'little')
                        self.ui.lineEditSpeed.setText(str(speed))
                    elif data[DEBUG_REPLY_PACKET_PART_IDX] == DEBUG_CONTROL_PACKET_IDX_SET_ANGLE:
                        self.ui.statusbar.showMessage('Acknowledged angle')
                    else:
                        self.ui.statusbar.showMessage('Unknown')

    def portRequest(self):
        if self.ser.isOpen():
            packet = bytearray()
            packet.append(DEBUG_HEADER)
            packet.append(DEBUG_CONTROL_PACKET_IDX_GET_SPEED)
            crc = 0
            for one in packet:
                crc = crc + one
            packet.append(crc & 0xFF)
            print('-> ' + ' '.join('0x{:02X}'.format(x) for x in packet))
            self.ser.write(packet)
        else:
            self.ui.statusbar.showMessage('Port not open')

    def portWrite(self, idx, data):
        if self.ser.isOpen():
            packet = bytearray()
            packet.append(DEBUG_HEADER)
            packet.append(idx)
            value = data.to_bytes(4, 'little')
            for v in value:
                packet.append(v)
            #packet.append((data >> 24) & 0xFF)
            #packet.append((data >> 16) & 0xFF)
            #packet.append((data >> 8) & 0xFF)
            #packet.append(data & 0xFF)
            crc = 0
            for one in packet:
                crc = crc + one
            packet.append(crc & 0xFF)
            print('-> ' + ' '.join('0x{:02X}'.format(x) for x in packet))
            self.ser.write(packet)
        else:
            self.ui.statusbar.showMessage('Port not open')

    def __init__(self):
        super(MainWindow, self).__init__()
        self.ui.setupUi(self)
        group = QtWidgets.QActionGroup(self.ui.menuPort, exclusive = True)
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
        choice = QtWidgets.QMessageBox.question(self, 'Exit', 'Are you sure?', QtWidgets.QMessageBox.Yes| QtWidgets.QMessageBox.No)
        if choice == QtWidgets.QMessageBox.Yes:
            if self.ser.isOpen():
                self.ser.close()
            event.accept()
        else:
            event.ignore()
          
    def on_pushButtonUpdate_released(self):
        if isint(self.ui.lineEditAngle.text()):
            angle = abs(int(self.ui.lineEditAngle.text()))
            self.portWrite(DEBUG_CONTROL_PACKET_IDX_SET_ANGLE, angle)
        else:
            self.ui.statusbar.showMessage('Angle wrong value')

    @QtCore.pyqtSlot(bool)
    def on_actionExit_triggered(self, arg):
        choice = QtGui.QMessageBox.question(self, 'Exit', 'Are you sure?', QtGui.QMessageBox.Yes | QtGui.QMessageBox.No)
        if choice == QtGui.QMessageBox.Yes:
            sys.exit()

app = QtWidgets.QApplication(sys.argv)
main = MainWindow()
main.show()
sys.exit(app.exec_())
