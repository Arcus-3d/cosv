import kivy
kivy.require('1.8.0')
  
from kivy.app import App
import serial
from serial.tools import list_ports

from kivy.properties import ObjectProperty
from kivy.uix.spinner import Spinner
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label 
from kivy.uix.switch import Switch 
from kivy.uix.gridlayout import GridLayout
from kivy.uix.popup import Popup
from kivy.clock import Clock
from math import sin
  
from kivy.garden.graph import Graph, MeshLinePlot
def listSerialPorts():
    ports = list_ports.comports()
    portList = list()
    for port in ports:
        portList.append(port.device)
    return portList

class COSVTouchApp(App):
    def build(self): 
        self.availablePorts = listSerialPorts()
        self.counter = 1
        if len(self.availablePorts) == 0:
            self.availablePorts.append("----")
        self.gridLayout = GridLayout(cols=2)
        
        # Main tab
        self.graphLayout = BoxLayout(orientation='vertical', spacing=0, padding=(10,0))
        self.graphVolume = Graph(ylabel='Volume', x_ticks_minor=5, x_ticks_major=25, y_ticks_major=1, y_grid_label=True, x_grid_label=False, padding=0, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=-50, ymax=50)
        self.graphPressure = Graph(ylabel='Pressure', x_ticks_minor=5, x_ticks_major=25, y_ticks_major=1, y_grid_label=True, x_grid_label=False, padding=0, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=-50, ymax=50)
        self.graphBlah = Graph(ylabel='Blah', xlabel='Time', x_ticks_minor=5, x_ticks_major=25, y_ticks_major=1, y_grid_label=True, x_grid_label=False, padding=0, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=-50, ymax=50)
        self.plot = []
        self.plot.append(MeshLinePlot(color=[1, 0, 0, 1]))
        self.plot.append(MeshLinePlot(color=[0, 1, 0, 1]))
        self.plot.append(MeshLinePlot(color=[0, 0, 1, 1]))
  
        self.reset_plots()
  
        self.graphVolume.add_plot(self.plot[0])
        self.graphPressure.add_plot(self.plot[1])
        self.graphBlah.add_plot(self.plot[2])

        self.graphLayout.add_widget(self.graphVolume)
        self.graphLayout.add_widget(self.graphPressure)
        self.graphLayout.add_widget(self.graphBlah)
        self.gridLayout.add_widget(self.graphLayout)
        # Setup Tab
        self.setupLayout = BoxLayout(orientation='vertical', spacing=10, padding=(5,10),size_hint_x=0.1)
        
        self.lblSerialSettings = Label(text="Serial settings")
        self.setupLayout.add_widget(self.lblSerialSettings)
        
        self.dlBaudrate = Spinner(values = ["57600", "115200", "230400", "460800", "921600"],
                                  text = "115200")
        self.setupLayout.add_widget(self.dlBaudrate)
        
        self.dlPort = Spinner(values = self.availablePorts,
                              text = self.availablePorts[0])
        self.setupLayout.add_widget(self.dlPort)
        
        self.btnConnect = Switch()
        self.btnConnect.bind(active = self.connect)
        
        self.setupLayout.add_widget(self.btnConnect)
        self.gridLayout.add_widget(self.setupLayout)
        return self.gridLayout
    def connect(self, instance, value):
        if value == True:
            try:
                self.connection = serial.Serial(self.dlPort.text, self.dlBaudrate.text)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                self.counter = 1      
            except Exception as e:
                print(e)
        else:
            try:
                self.connection.close()
                Clock.unschedule(self.get_data)
                self.reset_plots()
            except Exception as e:
                print(e)
     
    def reset_plots(self):
        for plot in self.plot:
            plot.points = [(0,0)]
  
    def get_data(self, dt):
        if (self.counter == 600):
            for plot in self.plot:
                del(plot.points[0])
                plot.points[:] = [(i[0]-1, i[1]) for i in plot.points[:]]
  
            self.counter = 599
  
        self.plot[0].points.append((self.counter, sin(self.counter/20.)*50. ))
        self.plot[1].points.append((self.counter, sin(self.counter/30.)*50. ))
        self.plot[2].points.append((self.counter, sin(self.counter/40.)*50. ))
        self.counter += 1      
  
class ErrorPopup(Popup):
    pass
  
if __name__ == '__main__':
    COSVTouchApp().run()
