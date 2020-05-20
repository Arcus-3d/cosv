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


static void draw_linechart(int x,int y,int w,int h,
			   int numb, FL_CHART_ENTRY entries[],
			   double min, double max, int autosize, int maxnumb,
			   Fl_Color textcolor)
/* Draws a line chart. x,y,w,h is the bounding box, entries the array of
   numb entries and min and max the boundaries. */
{
  int i;
  double lh = fl_height();
  double incr;
  if (max == min) incr = h-2.0*lh;
  else incr = (h-2.0*lh)/ (max-min);
  int zeroh = (int)rint(y+h-lh+min * incr);
  double bwidth = w/double(autosize?numb:maxnumb);
  /* Draw the values */
  for (i=0; i<numb; i++) {
      int x0 = x + (int)rint((i-.5)*bwidth);
      int x1 = x + (int)rint((i+.5)*bwidth);
      int yy0 = i ? zeroh - (int)rint(entries[i-1].val*incr) : 0;
      int yy1 = zeroh - (int)rint(entries[i].val*incr);

      fl_color((Fl_Color)entries[i-1].col);
      fl_line(x0,yy0,x1,yy1);
  }
//  /* Draw base line */
//  fl_color(textcolor);
//  fl_line(x,zeroh,x+w,zeroh);

  /* Draw the labels */
  for (i=0; i<numb; i++)
      fl_draw(entries[i].str,
	      x+(int)rint((i+.5)*bwidth), zeroh - (int)rint(entries[i].val*incr),0,0,
	      entries[i].val>=0 ? FL_ALIGN_BOTTOM : FL_ALIGN_TOP);  

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

      draw_linechart(xx,yy,ww,hh, numb, entries, min, max,
                      autosize(), maxnumb, textcolor());


    draw_label();
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
  maxnumb    = 0;
  sizenumb   = FL_CHART_MAX;
  autosize_  = 1;
  min = max  = 0;
  textfont_  = FL_HELVETICA;
  textsize_  = 10;
  textcolor_ = FL_FOREGROUND_COLOR;
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
  \param[in] val data value
  \param[in] str optional data label
  \param[in] col optional data color
 */
void My_Chart::add(double val, const char *str, unsigned col) {
  /* Allocate more entries if required */
  if (numb >= sizenumb) {
    sizenumb += FL_CHART_MAX;
    entries = (FL_CHART_ENTRY *)realloc(entries, sizeof(FL_CHART_ENTRY) * (sizenumb + 1));
  }
  // Shift entries as needed
  if (numb >= maxnumb && maxnumb > 0) {
    memmove(entries, entries + 1, sizeof(FL_CHART_ENTRY) * (numb - 1));
    numb --;
  }
  entries[numb].val = float(val);
  entries[numb].col = col;
    if (str) {
	strncpy(entries[numb].str,str,FL_CHART_LABEL_MAX + 1);
    } else {
	entries[numb].str[0] = 0;
    }
  numb++;
  redraw();
}

/**
  Inserts a data value \p val at the given position \p ind.
  Position 1 is the first data value.
  \param[in] ind insertion position
  \param[in] val data value
  \param[in] str optional data label
  \param[in] col optional data color
 */
void My_Chart::insert(int ind, double val, const char *str, unsigned col) {
  int i;
  if (ind < 1 || ind > numb+1) return;
  /* Allocate more entries if required */
  if (numb >= sizenumb) {
    sizenumb += FL_CHART_MAX;
    entries = (FL_CHART_ENTRY *)realloc(entries, sizeof(FL_CHART_ENTRY) * (sizenumb + 1));
  }
  // Shift entries as needed
  for (i=numb; i >= ind; i--) entries[i] = entries[i-1];
  if (numb < maxnumb || maxnumb == 0) numb++;
  /* Fill in the new entry */
  entries[ind-1].val = float(val);
  entries[ind-1].col = col;
  if (str) {
      strncpy(entries[ind-1].str,str,FL_CHART_LABEL_MAX+1);
  } else {
      entries[ind-1].str[0] = 0;
  }
  redraw();
}

/**
  Replace a data value \p val at the given position \p ind.
  Position 1 is the first data value.
  \param[in] ind insertion position
  \param[in] val data value
  \param[in] str optional data label
  \param[in] col optional data color
 */
void My_Chart::replace(int ind,double val, const char *str, unsigned col) {
  if (ind < 1 || ind > numb) return;
  entries[ind-1].val = float(val);
  entries[ind-1].col = col;
  if (str) {
      strncpy(entries[ind-1].str,str,FL_CHART_LABEL_MAX+1);
  } else {
      entries[ind-1].str[0] = 0;
  }
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

/**
  Set the maximum number of data values for a chart.
  If you do not call this method then the chart will be allowed to grow
  to any size depending on available memory.
  \param[in] m maximum number of data values allowed.
 */
void My_Chart::maxsize(int m) {
  int i;
  /* Fill in the new number */
  if (m < 0) return;
  maxnumb = m;
  /* Shift entries if required */
  if (numb > maxnumb) {
      for (i = 0; i<maxnumb; i++)
	  entries[i] = entries[i+numb-maxnumb];
      numb = maxnumb;
      redraw();
  }
}

//
// End of "$Id$".
//
