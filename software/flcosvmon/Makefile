VERSION:=$(shell git rev-parse HEAD)
DATE:=$(shell date +%F)

flcosvmon: flcosvmon.cpp My_Chart.cxx My_Chart.H Makefile
	g++ -g3 -o flcosvmon flcosvmon.cpp My_Chart.cxx `fltk-config --cxxflags --ldflags --libs` -DVERSION_STAMP='"Built on $(DATE) git $(VERSION)"'
