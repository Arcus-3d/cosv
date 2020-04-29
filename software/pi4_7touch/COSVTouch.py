import kivy
kivy.require('1.8.0')
  
import serial
from serial.tools import list_ports
from math import sin
from itertools import cycle

from kivy.app import App
from kivy.properties import StringProperty, ObjectProperty, OptionProperty
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
    def __init__(self,**kwargs):
        self.color=kwargs.pop('color',[1,1,1,1])
        self.line_width=kwargs.pop('line_width',1)
        self.draw_border=True
        self.y_grid_label=True
        self.x_grid_label=False
        self.padding=5
        self.x_grid=True
        self.y_grid=True
        self.xmin=0
        if (not self.y_ticks_major):
            if (self.ymax-self.ymin > 50 ):
                self.y_ticks_major=10
            else:
                self.y_ticks_major=5
        super(ScrollGraph,self).__init__(**kwargs)
        self.plot=LinePlot(color=self.color,line_width=self.line_width)
        self.add_plot(self.plot)
    def add_point(self,point):
        self.plot.points.append(point)
class ScrollGraphBoxLayout(BoxLayout):
    def __init__(self,**kwargs):
        self.graphHistorySize=kwargs.pop('history_size',600000)
        self.graphSize=kwargs.pop('graph_size',1200)
        super(ScrollGraphBoxLayout,self).__init__(**kwargs)
        self.autoScroll = True
        self.graphPosition = 1
        self.graphDataPosition = 1
        self.graphs = [];
        self.touchLast = None
    def reset(self):
        for graph in self.graphs:
            graph.plot.points = [(0,0)]
        self.graphPosition = 1
        self.graphDataPosition = 1
        self.update()
    def update(self):
        if (self.graphDataPosition == self.graphHistorySize):
            for graph in self.graphs:
                del(graph.plot.points[0])
                graph.plot.points[:] = [(i[0]-1, i[1]) for i in graph.plot.points[:]]
            self.graphDataPosition = self.graphHistorySize - 1;
        if self.autoScroll:
            self.graphPosition = self.graphDataPosition
        #if self.graphPosition > self.graphSize:
        for graph in self.graphs:
            graph.xmin=self.graphPosition-self.graphSize
            graph.xmax=self.graphPosition
    def add_graph(self,graph):
        graph.xmin=0
        graph.xmax=self.graphSize
        self.add_widget(graph)
        self.graphs.append(graph)
        
    def add_points(self,*argv):
        for i,graph in enumerate(self.graphs):
            point = 0.0 if len(argv) < i else argv[i]
            graph.add_point((self.graphDataPosition, point ))
        self.graphDataPosition += 1
        self.update()
    def on_touch_move(self,touch):
        if touch.grab_current is self:
            print(touch)
            if self.touchLast:
                dx = touch.x - self.touchLast.x
            else:
                dx = 0.0
            self.touchLast = touch
            #self.autoScroll=False
            self.graphPosition = self.graphPosition - dx
            self.update()
            return True
        else:
            pass
    def on_touch_down(self,touch):
        if touch.is_double_tap:
            self.autoScroll = True
        else:
            touch.grab(self)
        return True
    def on_touch_up(self,touch):
        if touch.grab_current is self:
            touch.ungrab(self)
            self.touchLast = None
            return True
        else:
            pass 

class COSVTouchApp(App):
    run_state = OptionProperty("stop",options=["stop","run"])
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
        #availablePorts = listSerialPorts()
        self.serial = serial.Serial()
        self.enableCO2 = False
        self.vt=0
        layoutMain = BoxLayout(orientation='horizontal',spacing=0, padding=0)
        
        layoutLeft = BoxLayout(orientation='vertical', spacing=0, padding=0)
        # Graphing area
        self.graphs = ScrollGraphBoxLayout(orientation='vertical', spacing=0, padding=(5,0),history_size=12000,graph_size=1200)
        self.graphs.add_graph(ScrollGraph(ylabel='Paw cmH2O', color=[1, 0, 1, 1], ymin=0, ymax=40))
        self.graphs.add_graph(ScrollGraph(ylabel='Flow L/min', color=[0, 1, 1, 1], ymin=-30, ymax=30))
        self.graphs.add_graph(ScrollGraph(ylabel='Vt mL', color=[1,1,0,1], ymin=0, ymax=15,size_hint=(1,0.75)))
        if (self.enableCO2):
            self.graphs.add_graph(ScrollGraph(ylabel='CO2 mmHg', color=[0.5,0.5,1,1], ymin=0, ymax=40))
        layoutLeft.add_widget(self.graphs)
        self.graphs.reset()
        
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
            self.run_state = 'stop'
        else:
            self.run_state = 'run'
    def on_run_state(self,instance,value):
        if value == 'run':
            try:
                availablePorts = listSerialPorts()
                self.serial = serial.Serial(port=availablePorts[0], baudrate=230400,timeout=1)
                Clock.schedule_interval(self.get_data, 1 / 100.)
                if (self.serial.is_open): 
                    self.serial.write(b'calibrate\n')
                self.graphs.reset()
            except Exception as e:
                print(e)
                popup=Popup(title='Error',content=Label(text=str(e)),size_hint=(None,None),size=(200,200))
                popup.open()
        else:
            try:
                self.serial.close()
                Clock.unschedule(self.get_data)
            except Exception as e:
                print(e)
                popup=Popup(title='Error',content=Label(text=str(e)),size_hint=(None,None),size=(200,200))
                popup.open()
  
    def get_data(self, dt):
        if self.serial.is_open:
            while self.serial.in_waiting > 0: 
                try:
                    row = str(self.serial.readline().decode('ascii'))
                    try:
                        col=[float(i) for i in row.strip().split(',',10)]
                        self.vt = self.vt + col[2]/100-0.01
                        self.vt = 0.0 if self.vt < 0.0 else self.vt
                        self.graphs.add_points(col[1],col[2],self.vt)
                        print(row)
                    except:
                        print(row)
                except Exception as e:      
                    print(e)
        else:
            print("Serial connection not open.")
class ErrorPopup(Popup):
    pass
  
if __name__ == '__main__':
    COSVTouchApp().run()
