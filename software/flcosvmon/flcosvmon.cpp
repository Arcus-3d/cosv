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


#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Button.H>
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


#include <string.h>
#include <termios.h>
#include <unistd.h>


#define MAX_CHARTS 3
#define MAX_CHART_SIZE 40*60 /* 1 minute on the display */

#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 60


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
{"Pressure", -10, 40, 40, FL_YELLOW},
{"Volume", -80, 80, 80, FL_GREEN},
{"Tidal Volume", 0, 800, 600, FL_BLUE}
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
    dictionary_t *dictionary; // For dictionary types
    char *textValue; // For dictionary types
    char *units;     // Units told to us by the core
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
    buffer_t streamData;
    int cmdTime; // 32-bit millis() since the core started
    int lastCmdTime; // 32-bit millis() since the core started
    char *settingName; // For use in command processing strdup()'ed
    int settingType;
    int sentQ;
    dynamic_button_t *button_list;

    Fl_Pack *packedButtons;
    Fl_Pack *packedStatusItems;

    My_Chart *flCharts[MAX_CHARTS];

} core_t;


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

void setHealth(int isGood)
{
  // if !isGood, big red X on screen, or red banner, whatever
}

dynamic_button_t *buttonLookup(core_t *core, char *settingName)
{
    for (dynamic_button_t *ptr=core->button_list; ptr; ptr=ptr->next)
    {
        if (strcasecmp(settingName,ptr->name)==0)
             return ptr;
    }
    
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

class MyPopupWindow : public Fl_Double_Window {
public:
    MyPopupWindow(int X,int Y,int W,int H, const char* title) : Fl_Double_Window (X, Y, W, H, title) {}

    int handle(int e) {
        switch(e) {
            case FL_FOCUS:
                break;
            case FL_UNFOCUS:
                if (!hasFocus((Fl_Pack *)this->child(0)))
                  Fl::delete_widget(this);
                break;
        }
        return(Fl_Double_Window::handle(e));
    }
};



void sliderChanged(Fl_Widget *w, void *data)
{
    core_t *core = (core_t *)data;
    const char *bname = (const char *)w->parent()->user_data();

    if (core->fd!=-1)
    {
      char *buf;
      int len=asprintf(&buf, "S,%s,%d\n", bname, (int)((Fl_Value_Slider *)w)->value());
      printf("<%s", buf);
      write(core->fd, buf, len);
      fsync(core->fd);      
      free(buf);  
    }
}


// Simple slider bar set to current strtoul(b->textValue); with ninRange/maxRange values
void popupRangeSelection(core_t *core, dynamic_button_t *b)
{
  MyPopupWindow *popup = new MyPopupWindow(0,0,300, BUTTON_HEIGHT, b->name);

  Fl_Value_Slider *slider = new Fl_Value_Slider(0,0,300,BUTTON_HEIGHT);
  slider->bounds(b->minRange, b->maxRange);
  slider->type(FL_HOR_NICE_SLIDER);
  slider->value(atoi(b->textValue));
  popup->user_data(b->name);
  slider->callback(sliderChanged, (void *)core);
  slider->step(1);
  popup->position((Fl::w() - popup->w())/2, (Fl::h() - popup->h())/2);
  popup->show();
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
      write(core->fd, buf, strlen(buf));
      fsync(core->fd);      
    }
    free(buf);
    // Tell the window to hide
    w->parent()->parent()->hide();
}

// Generate a popup window that is dictionary based
void popupDictionarySelection(core_t *core, dynamic_button_t *b)
{
  int count=0;
  for (dictionary_t *d=b->dictionary; d; d=d->next)
    count++;

  if (count)
  {
    MyPopupWindow *popup = new MyPopupWindow(0,0,300, count*BUTTON_HEIGHT, b->name);
    Fl_Pack *packer = new Fl_Pack(0,0,300, count*BUTTON_HEIGHT);
    packer->type(Fl_Pack::VERTICAL);
    for (dictionary_t *d=b->dictionary; d; d=d->next)
    {
      Fl_Button *button = new Fl_Button(0,0,BUTTON_WIDTH, BUTTON_HEIGHT, d->name);
      button->tooltip(d->description);
      packer->user_data(b->name);
      button->callback(popupSelected, (void *)core);
    }
    popup->position((Fl::w() - popup->w())/2, (Fl::h() - popup->h())/2);
    popup->show();
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



void refreshIcons(core_t *core)
{
    for (dynamic_button_t *b=core->button_list; b; b=b->next)
    {
        if (b->flButton==NULL)
        {
            b->flButton = new My_Button(0,0,BUTTON_WIDTH,BUTTON_HEIGHT);

            b->flButton->box(FL_RFLAT_BOX);    // buttons won't have 'edges'
            b->flButton->color(fl_rgb_color(120,120,120));
            b->flButton->labelcolor(fl_rgb_color( 174, 214, 241 ));
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
        }

        b->flButton->name(b->name);
        b->flButton->label(b->textValue);
        b->flButton->units(b->units);

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


#define SETTING_UNKNOWN 0
#define SETTING_VALUE   1
#define SETTING_MIN     2
#define SETTING_MAX     3
#define SETTING_DICT    4
#define SETTING_GROUP   5
#define SETTING_ENABLED 6
#define SETTING_UNITS   7

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
    }
}

void processCommandArgument(core_t *core, char commandByte, int currentArgIndex, buffer_t *argBuffer)
{
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
          setHealth(strncasecmp(argBuffer->data, "good", argBuffer->size)==0);

      // Let's request everything in a batch
      if (core->sentQ==0)
      {
          core->sentQ++;
          printf("<Q\n");
          write(core->fd, "\nQ\n", 3);
          fsync(core->fd);
      }
      break;
  case 'I': // Identify with version numbers (should do compatibility info here
      break;
  case 'c': // critical log output
      // Should go titlebar?
      // and get cleared on "good" health...
      break;
  case 'i': // Infor log output
  case 'w': // warning log output
  case 'g': // debug log output
      break;
  case 'd': // data
      if (core->sentQ)
      switch(currentArgIndex)
      {
      case 1: // Pressure
      case 2: // Volume
      case 3: // Tidal Volume
          // Detect milli wrap or core reboot (clear array when millis go back in time)
          if (core->lastCmdTime > core->cmdTime)
              core->flCharts[currentArgIndex-1]->clear();
          core->flCharts[currentArgIndex-1]->add(core->cmdTime, atof(argBuffer->data), NULL, charts[currentArgIndex-1].color);
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
       refreshIcons(core);
       break;
  }
}

void processCommand(core_t *core)
{
    int state=0;
    char commandByte=0;
    int argumentCount=0;
    buffer_t argumentBuffer;

    // debug output (ignore data samples for now)
    if (core->streamData.size && *core->streamData.data!='d')
        printf(">%.*s\n", core->streamData.size, core->streamData.data);

    memset(&argumentBuffer, 0, sizeof(argumentBuffer));

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


    Fl_Double_Window *win = new Fl_Double_Window(w, h, label);
    Fl_Pack   *packedAll = new Fl_Pack(0,0,w,h);
    Fl_Pack   *packedTop = new Fl_Pack(0,0,w,(h-1)-BUTTON_HEIGHT);
    Fl_Group  *groupedCharts = new Fl_Group(0,0,w-BUTTON_WIDTH,h-BUTTON_HEIGHT);
    
    for (int x=0; x<MAX_CHARTS; x++)
    {
        core->flCharts[x] = new My_Chart(0, ((h-BUTTON_HEIGHT)/3)*x, w-BUTTON_WIDTH, ((h-BUTTON_HEIGHT)/3), charts[x].name);
        core->flCharts[x]->bounds(charts[x].minRange, charts[x].maxRange);
        core->flCharts[x]->threshold(charts[x].waterLevel);
        core->flCharts[x]->thresholdcolor(FL_RED);
        core->flCharts[x]->maxtime(15000); // In milliseconds
        core->flCharts[x]->align(FL_ALIGN_CENTER|FL_ALIGN_TOP|FL_ALIGN_INSIDE);
        core->flCharts[x]->labelsize(16);
    }

    Fl_Scroll *scrollButtons = new Fl_Scroll(0,(h-1)-BUTTON_HEIGHT, w, BUTTON_HEIGHT+1);
    core->packedButtons = new Fl_Pack(0,h-BUTTON_HEIGHT,w, BUTTON_HEIGHT);
    core->packedButtons->type(Fl_Pack::HORIZONTAL);
    core->packedButtons->spacing(10);
    scrollButtons->add(core->packedButtons);

    Fl_Scroll *scrollStatusItems = new Fl_Scroll((w-1)-BUTTON_WIDTH, 0, BUTTON_WIDTH+1, h-BUTTON_HEIGHT);
    core->packedStatusItems = new Fl_Pack(w-BUTTON_WIDTH, 0, BUTTON_WIDTH, h-BUTTON_HEIGHT);
    core->packedStatusItems->type(Fl_Pack::VERTICAL);
    core->packedStatusItems->spacing(10);
    core->packedStatusItems->resizable(core->packedStatusItems);
    scrollStatusItems->add(core->packedStatusItems);


    // Put everything in their groups so that the window auto sizes cleanly
    for (int x=0;x<MAX_CHARTS; x++)
        groupedCharts->add(core->flCharts[x]);
    groupedCharts->resizable(groupedCharts);
    
    packedTop->type(Fl_Pack::HORIZONTAL);
    packedTop->add(groupedCharts);
    packedTop->add(scrollStatusItems);
    packedTop->resizable(groupedCharts);
    
    packedAll->type(Fl_Pack::VERTICAL);
    packedAll->add(packedTop);
    packedAll->add(scrollButtons);
    packedAll->resizable(packedTop);
    win->resizable(packedAll);
    win->position((Fl::w() - w)/2, (Fl::h() - h)/2);
    win->show();
}

void HandleFD(int fd, void *data)
{
  char ch;
  if (1==read(fd, &ch, 1))
      processSerialInput((core_t *)data, ch);
  else
  {
      Fl::remove_fd(fd);
      close(fd);
  }
}

void openSerial(core_t *core, const char *name)
{
    int fd=-1;
    fd=open(name, O_RDWR);
    if (fd!=-1)
    {
        core->fd=fd;
        core->title = name;
        set_interface_attribs (fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
        Fl::add_fd(fd, HandleFD, (void *)core);
    }
}

int main(int argc, char **argv) {
    core_t core;

    memset(&core, 0, sizeof(core));
    core.fd=-1;


    // TODO: better CLI argument parser
    for (int x=1; x<argc; x++)
        openSerial(&core, argv[x]);
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyACM0");
    if (core.fd==-1)
        openSerial(&core,"/dev/ttyUSB0");

    makeWindow(&core, 800,480, core.title);

    if (core.fd==-1)
        return 1;

    return(Fl::run());
}

