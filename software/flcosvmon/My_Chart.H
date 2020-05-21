//
// "$Id$"
//
// Forms chart header file for the Fast Light Tool Kit (FLTK).
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

/* \file
   My_Chart widget . */

#ifndef My_Chart_H
#define My_Chart_H

#ifndef Fl_Widget_H
#include <FL/Fl_Widget.H>
#endif

#define FL_CHART_MAX		128	/**< max entries per chart */

/** For internal use only */
struct FL_CHART_ENTRY {
   unsigned millis;                     /**< for internal use only. */
   float val;				/**< For internal use only. */
   unsigned col;			/**< For internal use only. */
   char *str;				/**< For internal use only. */
};

/**
  \class My_Chart
  \brief My_Chart displays simple charts.
  It is provided for Forms compatibility.

  \image html charts.png  
  \image latex charts.png  "My_Chart" width=10cm
 */
class FL_EXPORT My_Chart : public Fl_Widget {
    int numb;
    unsigned maxtime_;
    int sizenumb;
    FL_CHART_ENTRY *entries;
    double min,max;
    double threshold_;
    Fl_Font textfont_;
    Fl_Fontsize textsize_;
    Fl_Color textcolor_;
    Fl_Color thresholdcolor_;
protected:
    void draw();
public:
    My_Chart(int X, int Y, int W, int H, const char *L = 0);

    ~My_Chart();

    void clear();

    void add(unsigned millis, double val, const char *str = 0, unsigned col = 0);

    /**
      Gets the lower and upper bounds of the chart values.
      \param[out] a, b are set to lower, upper
     */
    void bounds(double *a,double *b) const {*a = min; *b = max;}
    void bounds(double a,double b);
    void threshold(double *a) {*a = threshold_;}
    void threshold(double a) {threshold_=a; redraw();}

    /**
      Returns the number of data values in the chart.
     */
    int size() const {return numb;}

    void size(int W, int H) { Fl_Widget::size(W, H); }

    /**
      Gets the maximum amount of time for a chart.
     */
    unsigned maxtime() const {return maxtime_;}
    void maxtime(unsigned m) {maxtime_ = m; redraw(); }

    /** Gets the chart's text font */
    Fl_Font textfont() const {return textfont_;}
    /** Sets the chart's text font to \p s. */
    void textfont(Fl_Font s) {textfont_ = s;}

    /** Gets the chart's text size */
    Fl_Fontsize textsize() const {return textsize_;}
    /** gets the chart's text size to \p s. */
    void textsize(Fl_Fontsize s) {textsize_ = s;}

    /** Gets the chart's text color */
    Fl_Color textcolor() const {return textcolor_;}
    /** gets the chart's text color to \p n. */
    void textcolor(Fl_Color n) {textcolor_ = n;}


    /** Gets the chart's threshold color */
    Fl_Color thresholdcolor() const {return thresholdcolor_;}
    /** gets the chart's text color to \p n. */
    void thresholdcolor(Fl_Color n) {thresholdcolor_ = n;}

};

#endif

//
// End of "$Id$".
//
