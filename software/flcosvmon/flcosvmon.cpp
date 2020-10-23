/*
   This file is part of VISP Core.

   VISP Core is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   VISP Core is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with VISP Core.  If not, see <http://www.gnu.org/licenses/>.

   Author: Steven.Carr@hammontonmakers.org
*/


// TODO: adjustable amount of time on the charts?  Currently fixed at 20 seconds
// TODO: Plateau Markers on the charts (Walk the list from the chart's arraySize() and arryItem() and identify plateau's and update the items *str with a strdup() of the value

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/fl_draw.H>
#include <math.h>
#include "My_Chart.H"
// for isspace()
#include <ctype.h>

// for read()
#include <stdio.h>
#include <unistd.h>

// for open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// for serial
#include <string.h>
#include <termios.h>
#include <unistd.h>

// for network
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <sys/signal.h>

#define MAX_CHARTS 3
#define MAX_CHART_TIME 20000 /* 20 seconds on the display */

#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 60
#define TITLE_HEIGHT  24

int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        if (tcgetattr (fd, &tty) != 0)
                return -1;

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                return -1;
        return 0;
}

typedef struct colormap_s {
  const char *name;
  uchar r;
  uchar g;
  uchar b;
} colormap_t;

colormap_t colors[] = {
  {"AliceBlue", 0xF0, 0xF8, 0xFF},
  {"AntiqueWhite", 0xFA, 0xEB, 0xD7},
  {"Aqua", 0x00, 0xFF, 0xFF},
  {"Aquamarine", 0x7F, 0xFF, 0xD4},
  {"Azure", 0xF0, 0xFF, 0xFF},
  {"Beige", 0xF5, 0xF5, 0xDC},
  {"Bisque", 0xFF, 0xE4, 0xC4},
  {"Black", 0x00, 0x00, 0x00},
  {"BlanchedAlmond", 0xFF, 0xEB, 0xCD},
  {"Blue", 0x00, 0x00, 0xFF},
  {"BlueViolet", 0x8A, 0x2B, 0xE2},
  {"Brown", 0xA5, 0x2A, 0x2A},
  {"BurlyWood", 0xDE, 0xB8, 0x87},
  {"CadetBlue", 0x5F, 0x9E, 0xA0},
  {"Chartreuse", 0x7F, 0xFF, 0x00},
  {"Chocolate", 0xD2, 0x69, 0x1E},
  {"Coral", 0xFF, 0x7F, 0x50},
  {"CornflowerBlue", 0x64, 0x95, 0xED},
  {"Cornsilk", 0xFF, 0xF8, 0xDC},
  {"Crimson", 0xDC, 0x14, 0x3C},
  {"Cyan", 0x00, 0xFF, 0xFF},
  {"DarkBlue", 0x00, 0x00, 0x8B},
  {"DarkCyan", 0x00, 0x8B, 0x8B},
  {"DarkGoldenRod", 0xB8, 0x86, 0x0B},
  {"DarkGray", 0xA9, 0xA9, 0xA9},
  {"DarkGreen", 0x00, 0x64, 0x00},
  {"DarkKhaki", 0xBD, 0xB7, 0x6B},
  {"DarkMagenta", 0x8B, 0x00, 0x8B},
  {"DarkOliveGreen", 0x55, 0x6B, 0x2F},
  {"DarkOrange", 0xFF, 0x8C, 0x00},
  {"DarkOrchid", 0x99, 0x32, 0xCC},
  {"DarkRed", 0x8B, 0x00, 0x00},
  {"DarkSalmon", 0xE9, 0x96, 0x7A},
  {"DarkSeaGreen", 0x8F, 0xBC, 0x8F},
  {"DarkSlateBlue", 0x48, 0x3D, 0x8B},
  {"DarkSlateGray", 0x2F, 0x4F, 0x4F},
  {"DarkTurquoise", 0x00, 0xCE, 0xD1},
  {"DarkViolet", 0x94, 0x00, 0xD3},
  {"DeepPink", 0xFF, 0x14, 0x93},
  {"DeepSkyBlue", 0x00, 0xBF, 0xFF},
  {"DimGray", 0x69, 0x69, 0x69},
  {"DodgerBlue", 0x1E, 0x90, 0xFF},
  {"FireBrick", 0xB2, 0x22, 0x22},
  {"FloralWhite", 0xFF, 0xFA, 0xF0},
  {"ForestGreen", 0x22, 0x8B, 0x22},
  {"Fuchsia", 0xFF, 0x00, 0xFF},
  {"Gainsboro", 0xDC, 0xDC, 0xDC},
  {"GhostWhite", 0xF8, 0xF8, 0xFF},
  {"Gold", 0xFF, 0xD7, 0x00},
  {"GoldenRod", 0xDA, 0xA5, 0x20},
  {"Gray", 0x80, 0x80, 0x80},
  {"Green", 0x00, 0x80, 0x00},
  {"GreenYellow", 0xAD, 0xFF, 0x2F},
  {"HoneyDew", 0xF0, 0xFF, 0xF0},
  {"HotPink", 0xFF, 0x69, 0xB4},
  {"IndianRed", 0xCD, 0x5C, 0x5C},
  {"Indigo", 0x4B, 0x00, 0x82},
  {"Ivory", 0xFF, 0xFF, 0xF0},
  {"Khaki", 0xF0, 0xE6, 0x8C},
  {"Lavender", 0xE6, 0xE6, 0xFA},
  {"LavenderBlush", 0xFF, 0xF0, 0xF5},
  {"LawnGreen", 0x7C, 0xFC, 0x00},
  {"LemonChiffon", 0xFF, 0xFA, 0xCD},
  {"LightBlue", 0xAD, 0xD8, 0xE6},
  {"LightCoral", 0xF0, 0x80, 0x80},
  {"LightCyan", 0xE0, 0xFF, 0xFF},
  {"LightGoldenRodYellow", 0xFA, 0xFA, 0xD2},
  {"LightGray", 0xD3, 0xD3, 0xD3},
  {"LightGreen", 0x90, 0xEE, 0x90},
  {"LightPink", 0xFF, 0xB6, 0xC1},
  {"LightSalmon", 0xFF, 0xA0, 0x7A},
  {"LightSeaGreen", 0x20, 0xB2, 0xAA},
  {"LightSkyBlue", 0x87, 0xCE, 0xFA},
  {"LightSlateGray", 0x77, 0x88, 0x99},
  {"LightSteelBlue", 0xB0, 0xC4, 0xDE},
  {"LightYellow", 0xFF, 0xFF, 0xE0},
  {"Lime", 0x00, 0xFF, 0x00},
  {"LimeGreen", 0x32, 0xCD, 0x32},
  {"Linen", 0xFA, 0xF0, 0xE6},
  {"Magenta", 0xFF, 0x00, 0xFF},
  {"Maroon", 0x80, 0x00, 0x00},
  {"MediumAquaMarine", 0x66, 0xCD, 0xAA},
  {"MediumBlue", 0x00, 0x00, 0xCD},
  {"MediumOrchid", 0xBA, 0x55, 0xD3},
  {"MediumPurple", 0x93, 0x70, 0xDB},
  {"MediumSeaGreen", 0x3C, 0xB3, 0x71},
  {"MediumSlateBlue", 0x7B, 0x68, 0xEE},
  {"MediumSpringGreen", 0x00, 0xFA, 0x9A},
  {"MediumTurquoise", 0x48, 0xD1, 0xCC},
  {"MediumVioletRed", 0xC7, 0x15, 0x85},
  {"MidnightBlue", 0x19, 0x19, 0x70},
  {"MintCream", 0xF5, 0xFF, 0xFA},
  {"MistyRose", 0xFF, 0xE4, 0xE1},
  {"Moccasin", 0xFF, 0xE4, 0xB5},
  {"NavajoWhite", 0xFF, 0xDE, 0xAD},
  {"Navy", 0x00, 0x00, 0x80},
  {"OldLace", 0xFD, 0xF5, 0xE6},
  {"Olive", 0x80, 0x80, 0x00},
  {"OliveDrab", 0x6B, 0x8E, 0x23},
  {"Orange", 0xFF, 0xA5, 0x00},
  {"OrangeRed", 0xFF, 0x45, 0x00},
  {"Orchid", 0xDA, 0x70, 0xD6},
  {"PaleGoldenRod", 0xEE, 0xE8, 0xAA},
  {"PaleGreen", 0x98, 0xFB, 0x98},
  {"PaleTurquoise", 0xAF, 0xEE, 0xEE},
  {"PaleVioletRed", 0xDB, 0x70, 0x93},
  {"PapayaWhip", 0xFF, 0xEF, 0xD5},
  {"PeachPuff", 0xFF, 0xDA, 0xB9},
  {"Peru", 0xCD, 0x85, 0x3F},
  {"Pink", 0xFF, 0xC0, 0xCB},
  {"Plum", 0xDD, 0xA0, 0xDD},
  {"PowderBlue", 0xB0, 0xE0, 0xE6},
  {"Purple", 0x80, 0x00, 0x80},
  {"Red", 0xFF, 0x00, 0x00},
  {"RosyBrown", 0xBC, 0x8F, 0x8F},
  {"RoyalBlue", 0x41, 0x69, 0xE1},
  {"SaddleBrown", 0x8B, 0x45, 0x13},
  {"Salmon", 0xFA, 0x80, 0x72},
  {"SandyBrown", 0xF4, 0xA4, 0x60},
  {"SeaGreen", 0x2E, 0x8B, 0x57},
  {"SeaShell", 0xFF, 0xF5, 0xEE},
  {"Sienna", 0xA0, 0x52, 0x2D},
  {"Silver", 0xC0, 0xC0, 0xC0},
  {"SkyBlue", 0x87, 0xCE, 0xEB},
  {"SlateBlue", 0x6A, 0x5A, 0xCD},
  {"SlateGray", 0x70, 0x80, 0x90},
  {"Snow", 0xFF, 0xFA, 0xFA},
  {"SpringGreen", 0x00, 0xFF, 0x7F},
  {"SteelBlue", 0x46, 0x82, 0xB4},
  {"Tan", 0xD2, 0xB4, 0x8C},
  {"Teal", 0x00, 0x80, 0x80},
  {"Thistle", 0xD8, 0xBF, 0xD8},
  {"Tomato", 0xFF, 0x63, 0x47},
  {"Turquoise", 0x40, 0xE0, 0xD0},
  {"Violet", 0xEE, 0x82, 0xEE},
  {"Wheat", 0xF5, 0xDE, 0xB3},
  {"White", 0xFF, 0xFF, 0xFF},
  {"WhiteSmoke", 0xF5, 0xF5, 0xF5},
  {"Yellow", 0xFF, 0xFF, 0x00},
  {"YellowGreen", 0x9A, 0xCD, 0x32},
  {NULL,0,0,0}
};

Fl_Color lookupColor(const char *name)
{
    for (colormap_t *color=colors; color->name; color++)
    {
        if (strcasecmp(color->name, name)==0)
        {
            return fl_rgb_color(color->r, color->g, color->b);
        }
    }
    return FL_FOREGROUND_COLOR;
}     

// 'X' overlay the whole window if the core is missing or reports being 'bad'
class My_Group : public Fl_Group {
public:
    bool isBad;
    My_Group(int X, int Y, int W, int H, const char*L=0) : Fl_Group(X,Y,W,H,L) {
      isBad=true;
    }
    void setBad() { isBad=true; redraw(); }
    void setGood() { isBad=false; redraw(); }
    
    void draw() {
        Fl_Group::draw();
        if (isBad)
        {
          // DRAW RED 'X'
          fl_color(FL_RED);
          int x1 = 0,       y1 = 0;
          int x2 = w()-1, y2 = h()-1;
          fl_line_style(FL_SOLID, 5);
          fl_line(x1, y1, x2, y2);
          fl_line(x1, y2, x2, y1);
        }
    }
};

class MyPopupWindow;

class FL_EXPORT My_Button : public Fl_Button {
protected:
  void draw();
  void resize(int x,int y,int w,int h) {
      Fl_Button::resize(x,y,w,h);	// no resizing of height
  }
  void draw_name() const;
  void draw_units() const;
  void draw_name(int X, int Y, int W, int H) const;
  void draw_units(int X, int Y, int W, int H) const;
  void SizeToText();
  
  Fl_Label name_;
  Fl_Label units_;
  
public:
  My_Button(int,int,int,int,const char * = 0);
  ~My_Button();
  void name(const char *);
  void units(const char *);
  void label(const char *);
};

void My_Button::SizeToText() {
    int W=0, H=0, W2=0, H2=0, W3=0, H3=0;
    fl_font(labelfont(), labelsize());
    fl_measure(Fl_Button::label(), W, H, 0);
    if (W<BUTTON_WIDTH) W=BUTTON_WIDTH;
    if (H<BUTTON_HEIGHT) H=BUTTON_HEIGHT;

    fl_font(name_.font, name_.size);
    fl_measure(name_.value, W2, H2, 0);
    if (W2>W) W=W2;
    if (H2>H) H=H2;

    fl_font(units_.font, units_.size);
    fl_measure(units_.value, W3, H3, 0);
    if (W3>W) W=W3;
    if (H3>H) H=H3;
    
    resize(0, 0, W+10, H+10);
}


// Draw
void My_Button::draw() {
    Fl_Button::draw_label();
    Fl_Button::draw();
    draw_name();
    draw_units();
}

void My_Button::draw_name() const {
  int X = x()+Fl::box_dx(box());
  int W = w()-Fl::box_dw(box());
  if (W > 11 && FL_ALIGN_TOP&(FL_ALIGN_LEFT|FL_ALIGN_RIGHT)) {X += 3; W -= 6;}
  draw_name(X, y()+Fl::box_dy(box()), W, h()-Fl::box_dh(box()));
}

void My_Button::draw_name(int X, int Y, int W, int H) const  {
  Fl_Label l1 = name_;
  l1.draw(X,Y,W,H,FL_ALIGN_TOP);
}

void My_Button::draw_units() const {
  int X = x()+Fl::box_dx(box());
  int W = w()-Fl::box_dw(box());
  if (W > 11 && FL_ALIGN_BOTTOM&(FL_ALIGN_LEFT|FL_ALIGN_RIGHT)) {X += 3; W -= 6;}
  draw_units(X, y()+Fl::box_dy(box()), W, h()-Fl::box_dh(box()));
}

void My_Button::draw_units(int X, int Y, int W, int H) const {
  int tW=0, tH=0;
  Fl_Label l1 = units_;
  l1.draw(X,Y,W,H,FL_ALIGN_BOTTOM);
}

void My_Button::units(const char *a) {
  if (units_.value) free((void *)(units_.value));
  units_.value=strdup((a?a:""));
  SizeToText();
  redraw();
}

void My_Button::name(const char *a) {
  if (name_.value) free((void *)(name_.value));
  name_.value=strdup((a?a:""));
  SizeToText();
  redraw();
}

void My_Button::label(const char *a) {
  Fl_Button::label(a);
  SizeToText();
}

// CTOR
My_Button::My_Button(int X,int Y,int W, int H, const char *l) : Fl_Button(X,Y,W,H,l) {
  name_.value	 = strdup("");
  name_.image    = 0;
  name_.deimage  = 0;
  name_.type	 = FL_NORMAL_LABEL;
  name_.font	 = FL_HELVETICA;
  name_.size	 = FL_NORMAL_SIZE;
  name_.color	 = FL_FOREGROUND_COLOR;
  name_.align_	 = FL_ALIGN_CENTER;
  units_.value	 = strdup("");
  units_.image   = 0;
  units_.deimage = 0;
  units_.type	 = FL_NORMAL_LABEL;
  units_.font	 = FL_HELVETICA;
  units_.size	 = FL_NORMAL_SIZE;
  units_.color	 = FL_FOREGROUND_COLOR;
  units_.align_	 = FL_ALIGN_CENTER;    
}

My_Button::~My_Button() {
  if (name_.value) free((void *)(name_.value));
  if (units_.value) free((void *)(units_.value));
}


typedef struct chart_s {
   const char *name;
   int minRange;
   int maxRange;
   int waterLevel; // dashed line on the chart indicating a threshold, if outside of the range, do not show
   int color;
   // Maybe alarm settings?
} chart_t;

chart_t charts[MAX_CHARTS] = {
{"Pressure",    -10,  40,  40, FL_YELLOW},
{"Volume",      -80,  80,  80, FL_GREEN},
{"Tidal Volume",  0, 800, 600, FL_BLUE}
};

typedef struct dictionary_s {
    char *name;
    char *description;
    struct dictionary_s *next;
} dictionary_t;

typedef struct dynamic_button_s {
    char *name;
    int isButton; // one of 2 groups we can be, "status" or "button"
    int enabled;  // Input is enabled or disabled
    int isBad;    // Individual buttons can be good/bad (like unknown motor, or too much volume)
    dictionary_t *dictionary; // For dictionary types
    char *textValue; // For dictionary types
    char *units;     // Units told to us by the core
    char *bgcolor;     // background color for button
    char *fgcolor;     // text color
    int minRange, maxRange; // For integer ranges    
    struct dynamic_button_s *next;
    My_Button *flButton; // The button to update
} dynamic_button_t;



typedef struct buffer_s {
    char *data; // Input buffer of current command;
    int size, allocated; // Housekeeping for *buffer
} buffer_t;

typedef struct core_s {
    int fd; // Serial/socket stream
    const char *title; // title of this patient/stream
    const char *serialPort; // Serial port we have connected to
    char *criticalText; // Last critical log output (stuff into titlebar)
    buffer_t streamData;
    int cmdTime; // 32-bit millis() since the core started
    int lastCmdTime; // 32-bit millis() since the core started
    int lastCmdTimeChecked; // Just checking to see if it is different...
    int adjustedMillis; // What we use to stuff into the chart.  adjustedMillis += ((cmdTime-lastCmdTime)>0 ? (cmdTime-lastCmdTime) : 0);
    bool addIcons;      // Only add when we are between 'Q' commands
    char *settingName; // For use in command processing strdup()'ed
    char *pendingChange; // slider's send us an update for every position while it's moving...
    int settingType;
    int sentQ;
    bool isGood;       // Is the core 'good', used for broadcasting our health out over networks
    int broadcastSock;
    int listeningSock;
    int remoteFd;      // If !=-1 then we have a remote monitor in control
    dynamic_button_t *button_list;

    Fl_Double_Window *win;
    Fl_Scroll *scrollButtons;
    Fl_Scroll *scrollStatusItems;
    Fl_Pack *packedButtons;
    Fl_Pack *packedStatusItems;
    My_Group *groupedCharts;
    My_Chart *flCharts[MAX_CHARTS];
    Fl_Box *titleBox;
    MyPopupWindow *popup; // So when a 'Q' happens, we can dismiss the active popup
} core_t;

void bufferClear(buffer_t *buffer)
{
    memset(buffer->data, 0, buffer->size);
    buffer->size=0;
}

void bufferAppend(buffer_t *buffer, char ch)
{
   if ((buffer->size+1) >= buffer->allocated)
   {
       char *newBuffer = (char *)realloc(buffer->data, buffer->allocated+1024);
       if (newBuffer)
       {
           buffer->data=newBuffer;
           buffer->allocated += 1024;
       }
   }
   if (buffer->size < buffer->allocated)
       buffer->data[buffer->size++] = ch;
   // NUll Terminate
   if (buffer->size < buffer->allocated)
       buffer->data[buffer->size] = 0;
}



void setHealth(core_t *core, int isGood)
{
  // if !isGood, big red X on screen, or red banner, whatever
  core->isGood = isGood;
  if (isGood) core->groupedCharts->setGood(); else core->groupedCharts->setBad();
  
  if (isGood)
  {
    if (core->criticalText)
    {
      free(core->criticalText);
      core->criticalText=NULL;
    }
    core->titleBox->label(core->title);
  }
  else
  {
      if (core->criticalText)
        core->titleBox->label(core->criticalText);
      else
        core->titleBox->label("Unknown Error");
  }  
}

dynamic_button_t *buttonLookup(core_t *core, char *settingName)
{
    for (dynamic_button_t *ptr=core->button_list; ptr; ptr=ptr->next)
    {
        if (strcasecmp(settingName,ptr->name)==0)
             return ptr;
    }
    
    if (!core->addIcons)
        return NULL;
    
    dynamic_button_t *newPtr = (dynamic_button_t *)calloc(1, sizeof(dynamic_button_t));
    newPtr->name = strdup(settingName);

    for (dynamic_button_t *ptr=core->button_list; ptr; ptr=ptr->next)
    {
        if (ptr->next==NULL)
        {
            ptr->next = newPtr;
            return newPtr;
        }
    }
    core->button_list=newPtr;
    return newPtr;
}

void buttonDictionaryClear(dynamic_button_t *b)
{
    dictionary_t *dictionary; // For dictionary types
    while (b->dictionary)
    {
        dictionary = b->dictionary;
        b->dictionary = dictionary->next;
        if (dictionary->name) free(dictionary->name);
        if (dictionary->description) free(dictionary->description);
        free(dictionary);
    }
}

void buttonDictionaryEntryAppend(dynamic_button_t *b, char *name)
{
    for (dictionary_t *d=b->dictionary; d; d=d->next)
    {
        if (d->next==NULL)
        {
            d->next = (dictionary_t *)calloc(1, sizeof(dictionary_t));
            d=d->next;
            d->name=strdup(name);
            return;
        }
    }

    b->dictionary = (dictionary_t *)calloc(1, sizeof(dictionary_t));
    b->dictionary->name = strdup(name);
    return;
}

// Set the last entries description
void buttonDictionaryEntryDescription(dynamic_button_t *b, char *description)
{
    for (dictionary_t *d=b->dictionary; d; d=d->next)
    {
        if (d->next==NULL)
        {
            if (d->description) free(d->description);
            d->description=strdup(description);
            return;
        }
    }
}

int hasFocus(Fl_Group *g)
{
    Fl_Widget *f = Fl::focus();

    for (int x=0; x<g->children(); x++)
    {
        if (g->child(x)==f)
           return true;
    }
    return false;
}


void processSerialFailure(core_t *core)
{
  core->sentQ=0;
  Fl::remove_fd(core->fd);
  close(core->fd);
  core->fd = -1;
  // Yeah, we are bad now...
  
  if (core->criticalText)
      free(core->criticalText);
  core->criticalText = strdup("Core Serial Communication Failure");
  setHealth(core, false);
}

class MyPopupWindow : public Fl_Double_Window {
public:
    MyPopupWindow(int X,int Y,int W,int H, const char* title) : Fl_Double_Window (X, Y, W, H, title) {}

    int handle(int e) {
        core_t *core = (core_t *)user_data();
        switch(e) {
            case FL_FOCUS:
                break;
            case FL_UNFOCUS:
                if (core->pendingChange)
                {
                  printf("<%s", core->pendingChange);
                  if (core->fd!=-1)
                  {
                    if (-1==write(core->fd, core->pendingChange, strlen(core->pendingChange)))
                      processSerialFailure(core);
                    else
                      fsync(core->fd);
                  }
                  free(core->pendingChange);
                  core->pendingChange=NULL; 
                }
                if (!hasFocus((Fl_Pack *)this->child(0)))
                {
                  core->popup=NULL;
                  Fl::delete_widget(this);
                }
                break;
        }
        return(Fl_Double_Window::handle(e));
    }
};




void sliderChanged(Fl_Widget *w, void *data)
{
    const char *bname = (const char *)data;
    core_t *core = (core_t *)w->parent()->user_data();

    if (core->fd!=-1)
    {
      if (core->pendingChange)
          free(core->pendingChange);
      core->pendingChange=NULL;
      asprintf(&core->pendingChange, "S,%s,%d\n", bname, (int)((Fl_Value_Slider *)w)->value());
    }
}


// Simple slider bar set to current strtoul(b->textValue); with minRange/maxRange values
void popupRangeSelection(core_t *core, dynamic_button_t *b)
{
  core->popup = new MyPopupWindow(0,0,600, BUTTON_HEIGHT*2, b->name);
  core->popup->user_data((void *)core);

  Fl_Value_Slider *slider = new Fl_Value_Slider(0,0,600,BUTTON_HEIGHT*2);
  slider->bounds(b->minRange, b->maxRange);
  slider->type(FL_HOR_NICE_SLIDER);
  slider->value(atoi(b->textValue));
  slider->callback(sliderChanged, (void *)b->name);
  slider->step(1);
  core->popup->position((Fl::w() - core->popup->w())/2, (Fl::h() - core->popup->h())/2);
  core->popup->show();
}

void popupSelected(Fl_Widget *w, void *data)
{
    core_t *core = (core_t *)data;    
    const char *bname = (const char *)w->parent()->user_data();
    const char *dname = (const char *)w->label();
    char *buf;
    asprintf(&buf, "S,%s,%s\n", bname, dname);
    if (core->fd!=-1)
    {
      printf("<%s", buf);
      if (-1==write(core->fd, buf, strlen(buf)))
        processSerialFailure(core);
      else
        fsync(core->fd);      
    }
    free(buf);
    // Tell the window to hide
    w->parent()->parent()->hide();
    //Fl::delete_widget(w->parent()->parent());
}

// Generate a popup window that is dictionary based
void popupDictionarySelection(core_t *core, dynamic_button_t *b)
{
  int count=0;
  for (dictionary_t *d=b->dictionary; d; d=d->next)
    count++;

  if (count)
  {
    core->popup = new MyPopupWindow(0,0,300, count*BUTTON_HEIGHT, b->name);
    Fl_Pack *packer = new Fl_Pack(0,0,300, count*BUTTON_HEIGHT);
    packer->type(Fl_Pack::VERTICAL);
    core->popup->user_data((void *)core);
    for (dictionary_t *d=b->dictionary; d; d=d->next)
    {
      Fl_Button *button = new Fl_Button(0,0,BUTTON_WIDTH, BUTTON_HEIGHT, d->name);
      button->tooltip(d->description);
      packer->user_data(b->name);
      button->callback(popupSelected, (void *)core);
    }
    core->popup->position((Fl::w() - core->popup->w())/2, (Fl::h() - core->popup->h())/2);
    core->popup->show();
  }
}


void buttonPushed(Fl_Widget *w, void *data)
{
  core_t *core = (core_t *)data;
  // Lookup the button in the core->button_list
  // Determine what to show, and show it!
  for (dynamic_button_t *b=core->button_list; b; b=b->next)
  {
     if (b->flButton==w)
     {
       if (b->dictionary)
           popupDictionarySelection(core, b);
       else
           popupRangeSelection(core, b);
       return;
     }
  }
}

void wipeIcon(core_t *core, dynamic_button_t *button)
{
    buttonDictionaryClear(button);
    if (button->textValue)
        free(button->textValue);
    if (button->units)
        free(button->units);
    if (button->bgcolor)
        free(button->bgcolor);
    if (button->fgcolor)
        free(button->fgcolor);
}

void wipeIcons(core_t *core)
{
  core->scrollButtons->scroll_to(0,0);
  core->scrollStatusItems->scroll_to(0,0);
  core->packedButtons->clear();
  core->packedStatusItems->clear();
  while (core->button_list)
  {
      dynamic_button_t *b = core->button_list;
      core->button_list = b->next;
      wipeIcon(core, b);
      free(b);
  }
}

void refreshIcons(core_t *core)
{
    for (dynamic_button_t *b=core->button_list; b; b=b->next)
    {
        if (b->flButton==NULL)
            b->flButton = new My_Button(0,0,BUTTON_WIDTH,BUTTON_HEIGHT);
        

        b->flButton->box(FL_RFLAT_BOX);    // buttons won't have 'edges'
        // If a color is sent from the core, and it is not 'empty', use the core defined color, else, our own
        b->flButton->labelsize(24);
        if (b->isButton)
        {
            b->flButton->callback(buttonPushed, (void *)core);
            core->packedButtons->add(b->flButton);
        }
        else
        {
            // No action for status outputs
            core->packedStatusItems->add(b->flButton);
        }

        b->flButton->name(b->name);
        b->flButton->label(b->textValue);
        b->flButton->units(b->units);
        b->flButton->color(b->bgcolor && *b->bgcolor ? lookupColor(b->bgcolor) : (b->isBad ? lookupColor("DarkRed") :  lookupColor("Gray")));
        b->flButton->labelcolor(b->fgcolor && *b->fgcolor ? lookupColor(b->fgcolor) : (b->isBad ? lookupColor("White") : lookupColor("SkyBlue")));

        if (b->enabled)
           b->flButton->show();
        else
           b->flButton->hide();
        if (b->isButton)
        {
            core->packedButtons->init_sizes();
            core->packedButtons->parent()->damage(FL_DAMAGE_EXPOSE|FL_DAMAGE_ALL);
        }
        else
        {
            core->packedStatusItems->init_sizes();
            core->packedStatusItems->parent()->damage(FL_DAMAGE_EXPOSE|FL_DAMAGE_ALL);
        }
    }
}


#define SETTING_UNKNOWN  0
#define SETTING_VALUE    1
#define SETTING_MIN      2
#define SETTING_MAX      3
#define SETTING_DICT     4
#define SETTING_GROUP    5
#define SETTING_ENABLED  6
#define SETTING_UNITS    7
#define SETTING_BGCOLOR  8
#define SETTING_FGCOLOR  9
#define SETTING_STATUS  10
void processSetting(core_t *core, char *settingName, int argIndex, buffer_t *arg)
{
    int len=strlen(settingName);
    dynamic_button_t *button = buttonLookup(core, settingName);

    if (!button)
        return;

    switch (core->settingType)
    {
    case SETTING_DICT:
      if (argIndex==3) // time=0, name=1, type=2, first name =3, first desc=4
          buttonDictionaryClear(button);
      if (argIndex&1)
          buttonDictionaryEntryAppend(button, arg->data);
      else
           buttonDictionaryEntryDescription(button, arg->data);
      break;
    case SETTING_MIN:
        button->minRange = atoi(arg->data);
        break;
    case SETTING_MAX:
        button->maxRange = atoi(arg->data);
        break;
    case SETTING_ENABLED:
        button->enabled = (strcasecmp(arg->data,"true")==0);
        refreshIcons(core);
        break;
    case SETTING_GROUP:
        button->isButton = (strcasecmp(arg->data,"button")==0); // Is it in the button group?
        refreshIcons(core);
        break;
    case SETTING_VALUE:
        if (button->textValue)
            free(button->textValue);
        button->textValue=strdup(arg->data);
        refreshIcons(core);
        break;
    case SETTING_UNITS:
        if (button->units)
            free(button->units);
        button->units=strdup(arg->data);
        refreshIcons(core);
        break;
    case SETTING_BGCOLOR:
        if (button->bgcolor)
            free(button->bgcolor);
        button->bgcolor=strdup(arg->data);
        refreshIcons(core);
        break;
    case SETTING_FGCOLOR:
        if (button->fgcolor)
            free(button->fgcolor);
        button->fgcolor=strdup(arg->data);
        refreshIcons(core);
        break;
    case SETTING_STATUS:
        button->isBad = (strcasecmp(arg->data,"good")!=0);
        refreshIcons(core);
        break;
    }
}

void processCommandArgument(core_t *core, char commandByte, int currentArgIndex, buffer_t *argBuffer)
{
  int dTime;
  // Everything comes prefixed with a timestamp, which is arg# 0
  if (currentArgIndex==0)
  {
    core->cmdTime = atoi(argBuffer->data);
    return;
  }
  
  switch (commandByte)
  {
  case 'H': // Health check ("good" or "bad")
      if (currentArgIndex==1)
          setHealth(core, strncasecmp(argBuffer->data, "good", argBuffer->size)==0);
  
      // Let's request everything in a batch
      if (core->fd!=-1 && core->sentQ==0)
      {
          core->sentQ++;
          printf("<Q\n");
          if (-1==write(core->fd, "\nQ\n", 3))
            processSerialFailure(core);
          else
            fsync(core->fd);
      }
      break;
  case 'I': // Identify with version numbers (should do compatibility info here
      break;
  case 'c': // critical log output
      if (core->criticalText)
          free(core->criticalText);
      core->criticalText = strdup(argBuffer->data);
      setHealth(core, false);
      break;
  case 'i': // Infor log output
  case 'w': // warning log output
  case 'g': // debug log output
      break;
  case 'd': // data
      switch(currentArgIndex)
      {
      case 1: // Pressure
      case 2: // Volume
      case 3: // Tidal Volume
          // Detect milli wrap or core reboot (clear array when millis go back in time)
          dTime = core->cmdTime - core->lastCmdTime;
          core->adjustedMillis += ((dTime>0) ? dTime : 0); // Handle milli wrapping or core restarting gracefully
          core->flCharts[currentArgIndex-1]->add(core->adjustedMillis, atof(argBuffer->data), NULL, charts[currentArgIndex-1].color);
          break;
      default: // Debug output follows
          break;
      }
      break;
   case 'S': // Setting (gets complicated)
       switch(currentArgIndex)
       {
       case 1: // name of setting
           if (core->settingName)
               free(core->settingName);
           core->settingName = strdup(argBuffer->data);
           break;
       case 2: // type of setting response
           if (strcasecmp(argBuffer->data,"value")==0)
               core->settingType = SETTING_VALUE;
           else if (strcasecmp(argBuffer->data,"min")==0)
               core->settingType = SETTING_MIN;
           else if (strcasecmp(argBuffer->data,"max")==0)
               core->settingType = SETTING_MAX;
           else if (strcasecmp(argBuffer->data,"dict")==0)
               core->settingType = SETTING_DICT;
           else if (strcasecmp(argBuffer->data,"group")==0)
               core->settingType = SETTING_GROUP;
           else if (strcasecmp(argBuffer->data,"enabled")==0)
               core->settingType = SETTING_ENABLED;
           else if (strcasecmp(argBuffer->data,"units")==0)
               core->settingType = SETTING_UNITS;
           else if (strcasecmp(argBuffer->data,"bgcolor")==0)
               core->settingType = SETTING_BGCOLOR;
           else if (strcasecmp(argBuffer->data,"fgcolor")==0)
               core->settingType = SETTING_FGCOLOR;
           else if (strcasecmp(argBuffer->data,"status")==0) // Individual buttons can be good/bad
               core->settingType = SETTING_STATUS;
          else
              core->settingType = SETTING_UNKNOWN;
           break;
       default: // Should be type appropriate
          processSetting(core, core->settingName, currentArgIndex, argBuffer);
          break;
       }
       break;
   case 'Q':
       // End of a "query" command, we need to redraw all of the buttons! (Maybe)
       // We get a   Q,<t>,Begin
       // A bunch of S,<t>,....
       // And a      Q,<t>,End
       core->addIcons = (strcasecmp(argBuffer->data,"Begin")==0);
       if (core->addIcons)
           wipeIcons(core);
       else
           refreshIcons(core);
       break;
  }
}

void processCommand(core_t *core)
{
    int state=0;
    char commandByte=0;
    int argumentCount=0;

    static buffer_t argumentBuffer;

    // debug output (ignore data samples and Health checks)
    if (core->streamData.size && (*core->streamData.data!='d' && *core->streamData.data!='H'))
        printf(">%.*s\n", core->streamData.size, core->streamData.data);

   // Echo everything out to the remote monitor
   if (core->remoteFd>=0 && core->streamData.size)
   {
       if (write(core->remoteFd, core->streamData.data, core->streamData.size)<0)
       {
           close(core->remoteFd);
           Fl::remove_fd(core->remoteFd);
           core->remoteFd=-1;
       }
       else
       {
           const char ch[3]="\n\r";
           write(core->remoteFd, ch, 2);
           if (core->streamData.size && (*core->streamData.data!='d'))
             printf("<= %.*s\n", core->streamData.size, core->streamData.data);
       }
   }

    bufferClear(&argumentBuffer);

    // ok, we have a bunch of crap that has encountered a '\n' or '\l'
    // Decode what is in core->buffer
    for (int x=0; x<core->streamData.size; x++)
    {
        char ch = core->streamData.data[x];
        switch (state)
        {
        case 0: // Looking for the last char to make it the command byte
            if (ch==',')
               state++;
            else
               commandByte=ch;
            break;
        case 1: // Looking for an argument
            if (ch==',')
            {
               processCommandArgument(core, commandByte, argumentCount, &argumentBuffer);
               argumentCount++;
               argumentBuffer.size=0;
            }
            else
                bufferAppend(&argumentBuffer, ch);
            break;
        }
    }
    
    // Final argument!
    if (argumentBuffer.size)
        processCommandArgument(core, commandByte, argumentCount, &argumentBuffer);

    // Clear the input buffer
    core->streamData.size=0;
    core->lastCmdTime = core->cmdTime;
}

void processSerialInput(core_t *core, char ch)
{
   // End of line of text, process it
   // TODO: potential Denial of service if we get too much data without ever encountering a \n or \r (imagine opening \dev\zero)
   if (ch=='\n' || ch=='\r')
       processCommand(core);
   else
       bufferAppend(&core->streamData, ch);
}


void makeWindow(core_t *core, int w, int h, const char *label)
{

//    FL_INACTIVE_COLOR - the inactive foreground color

    // dark color theme
    Fl::set_color(FL_BACKGROUND_COLOR, 50, 50, 50); // the default background color
    Fl::set_color(FL_BACKGROUND2_COLOR, 120, 120, 120); // the default background color for text, list, and valuator widgets
    Fl::set_color(FL_FOREGROUND_COLOR, 240, 240, 240); // the default foreground color (0) used for labels and text
    for (int i = 0; i < FL_NUM_GRAY; i++) {
      double min = 0., max = 65.;// 135.;
      int d = (int)(min + i * (max - min) / (FL_NUM_GRAY - 1.));
      Fl::set_color(fl_gray_ramp(i), d, d, d);
    }
    Fl::reload_scheme();
    Fl::set_color(FL_SELECTION_COLOR, 200, 200, 200); // the default selection/highlight color

    core->win = new Fl_Double_Window(0,0, w, h, label);
    Fl_Pack   *packedAll = new Fl_Pack(0,0,w,h);
    Fl_Pack   *packedTop = new Fl_Pack(0,0,w,(h-1)-BUTTON_HEIGHT);
    core->groupedCharts = new My_Group(0,0,w-BUTTON_WIDTH,h-BUTTON_HEIGHT);
    core->titleBox = new Fl_Box(0,0,w-BUTTON_WIDTH,TITLE_HEIGHT,"Version " VERSION_STAMP);
    core->titleBox->box(FL_NO_BOX);
    for (int x=0; x<MAX_CHARTS; x++)
    {
        int chartHeight=(core->groupedCharts->h() - TITLE_HEIGHT)/3;
        core->flCharts[x] = new My_Chart(0, TITLE_HEIGHT+(chartHeight*x), w-BUTTON_WIDTH, chartHeight, charts[x].name);
        core->flCharts[x]->bounds(charts[x].minRange, charts[x].maxRange);
        core->flCharts[x]->threshold(charts[x].waterLevel);
        core->flCharts[x]->thresholdcolor(FL_RED);
        core->flCharts[x]->maxtime(MAX_CHART_TIME); // In milliseconds
        core->flCharts[x]->align(FL_ALIGN_CENTER|FL_ALIGN_TOP|FL_ALIGN_INSIDE);
        core->flCharts[x]->labelsize(16);
    }

    core->scrollButtons = new Fl_Scroll(0,(h-1)-BUTTON_HEIGHT, w, BUTTON_HEIGHT+1);
    core->packedButtons = new Fl_Pack(0,h-BUTTON_HEIGHT,w, BUTTON_HEIGHT);
    core->packedButtons->type(Fl_Pack::HORIZONTAL);
    core->packedButtons->spacing(10);
    core->scrollButtons->add(core->packedButtons);

    core->scrollStatusItems = new Fl_Scroll((w-1)-BUTTON_WIDTH, 0, BUTTON_WIDTH+1, h-BUTTON_HEIGHT);
    core->packedStatusItems = new Fl_Pack(w-BUTTON_WIDTH, 0, BUTTON_WIDTH, h-BUTTON_HEIGHT);
    core->packedStatusItems->type(Fl_Pack::VERTICAL);
    core->packedStatusItems->spacing(10);
    core->packedStatusItems->resizable(core->packedStatusItems);
    core->scrollStatusItems->add(core->packedStatusItems);


    // Put everything in their groups so that the window auto sizes cleanly
    core->groupedCharts->add(core->titleBox);
    for (int x=0;x<MAX_CHARTS; x++)
        core->groupedCharts->add(core->flCharts[x]);
    core->groupedCharts->resizable(core->groupedCharts);
    
    packedTop->type(Fl_Pack::HORIZONTAL);
    packedTop->add(core->groupedCharts);
    packedTop->add(core->scrollStatusItems);
    packedTop->resizable(core->groupedCharts);
    
    packedAll->type(Fl_Pack::VERTICAL);
    packedAll->add(packedTop);
    packedAll->add(core->scrollButtons);
    packedAll->resizable(packedTop);
    core->win->resizable(packedAll);
    core->win->show();
    core->win->fullscreen();
}

void HandleFD(int fd, void *data)
{
  char ch;
  if (1==read(fd, &ch, 1))
      processSerialInput((core_t *)data, ch);
  else
  {
      processSerialFailure((core_t *)data);
  }
}

void openSerial(core_t *core, const char *name)
{
    int fd=-1;
    fd=open(name, O_RDWR);
    if (fd!=-1)
    {
        core->fd=fd;
        core->title = name; // Patient name?  Bed number?
        core->serialPort = name;
        set_interface_attribs (fd, B115200, 0);  // set speed to 11500 bps, 8n1 (no parity)
        Fl::add_fd(fd, HandleFD, (void *)core);
    }
}

#define COSV_SOCKET_PORT 8445
static void broadcast(core_t *core, const char *mess)
{
    struct sockaddr_in s;

    if(core->broadcastSock < 0)
        return;

    memset(&s, '\0', sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = (in_port_t)htons(COSV_SOCKET_PORT);
    s.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if(sendto(core->broadcastSock, mess, strlen(mess), 0, (struct sockaddr *)&s, sizeof(struct sockaddr_in)) < 0)
        perror("sendto");
}

static void Timer_CB(void *data) {              // timer callback
    core_t *core = (core_t *)data;
    if (core->fd==-1)
        openSerial(core, core->serialPort);
    // Looking for *anything* from the core in the past second
    // Health checks are sent out periodically
    if (core->lastCmdTimeChecked == core->cmdTime)
        setHealth(core, false);
    core->lastCmdTimeChecked = core->cmdTime;

    broadcast(core, (core->isGood ? "I,flcosvmon,1,0,0\nH,good\n" : "I,flcosvmon,1,0,0\nH,bad\n"));
    Fl::repeat_timeout(2.0, Timer_CB, data);
}

void ServiceSock(int fd, void *data)
{
    char ch;
    core_t *core = (core_t *)data;

    // Ignore anything coming in, all remote input is ignored
    // Remotes are view-only
    read(fd, &ch, 1);
}

void HandleNewSock(int fd, void *data)
{
    core_t *core = (core_t *)data;
    struct sockaddr_in cli; 
    socklen_t len = sizeof(cli);
  
    // socket create and verification 
    // Accept the data packet from client and verification 
    int connfd = accept(fd, (struct sockaddr *)&cli, &len); 
    if (connfd < 0)
        printf("server acccept failed...\n"); 
    else
    {
        printf("server acccept the client on fd%d...\n", connfd); 
        if (core->remoteFd>=0)
        {
            printf("dropping older remote connection on fd%d...\n", core->remoteFd); 
            close(core->remoteFd);
            Fl::remove_fd(core->remoteFd);
        }
        core->remoteFd = connfd;
        Fl::add_fd(connfd, ServiceSock, (void *)core);
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
            fcntl(connfd, F_SETFL, flags | O_NONBLOCK);
        core->sentQ = 0; // Need to resend the 'Q' to populate the remote client
    }  
}

void createListeningSocket(core_t *core)
{
    int sockfd=-1;
    struct sockaddr_in servaddr; 


    core->broadcastSock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcastEnable=1;
    int ret=setsockopt(core->broadcastSock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1)
        return;
    bzero(&servaddr, sizeof(servaddr)); 
  
  
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(COSV_SOCKET_PORT); 
   // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        close(sockfd);
        sleep(5);
        return;
    } 
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        close(sockfd);
        return;
    }
    printf("Listening for remote servers\n");
    core->listeningSock = sockfd;
    Fl::add_fd(sockfd, HandleNewSock, (void *)core);
}

int main(int argc, char **argv) {
    core_t core;

    memset(&core, 0, sizeof(core));
    core.fd=-1;
    core.remoteFd=-1;
    core.broadcastSock=-1;    

    signal(SIGPIPE, SIG_IGN);

    // TODO: better CLI argument parser
    for (int x=1; x<argc; x++)
        openSerial(&core, argv[x]);
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyS0");
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyACM0");
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyUSB0");
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyAMA0");

    Fl::visual(FL_DOUBLE|FL_INDEX); // dbe support

    makeWindow(&core, 800,480, core.title);

    if (core.fd==-1)
        return 1;

    createListeningSocket(&core);

    Fl::add_timeout(2.0, Timer_CB, (void *)&core);

    return(Fl::run());
}

