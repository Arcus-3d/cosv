import kivy
kivy.require('1.8.0')
  
from kivy.app import App
from kivy.properties import ObjectProperty
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.popup import Popup
from kivy.clock import Clock
from math import sin
  
from kivy.garden.graph import Graph, MeshLinePlot
  
class VentTouchDemo(BoxLayout):
    def __init__(self):
        super(VentTouchDemo, self).__init__()
  
        self.sensorEnabled = False
        self.graph = self.ids.graph_plot
  
        self.plot = []
        self.plot.append(MeshLinePlot(color=[1, 0, 0, 1]))
        self.plot.append(MeshLinePlot(color=[0, 1, 0, 1]))
        self.plot.append(MeshLinePlot(color=[0, 0, 1, 1]))
  
        self.reset_plots()
  
        for plot in self.plot:
            self.graph.add_plot(plot)
  
    def reset_plots(self):
        for plot in self.plot:
            plot.points = [(0,0)]
  
        self.counter = 1
  
    def do_toggle(self):
        try:
            if not self.sensorEnabled:            
                Clock.schedule_interval(self.get_data, 1 / 20.)
  
                self.sensorEnabled = True
                self.ids.toggle_button.text = "Stop"
            else:
                self.reset_plots()
                Clock.unschedule(self.get_data)
  
                self.sensorEnabled = False
                self.ids.toggle_button.text = "Run"
        except NotImplementedError:
                popup = ErrorPopup()
                popup.open()
  
    def get_data(self, dt):
        if (self.counter == 100):
            for plot in self.plot:
                del(plot.points[0])
                plot.points[:] = [(i[0]-1, i[1]) for i in plot.points[:]]
  
            self.counter = 99
  
        self.plot[0].points.append((self.counter, sin(x/20) ))
        self.plot[1].points.append((self.counter, sin(x/30) ))
        self.plot[2].points.append((self.counter, sin(x/40) ))
  
        self.counter += 1      
  
class VentTouchDemoApp(App):
    def build(self):
        return VentTouchDemo()
  
class ErrorPopup(Popup):
    pass
  
if __name__ == '__main__':
    VentTouchDemoApp().run()
