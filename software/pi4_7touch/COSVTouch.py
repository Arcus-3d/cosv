import kivy
kivy.require('1.8.0')
  
from kivy.app import App
import serial
from serial.tools import list_ports

from kivy.properties import StringProperty, ObjectProperty
from kivy.uix.spinner import Spinner
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label 
from kivy.uix.switch import Switch 
from kivy.uix.gridlayout import GridLayout
from kivy.uix.popup import Popup
from kivy.uix.button import Button
from kivy.uix.behaviors import ButtonBehavior
from kivy.clock import Clock
from math import sin
  
from kivy.garden.graph import Graph, MeshLinePlot, LinePlot
def listSerialPorts():
    ports = list_ports.comports()
    portList = list()
    for port in ports:
        portList.append(port.device)
    return portList

class DisplayButton(BoxLayout,Button):
    text_top = StringProperty(' ')
    text_middle = StringProperty(' ')
    text_bottom = StringProperty(' ')
    obj_state = ObjectProperty(True)
    def __init__(self,*args,**kwargs):
        super(DisplayButton,self).__init__(*args,**kwargs)
        self.orientation="vertical"
        self.padding=(5,5)
        self.bold=True
        self.font_size='20sp'
        labelTop = Label(halign='center',valign='top',text=self.text_top,size=self.texture_size,font_size="13sp")
        self.add_widget(labelTop)
        labelMiddle = Label(text=" ",size=self.texture_size, bold=True,font_size='20sp')
        self.add_widget(labelMiddle)
        labelBottom = Label(halign='center',valign='bottom',text=self.text_bottom,size=self.texture_size,font_size="13sp")
        self.add_widget(labelBottom)


class COSVTouchApp(App):
    def build(self): 
        self.availablePorts = listSerialPorts()
        self.connection = serial.Serial
        self.counter = 1
        self.runState = False 
        if len(self.availablePorts) == 0:
            self.availablePorts.append("----")
        self.layoutMain = BoxLayout(orientation='horizontal',spacing=0, padding=0)
        
        self.layoutLeft = BoxLayout(orientation='vertical', spacing=0, padding=0,size_hint=(1,0.8))
        # Graphing area
        self.layoutGraph = BoxLayout(orientation='vertical', spacing=0, padding=(5,0))
        self.graphPaw = Graph(ylabel='Paw cmH2O', draw_border=True, y_ticks_major=5, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=0, ymax=20)
        self.graphFlow = Graph(ylabel='Flow l/min', draw_border=True, y_ticks_major=20, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=-60, ymax=60)
        self.graphCO2 = Graph(ylabel='CO2 mmHg', draw_border=True, y_ticks_major=10, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=0, ymax=40,size_hint_y=0)
        self.plot = []
        self.plot.append(LinePlot(color=[1, 0, 0, 1],line_width=1))
        self.plot.append(LinePlot(color=[0, 1, 0, 1],line_width=1))
        self.plot.append(LinePlot(color=[0, 1, 0, 1],line_width=1))
  
        self.reset_plots()
  
        self.graphPaw.add_plot(self.plot[0])
        self.graphFlow.add_plot(self.plot[1])
        self.graphCO2.add_plot(self.plot[2])

        self.layoutGraph.add_widget(self.graphPaw)
        self.layoutGraph.add_widget(self.graphFlow)
        self.layoutGraph.add_widget(self.graphCO2)

        self.layoutLeft.add_widget(self.layoutGraph)
        
        # Bottom Controls
        self.layoutControlBottom = BoxLayout(orientation='horizontal', spacing=10, padding=(5,10),size_hint=(1,0.2))
        self.buttonMode = DisplayButton(text_top="Mode",text="PCV-VG",text_bottom=" ");
        self.layoutControlBottom.add_widget(self.buttonMode);
        
        self.buttonTidalVolume = DisplayButton(text_top="TidalV",text="500",text_bottom="ml");
        self.layoutControlBottom.add_widget(self.buttonTidalVolume);
        
        self.buttonRespRate = DisplayButton(text_top="Rate",text="12",text_bottom="/min");
        self.layoutControlBottom.add_widget(self.buttonRespRate);
        
        self.buttonInhaleExhale = DisplayButton(text_top="I:E",text="1:2",text_bottom=" ");
        self.layoutControlBottom.add_widget(self.buttonInhaleExhale);
        
        self.buttonPEEP = DisplayButton(text_top="PEEP",text="5",text_bottom="cmH2O");
        self.layoutControlBottom.add_widget(self.buttonPEEP);
        
        self.buttonPressureMax = DisplayButton(text_top="Pmax",text="5",text_bottom="cmH2O");
        self.layoutControlBottom.add_widget(self.buttonPressureMax);
        
        self.layoutLeft.add_widget(self.layoutControlBottom)
        self.layoutMain.add_widget(self.layoutLeft)
        
        # Right Controls
        self.layoutControlRight = BoxLayout(orientation='vertical', spacing=10, padding=(10,5),size_hint=(0.2,1))
        self.buttonAlarmPause = DisplayButton(text="Alarm\nSilence",background_color=(1,1,0,1));
        self.layoutControlRight.add_widget(self.buttonAlarmPause);
        self.buttonAlarmSetup = DisplayButton(text="Alarm\nSetup");
        self.layoutControlRight.add_widget(self.buttonAlarmSetup);
        self.buttonSystemSetup = DisplayButton(text="System\nSetup");
        self.layoutControlRight.add_widget(self.buttonSystemSetup);
        self.buttonRun = DisplayButton(text_top="Status",text="Stop",background_color=(1,0.3,0.3,1))
        self.buttonRun.bind(on_press = self.buttonRunToggle)
        self.layoutControlRight.add_widget(self.buttonRun)

        
        self.layoutMain.add_widget(self.layoutControlRight)
       
        self.layoutSetup = BoxLayout(orientation='vertical', spacing=10, padding=(5,10))
        self.lblSerialSettings = Label(text="Serial settings")
        self.layoutSetup.add_widget(self.lblSerialSettings)
        
        self.dlBaudrate = Spinner(values = ["57600", "115200", "230400", "460800", "921600"],
                                  text = "115200")
        self.layoutSetup.add_widget(self.dlBaudrate)
        self.dlPort = Spinner(values = self.availablePorts,
                              text = self.availablePorts[0])
        self.layoutSetup.add_widget(self.dlPort)
        

        return self.layoutMain
    def buttonRunToggleInstance(self,instance):
        if instance.state == False:
            try:
                self.connection = serial.Serial(self.dlPort.text, self.dlBaudrate.text)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                self.counter = 1
                instance.state = True
                instance.text_middle="Running"      
                self.background_color=(0.3,1,0.3,1)      
            except Exception as e:
                print(e)
        else:
            try:
                self.connection.close()
                Clock.unschedule(self.get_data)
                self.reset_plots()
                instance.text_middle="Stop"      
                instance.background_color=(1,0.4,0.4,1)      
                instance.state = False

            except Exception as e:
                print(e)
     
    def buttonRunToggle(self,instance):
        if self.runState == False:
            try:
                self.connection = serial.Serial(self.dlPort.text, self.dlBaudrate.text)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                self.counter = 1
                self.buttonRun.text="Run"      
                self.buttonRun.background_color=(0.3,1,0.3,1)      
                self.runState = True
            except Exception as e:
                print(e)
        else:
            try:
                self.connection.close()
                Clock.unschedule(self.get_data)
                self.reset_plots()
                self.buttonRun.text="Stop"      
                self.buttonRun.background_color=(1,0.3,0.3,1)      
                self.runState = False

            except Exception as e:
                print(e)
    def buttonRunToggle(self,instance):
        if self.runState == False:
            try:
                self.connection = serial.Serial(self.dlPort.text, self.dlBaudrate.text)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                self.counter = 1
                self.buttonRun.text="Run"      
                self.buttonRun.background_color=(0.3,1,0.3,1)      
                self.runState = True
            except Exception as e:
                print(e)
        else:
            try:
                self.connection.close()
                Clock.unschedule(self.get_data)
                self.reset_plots()
                self.buttonRun.text="Stop"      
                self.buttonRun.background_color=(1,0.3,0.3,1)      
                self.runState = False

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
