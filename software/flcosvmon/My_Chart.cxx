//
// "$Id$"
//
// Forms-compatible chart widget for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#include <FL/math.h>
#include <FL/Fl.H>
#include "My_Chart.H"
#include <FL/fl_draw.H>
#include <stdlib.h>

#define ARCINC	(2.0*M_PI/360.0)

// this function is in fl_boxtype.cxx:
void fl_rectbound(int x,int y,int w,int h, Fl_Color color);

/* Widget specific information */


static void draw_timechart(int x,int y,int w,int h,
			   int numb, FL_CHART_ENTRY entries[],
			   double min, double max, double threshold, unsigned maxtime,
			   Fl_Color textcolor, Fl_Color thresholdcolor)
/* Draws a line chart. x,y,w,h is the bounding box, entries the array of
   numb entries and min and max the boundaries. */
{
  int i;
  double lh = fl_height();
  double incr;
  int W=0, H=0;

  fl_measure("9999", W, H, 0);
  x+=W; // Shift over to make room for the left labels
  w-=W;

  if (max == min) incr = h-2.0*lh;
  else incr = (h-2.0*lh)/ (max-min);
  int zeroh = (int)rint(y+h-lh+min * incr);
  int thresholdh =  zeroh - (int)rint(threshold * incr);
  double pixelsPerMilli = (double)w/(double)(maxtime);
  char str[16];


  // /* Draw base line */
  // fl_color(textcolor);
  // fl_line(x,zeroh,x+w,zeroh);

  /* Draw the threshold */
  if ((threshold>min && threshold<max) && (thresholdh!=zeroh))
  {
    fl_color(thresholdcolor);
    fl_line_style(FL_DASH); // Because of how line styles are implemented on WIN32 systems, you must set the line style after setting the drawing color.
    fl_line(x,thresholdh,x+w,thresholdh);
    fl_line_style(FL_SOLID);
    snprintf(str,sizeof(str), "%.0f", threshold);
    fl_draw(str, x, thresholdh ,0,0,  FL_ALIGN_CENTER|FL_ALIGN_RIGHT);
  }

  /* Draw the left scale */
  // Draw min, max, and zero if it is not min or max
  
  fl_color(textcolor);
  snprintf(str,sizeof(str), "%.0f", min);
  fl_draw(str, x,  zeroh - (int)rint(min*incr),0,0,  FL_ALIGN_CENTER|FL_ALIGN_RIGHT);
  snprintf(str,sizeof(str), "%.0f", max);
  fl_draw(str, x,  zeroh - (int)rint(max*incr),0,0,  FL_ALIGN_CENTER|FL_ALIGN_RIGHT);
  if (min!=0.0)
  {
    snprintf(str,sizeof(str), "0");
    fl_draw(str, x,  zeroh,0,0,  FL_ALIGN_CENTER|FL_ALIGN_RIGHT);
  }


  /* Draw the bottom scale */
  // ok, we are going to work backwards in time
  // -15 on the left, and 0 on the right.  
  // The issue, is that the elements are inserted in a list, and are not
  // exactly equal in time apart.  

  fl_color(textcolor);
  for (i=(maxtime-1000); i>0; i-=1000)
  {
      unsigned theTime = maxtime-i;
      snprintf(str,sizeof(str),"-%d",i/1000);
      fl_draw(str,
              x + (int)rint((theTime)*pixelsPerMilli),
              (y+h),
              0,0, FL_ALIGN_BOTTOM);
    
  }

  /* Draw the values */
  if (numb)
  {
    unsigned latestMillis = entries[numb-1].millis;
    for (i=1; i<numb; i++) {
      int x0 = x + (int)rint((maxtime-(latestMillis-entries[i-1].millis))*pixelsPerMilli);
      int x1 = x + (int)rint((maxtime-(latestMillis-entries[i].millis))*pixelsPerMilli);
      int yy0 = zeroh - (int)rint(entries[i-1].val*incr);
      int yy1 = zeroh - (int)rint(entries[i].val*incr);

      fl_color((Fl_Color)entries[i-1].col);
      fl_line(x0,yy0,x1,yy1);
    }
  }

  /* Draw the labels */
  fl_color(textcolor);
  if (numb)
  {
    unsigned latestMillis = entries[numb-1].millis;
    for (i=1; i<numb; i++)
      if (entries[i].str)
        fl_draw(entries[i].str,
                x + (int)rint((maxtime-(latestMillis-entries[i].millis))*pixelsPerMilli),
  	        zeroh - (int)rint(entries[i].val*incr),0,0,
	        entries[i].val>=0 ? FL_ALIGN_BOTTOM : FL_ALIGN_TOP);
  }

  fl_color(FL_MAGENTA);

}

void My_Chart::draw() {

    draw_box();
    Fl_Boxtype b = box();
    int xx = x()+Fl::box_dx(b); // was 9 instead of dx...
    int yy = y()+Fl::box_dy(b);
    int ww = w()-Fl::box_dw(b);
    int hh = h()-Fl::box_dh(b);
    fl_push_clip(xx, yy, ww, hh);

    ww--; hh--; // adjust for line thickness

    if (min >= max) {
	min = max = 0.0;
	for (int i=0; i<numb; i++) {
	    if (entries[i].val < min) min = entries[i].val;
	    if (entries[i].val > max) max = entries[i].val;
	}
    }

    fl_font(textfont(),textsize());
    draw_label();

    draw_timechart(xx,yy,ww,hh, numb, entries, min, max, threshold_,
                   maxtime_, textcolor(), thresholdcolor());


    fl_pop_clip();
}

/*------------------------------*/

#define FL_CHART_BOXTYPE	FL_BORDER_BOX
#define FL_CHART_COL1		FL_COL1
#define FL_CHART_LCOL		FL_LCOL
#define FL_CHART_ALIGN		FL_ALIGN_BOTTOM

/**
  Create a new My_Chart widget using the given position, size and label string.
  The default boxstyle is \c FL_NO_BOX.
  \param[in] X, Y, W, H position and size of the widget
  \param[in] L widget label, default is no label
 */
My_Chart::My_Chart(int X, int Y, int W, int H,const char *L) :
Fl_Widget(X,Y,W,H,L) {
  box(FL_BORDER_BOX);
  align(FL_ALIGN_BOTTOM);
  numb       = 0;
  maxtime_   = 15000; // in milliseconds
  sizenumb   = FL_CHART_MAX;
  min = max  = 0;
  threshold_ = 0;
  textfont_  = FL_HELVETICA;
  textsize_  = 10;
  textcolor_ = FL_FOREGROUND_COLOR;
  thresholdcolor_ = FL_FOREGROUND_COLOR;
  entries    = (FL_CHART_ENTRY *)calloc(sizeof(FL_CHART_ENTRY), FL_CHART_MAX + 1);
}

/**
  Destroys the My_Chart widget and all of its data.
 */
My_Chart::~My_Chart() {
  free(entries);
}

/**
  Removes all values from the chart.
 */
void My_Chart::clear() {
  numb = 0;
  min = max = 0;
  redraw();
}

/**
  Add the data value \p val with optional label \p str and color \p col
  to the chart.
  \param[in] millis milliseconds since core startup
  \param[in] val data value
  \param[in] str optional data label
  \param[in] col optional data color
 */
void My_Chart::add(unsigned millis, double val, const char *str, unsigned col) {
  /* Allocate more entries if required */
  if (numb >= sizenumb) {
    sizenumb += FL_CHART_MAX;
    entries = (FL_CHART_ENTRY *)realloc(entries, sizeof(FL_CHART_ENTRY) * (sizenumb + 1));
  }

  // Shift entries as needed (we have a max time for this chart)
  while (numb && ((entries[0].millis)+(maxtime_)) <= millis) {
    if (entries[0].str)
      free(entries[0].str);
    memmove(entries, entries + 1, sizeof(FL_CHART_ENTRY) * (numb - 1));
    numb --;
  }
  entries[numb].millis = millis;
  entries[numb].val = float(val);
  entries[numb].col = col;
  entries[numb].str = (str ? strdup(str) : NULL);
  numb++;
  redraw();
}

/**
  Sets the lower and upper bounds of the chart values.
  \param[in] a, b are used to set lower, upper
 */
void My_Chart::bounds(double a, double b) {
  this->min = a;
  this->max = b;
  redraw();
}


//
// End of "$Id$".
//
