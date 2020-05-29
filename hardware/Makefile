# From: https://www.trueelena.org/computers/howto/makefile_for_openscad_projects.html
# Adapted by Steven Carr
#
# This makefile expects an OpenSCAD file named $(PREFIX).scad that defines 
# the following variables:
# 
# PART    -- the name of a part to generate
# VERBOSE -- logically true if you want verbose output (optional)
#
# The OpenSCAD program should have logic that renders a single part by name:
#
# if (PART == "foo") {
#     foo();
# } else if (PART == "bar") {
#     bar();
# } else if (PART == "foo-bar") {
#     foo_bar();
# }
#
# This makefile will use OpenSCAD to create individual model (STL) files and 
# image (PNG) files for each declared part.
#
# So if PREFIX=widget, then the created files will be:
#
# stl/widget-foo.stl stl/widget-bar.stl stl/widget-foo-bar.stl
# png/widget-foo.png png/widget-bar.png png/widget-foo-bar.png
#
#
# In order to pre-define variables, use the -D option.  It can be given
# repeatedly.  Each occurrence of -D must be followed by an assignment. 
# Unlike normal OpenSCAD assignments, these assignments don't define
# variables, but constants, which cannot be changed inside the program, and
# can thus be used to overwrite values defined in the program at export
# time.
#
# If you want to assign the -D variable to another variable, the -D variable
# MUST be initialised in the main .scad program
#
# param1=17;       // must be initialised
# val=param1;      // param1 passed via -D on cmd-line
# echo(val,param1); // outputs 17,17
#
# without the first line, val would be undefined. 
#

# OpenSCAD binary and options
#OPENSCAD=/usr/bin/openscad
OPENSCAD=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=openscad --file-forwarding org.openscad.OpenSCAD 
OPENSCAD_OPTIONS=-DVERBOSE=false -Dparam_complexity=90 -Dparam_path_step=1


# Name of OpenSCAD model file
PREFIX=cosv

#echo param_motor=bldc|nema23-635|nema23|wiper   param_d_shaft=zero|one|two  param_kerf=0.025|0.2

all: test visp laser 


test: cosv.scad
	mkdir -p generated/Test
	python3 makescad.py Test cosv.scad  generated/Test
	for f in generated/Test/*.scad ; do \
	  $(OPENSCAD) $(OPENSCAD_OPTIONS) -o generated/Test/`basename $${f%.scad}.svg` $$f ; \
	  $(OPENSCAD) $(OPENSCAD_OPTIONS) -o generated/Test/`basename $${f%.scad}.dxf` $$f ; \
	  rm $$f ; \
	done

visp: cosv.scad
	mkdir -p generated/VISP
	python3 makescad.py VISP cosv.scad  generated/VISP
	for f in generated/VISP/*.scad ; do \
	  $(OPENSCAD) $(OPENSCAD_OPTIONS) -o generated/VISP/`basename $${f%.scad}.stl` $$f ; \
	  rm $$f ; \
	done

laser: cosv.scad
	mkdir -p generated/Laser
	python3 makescad.py Laser cosv.scad  generated/Laser
	mkdir -p generated/Laser/kerf-0.04 generated/Laser/kerf-0.15
	for f in generated/Laser/*.scad ; do \
	  for k in 0.04 0.15; do \
	    mkdir -p generated/Laser/kerf-$$k ; \
	    $(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_kerf=$$k -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\" -o generated/Laser/kerf-$$k/`basename $${f%.scad}.svg` $$f ; \
	    $(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_kerf=$$k -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\" -o generated/Laser/kerf-$$k/`basename $${f%.scad}.dxf` $$f ; \
	    rm $$f ; \
	  done ; \
	done

clean:
	rm -rf generated
