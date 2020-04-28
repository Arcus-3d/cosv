import kivy
kivy.require('1.8.0')
  
import serial
from serial.tools import list_ports
from math import sin
from itertools import cycle

from kivy.app import App
from kivy.properties import StringProperty, ObjectProperty
from kivy.uix.spinner import Spinner
from kivy.uix.slider import Slider
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label 
from kivy.uix.switch import Switch 
from kivy.uix.gridlayout import GridLayout
from kivy.uix.popup import Popup
from kivy.uix.button import Button
from kivy.uix.behaviors import ButtonBehavior
from kivy.clock import Clock
from kivy.core.window import Window
from kivy.garden.graph import Graph, MeshLinePlot, LinePlot
from kivy.config import Config
#Config.set('graphics','show_cursor','1')
#Config.write()
#quit()  
def listSerialPorts():
    ports = list_ports.comports()
    portList = list()
    for port in ports:
        portList.append(port.device)
    return portList

class DisplayButton(BoxLayout,Button):
    def updateValue(self,*args):
        self.text=str(self.value)
    def pressEvent(self,*args):
        return True
    def __init__(self,**kwargs):
        self.name=kwargs.pop('name',None)
        self.text_top=kwargs.pop('text_top',' ')
        self.value=kwargs.pop('value',0)
        self.text_bottom=kwargs.pop('text_bottom',' ')
        super(DisplayButton,self).__init__(**kwargs)
        self.text=str(self.value)
        self.spacing=0
        self.bold=True
        self.font_size='20sp'
        self.halign='center'
        self.orientation='vertical'
        labelTop = Label(halign='center',valign='top',text=self.text_top,font_size="13sp")
        self.add_widget(labelTop)
        labelMiddle = Label(text=" ",bold=True,font_size='20sp')
        self.add_widget(labelMiddle)
        labelBottom = Label(halign='center',valign='bottom',text=self.text_bottom,font_size="13sp")
        self.add_widget(labelBottom)
        self.bind(on_release=self.pressEvent)
        #self.bind(value=self.updateValue)

class PopupButton(DisplayButton):
    def __init__(self,**kwargs):
        self.popup_orientation=kwargs.pop('orientation','vertical')
        super(PopupButton,self).__init__(**kwargs)
        self.popupBox=BoxLayout(orientation=self.popup_orientation)
        size_hint=(0.2,0.7)
        if (self.popup_orientation == 'horizontal'): size_hint=(0.7,0.2)
        self.popup=Popup(title=self.text_top,auto_dismiss=False,content=self.popupBox,title_size='20sp',title_align='center',size_hint=size_hint)
    def updateValue(self,*args):
        self.popup.dismiss()
        super(PopupButton,self).updateValue(self,*args)
    def pressEvent(self,*args):
        self.popup.pos_hint={'center_x':(self.pos[0]+self.width/2)/Window.width,'bottom_y':(self.pos[1]+self.height)/Window.height}
        self.popup.open()
        super(PopupButton,self).pressEvent(self,*args)

class SliderButton(PopupButton):
    def __init__(self,**kwargs):
        self.min=kwargs.pop('min',0)
        self.max=kwargs.pop('max',100)
        self.step=kwargs.pop('step',1)
        self.sliderBind=False
        super(SliderButton,self).__init__(**kwargs)
        self.slider=Slider(min=self.min,value=self.value,max=self.max,step=self.step,cursor_width='32sp',orientation=self.popup_orientation,size_hint=(1,1),size=('25sp','100sp'))
        self.popupBox.add_widget(self.slider)
        #self.popupBox.title=str(self.value)
    def bindTouchUp(self,*args):
        self.slider.bind(on_touch_up=self.updateValue)
    def updateValue(self,*args):
        #self.popup.title=str(self.text_top) + str(':  ') + str(self.slider.value)
        self.value=int(self.slider.value)
        self.slider.unbind(on_touch_up=self.updateValue)
        self.slider.unbind(on_touch_move=self.updatePopup)
        super(SliderButton,self).updateValue(self,*args)
        
    def updatePopup(self,*args):
        self.popup.title=str(self.text_top) + str(' : ') + str(self.slider.value)
        Clock.schedule_once(self.bindTouchUp,0.5)
    def pressEvent(self,*args):
        self.slider.value=self.value
        super(SliderButton,self).pressEvent(self,*args)
        self.slider.bind(on_touch_move=self.updatePopup)
        self.updatePopup()

class SelectButton(PopupButton):
    def __init__(self,**kwargs):
        self.values=kwargs.pop('values',('------'))
        super(SelectButton,self).__init__(**kwargs)
        for v in self.values:
            button = Button(text=v,size=('100sp','50sp'), on_press=lambda button:self.updateValue(button.text))
            self.popupBox.add_widget(button)
    def updateValue(self,value):
        self.value=value
        super(SelectButton,self).updateValue(self,value)
    def stopUpdate(self,value):
        return False
    def pressEvent(self,value):
        self.popup.pos_hint={'center_x':(self.pos[0]+self.width/2)/Window.width,'bottom_y':(self.pos[1]+self.height)/Window.height}
        self.popup.open()

class RotaryButton(DisplayButton):
    def __init__(self,**kwargs):
        self.value_colors=kwargs.pop('value_colors',{'default':(1,1,1,1)})
        self.values=kwargs.pop('values',('-----'))
        self.value_list=cycle(self.values)
        super(RotaryButton,self).__init__(**kwargs)
        if (self.value in self.values):
            for v in self.value_list:
                if (v == self.value): break
        if (self.value in self.value_colors): self.background_color=self.value_colors.get(self.value) 
    def pressEvent(self,*args):
        super(RotaryButton,self).pressEvent(self,args)
        self.value=next(self.value_list)
        self.text =str(self.value)
        if (self.value in self.value_colors): self.background_color=self.value_colors.get(self.value) 
class ScrollGraph(Graph):
    pass
class COSVTouchApp(App):
    def build_config(self,config):
        config.setdefaults('state', {
            'running'   : 0,
            'mode'      : 'PCV-VG',
            'tidalv'    : 500,
            'rate'      : 20,
            'ie'        : '1:2',
            'pressure'  : 20,
            'pmax'      : 50,
            'peep'      : 0,
        })
        config.setdefaults('modes', {
            'PCV-VG': 'Pressure Controlled Ventilation'
        })
    def build(self): 
        availablePorts = listSerialPorts()
        self.serial = serial.Serial()
        self.counter = 1
        self.runState = False
        self.enableCO2 = False
        self.vt=0;
        layoutMain = BoxLayout(orientation='horizontal',spacing=0, padding=0)
        
        layoutLeft = BoxLayout(orientation='vertical', spacing=0, padding=0)
        # Graphing area
        layoutGraph = BoxLayout(orientation='vertical', spacing=0, padding=(5,0))
        graphPaw = ScrollGraph(ylabel='Paw cmH2O', draw_border=True, y_ticks_major=10, y_grid_label=True, x_grid_label=False,  padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=0, ymax=60)
        graphFlow = ScrollGraph(ylabel='Flow L/min', draw_border=True, y_ticks_major=10, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=-30, ymax=30)
        graphVt = ScrollGraph(ylabel='Vt mL', draw_border=True, y_ticks_major=10, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=0, ymax=40)
        if (self.enableCO2):
            graphCO2 = ScrollGraph(ylabel='CO2 mmHg', draw_border=True, y_ticks_major=10, y_grid_label=True, x_grid_label=False, padding=5, x_grid=True, y_grid=True, xmin=0, xmax=600, ymin=0, ymax=40,size_hint_y=0,)
        self.plot = []
        self.plot.append(LinePlot(color=[1, 0, 1, 1],line_width=1))
        self.plot.append(LinePlot(color=[0, 1, 1, 1],line_width=1))
        self.plot.append(LinePlot(color=[1, 1, 0, 1],line_width=1))
        if (self.enableCO2): 
            self.plot.append(LinePlot(color=[0.5, 0.5, 1, 1],line_width=1))
  
        self.reset_plots()
  
        graphPaw.add_plot(self.plot[0])
        graphFlow.add_plot(self.plot[1])
        graphVt.add_plot(self.plot[2])
        if (self.enableCO2):
            graphCO2.add_plot(self.plot[3])

        layoutGraph.add_widget(graphPaw)
        layoutGraph.add_widget(graphFlow)
        layoutGraph.add_widget(graphVt)
        if (self.enableCO2):
            layoutGraph.add_widget(graphCO2)
        #self.graphCO2.height='0dp'
        #self.graphCO2.size_hint_y=0
        layoutLeft.add_widget(layoutGraph)
        
        # Bottom Controls
        layoutControlBottom = BoxLayout(orientation='horizontal', spacing=10, padding=(5,10),size_hint=(1,0.2))
        buttonMode = SelectButton(name='mode',text_top="Mode",value="PCV-VG",values=('PCV-VG','PCV'),text_bottom=" ")
        layoutControlBottom.add_widget(buttonMode);
        
        buttonTidalVolume = SliderButton(name='tidalv',text_top="TidalV",min=200,max=800,value=500,text_bottom="ml");
        layoutControlBottom.add_widget(buttonTidalVolume);
        
        buttonRespRate = SliderButton(name='rate',text_top="Rate",min=8,max=40,value=18,text_bottom="/min");
        layoutControlBottom.add_widget(buttonRespRate);
        
        buttonInhaleExhale = SelectButton(name='ie',text_top="I:E",value="1:2",values=('1:1','1:2','1:3','1:4'),text_bottom=" ");
        layoutControlBottom.add_widget(buttonInhaleExhale);
        
        buttonPEEP = SliderButton(name='peep',text_top="PEEP",min=0,value="5",max=25,text_bottom="cmH2O");
        layoutControlBottom.add_widget(buttonPEEP);
        
        buttonPressureMax = SliderButton(name='pmax',text_top="Pmax",min=10,value="20",max=60,text_bottom="cmH2O");
        layoutControlBottom.add_widget(buttonPressureMax);
        
        layoutLeft.add_widget(layoutControlBottom)
        layoutMain.add_widget(layoutLeft)
        
        # Right Controls
        layoutControlRight = BoxLayout(orientation='vertical', spacing=10, padding=(10,5),size_hint=(0.2,1))
        buttonAlarmPause = RotaryButton(name='alarm',text_top='Alarm Status',value="Silenced",values={'Normal','Alarm','Silenced'},value_colors={'Normal':(0.3,1,0.3,1),'Silenced':(1,1,0.3,1),'Alarm':(1,0.3,0.3,1)});
        layoutControlRight.add_widget(buttonAlarmPause);
        buttonAlarmSetup = DisplayButton(value="Alarm\nSetup");
        layoutControlRight.add_widget(buttonAlarmSetup);
        buttonSystemSetup = DisplayButton(value="System\nSetup");
        layoutControlRight.add_widget(buttonSystemSetup);
        self.buttonRun = RotaryButton(name='status',text_top="Status",on_press=self.runButton,value="Stopped",values=('Stopped','Running'),value_colors={'Stopped':(1,0.3,0.3,1),'Running':(0.3,1,0.3,1)})
        layoutControlRight.add_widget(self.buttonRun)

        layoutMain.add_widget(layoutControlRight)
       
        return layoutMain
    def runButton(self,event):
        if self.buttonRun.value == 'Stopped':
            try:
                availablePorts = listSerialPorts()
                self.serial = serial.Serial(port=availablePorts[0], baudrate=230400,timeout=1)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                self.counter = 1
            except Exception as e:
                print(e)
                popup=Popup(title='Error',content=Label(text=str(e)),size_hint=(None,None),size=(200,200))
                popup.open()
        else:
            try:
                self.serial.close()
                Clock.unschedule(self.get_data)
                self.reset_plots()
            except Exception as e:
                print(e)
                popup=Popup(title='Error',content=Label(text=str(e)),size_hint=(None,None),size=(200,200))
                popup.open()
    def reset_plots(self):
        for plot in self.plot:
            plot.points = [(0,0)]
  
    def get_data(self, dt):
        if (self.counter == 600):
            for plot in self.plot:
                del(plot.points[0])
                plot.points[:] = [(i[0]-1, i[1]) for i in plot.points[:]]
            self.counter = 599
        try:
            if (self.serial.is_open and self.serial.in_waiting > 0): 
                row = str(self.serial.readline().decode('ascii'))
                col=[float(i) for i in row.strip().split(',',10)]
                self.vt = (self.vt + col[2]/100);
                if (self.vt < 0):
                    self.vt = 0
                self.plot[0].points.append((self.counter, col[1] ))
                self.plot[1].points.append((self.counter, col[2] ))
                self.plot[2].points.append((self.counter, self.vt ))
                #self.plot[3].points.append((self.counter, col[4] ))
                self.counter += 1
        except Exception as e:      
            print(e)
            print(row)

class ErrorPopup(Popup):
    pass
  
if __name__ == '__main__':
    COSVTouchApp().run()
