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
OPENSCAD_OPTIONS=-DVERBOSE=false -Dparam_motor=\"bldc\" -Dparam_complexity=90


# Name of OpenSCAD model file
PREFIX=cosv
# ----- Everything after this should not need modification

# Names of parts to build
PARTS=$(shell grep 'PART == ' $(PREFIX).scad | cut -d'"' -f2)
PARTLS=$(shell grep 'PARTL == ' $(PREFIX).scad | cut -d'"' -f2)


MODELS=$(patsubst %,generated/FFF/$(PREFIX)-%.stl,$(PARTS))

IMAGES1=$(patsubst %,generated/Laser/kerf-0.2/$(PREFIX)-%.svg,$(PARTLS))
IMAGES2=$(patsubst %,generated/Laser/kerf-0.025/$(PREFIX)-%.svg,$(PARTLS))
IMAGES3=$(patsubst %,generated/Laser/kerf-0.2/$(PREFIX)-%.dxf,$(PARTLS))
IMAGES4=$(patsubst %,generated/Laser/kerf-0.025/$(PREFIX)-%.dxf,$(PARTLS))

all: models images

directories:
	@mkdir -p generated/FFF generated/Laser/kerf-0.2 generated/Laser/kerf-0.025

models: directories $(MODELS)

images: images1 images2 images3 images4

images1: directories $(IMAGES1)

images2: directories $(IMAGES2)

images3: directories $(IMAGES3)

images4: directories $(IMAGES4)

clean:
	rm -rf generated

# Dependencies for FFF models

$(MODELS) : generated/FFF/$(PREFIX)-%.stl : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .stl,,$(@F)))\" -Dparam_partl=\"\"  $<

# Dependencies for Laser SVG images

$(IMAGES1) : generated/Laser/kerf-0.2/$(PREFIX)-%.svg : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"\" -Dparam_partl=\"$(subst $(PREFIX)-,,$(subst .svg,,$(@F)))\" -Dparam_kerf=0.2 $(IMAGE_OPTIONS) $<

$(IMAGES2) : generated/Laser/kerf-0.025/$(PREFIX)-%.svg : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"\" -Dparam_partl=\"$(subst $(PREFIX)-,,$(subst .svg,,$(@F)))\" -Dparam_kerf=0.025 $(IMAGE_OPTIONS) $<

# Dependencies for Laser DXF images
$(IMAGES3) : generated/Laser/kerf-0.2/$(PREFIX)-%.dxf : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"\" -Dparam_partl=\"$(subst $(PREFIX)-,,$(subst .dxf,,$(@F)))\" -Dparam_kerf=0.2 $(IMAGE_OPTIONS) $<

$(IMAGES4) : generated/Laser/kerf-0.025/$(PREFIX)-%.dxf : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"\" -Dparam_partl=\"$(subst $(PREFIX)-,,$(subst .dxf,,$(@F)))\" -Dparam_kerf=0.025 $(IMAGE_OPTIONS) $<

