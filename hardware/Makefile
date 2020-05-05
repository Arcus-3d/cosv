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
# ----- Everything after this should not need modification

# Names of parts to build
PARTS = \
	paddle \
	base_t \
	bldc_motor_standoff \
	bag_mount \
	arm_l \
	supply_cover \
	chest_bar \
	flow_sensor_for_pcb \
	flow_sensor_venturi_for_pcb \
	flow_sensor_for_pcb_no_pocket \
	flow_sensor_cover_oled \
	flow_sensor_cover \
	flow_sensor_test \
	bearing_bushing \
	bearing_washer

PARTLS = \
	laser_callibration_square_10x10mm \
	laser_arm_mount \
	laser_bearing_washer \
	laser_bearing_bushing \
	laser_bldc_motor_standoff \
	laser_base_t \
	laser_paddle \
	laser_arm \
	laser_bag_mount


MODELS=$(patsubst %,generated/FFF/$(PREFIX)-%.stl,$(PARTS))

IMAGES1=$(patsubst %,generated/Laser/kerf-0.2/$(PREFIX)-%.svg,$(PARTLS))
IMAGES2=$(patsubst %,generated/Laser/kerf-0.025/$(PREFIX)-%.svg,$(PARTLS))
IMAGES3=$(patsubst %,generated/Laser/kerf-0.2/$(PREFIX)-%.dxf,$(PARTLS))
IMAGES4=$(patsubst %,generated/Laser/kerf-0.025/$(PREFIX)-%.dxf,$(PARTLS))

all: models images

directories:
	@mkdir -p generated/FFF generated/Laser/kerf-0.2 generated/Laser/kerf-0.025

# For FFM 3D printers
models: directories $(MODELS) cams bases

cams: directories $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"bldc\"       -Dparam_d_shaft=\"zero\  -o generated/FFF/cam_6mm.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"bldc\"       -Dparam_d_shaft=\"one\"  -o generated/FFF/cam_6mm_d.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\  -o generated/FFF/cam_6.35mm_d.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\"  -o generated/FFF/cam_6.35mm.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"nema23\"     -Dparam_d_shaft=\"zero\" -o generated/FFF/cam_8mm.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"nema23\"     -Dparam_d_shaft=\"one\"  -o generated/FFF/cam_8mm_d.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"cam\"  -Dparam_motor=\"wiper\"      -Dparam_d_shaft=\"two\"  -o generated/FFF/cam_12mm_double_d.stl $(PREFIX).scad

bases: directories $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"base_b\"  -Dparam_motor=\"nema23-635\" -o generated/FFF/base_b_nema23.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"base_b\"  -Dparam_motor=\"nema23\"     -o generated/FFF/base_b_nema23.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"base_b\"  -Dparam_motor=\"bldc\"       -o generated/FFF/base_b_bldc.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"base_b\"  -Dparam_motor=\"worm\"       -o generated/FFF/base_b_worm.stl $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"base_b\"  -Dparam_motor=\"wiper\"      -o generated/FFF/base_b_wiper.stl $(PREFIX).scad

$(MODELS) : generated/FFF/$(PREFIX)-%.stl : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .stl,,$(@F)))\"  $<
	

# For laser cutters
images: lcams lbases images1 images2 images3 images4

images1: directories $(IMAGES1)

images2: directories $(IMAGES2)

images3: directories $(IMAGES3)

images4: directories $(IMAGES4)

clean:
	rm -rf generated


# Dependencies for Laser SVG images

$(IMAGES1) : generated/Laser/kerf-0.2/$(PREFIX)-%.svg : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .svg,,$(@F)))\" -Dparam_kerf=0.2 $(IMAGE_OPTIONS) $<

$(IMAGES2) : generated/Laser/kerf-0.025/$(PREFIX)-%.svg : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .svg,,$(@F)))\" -Dparam_kerf=0.025 $(IMAGE_OPTIONS) $<

# Dependencies for Laser DXF images
$(IMAGES3) : generated/Laser/kerf-0.2/$(PREFIX)-%.dxf : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .dxf,,$(@F)))\" -Dparam_kerf=0.2 $(IMAGE_OPTIONS) $<

$(IMAGES4) : generated/Laser/kerf-0.025/$(PREFIX)-%.dxf : $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -o $@ -Dparam_part=\"$(subst $(PREFIX)-,,$(subst .dxf,,$(@F)))\" -Dparam_kerf=0.025 $(IMAGE_OPTIONS) $<


lcams: directories $(PREFIX).scad
	echo "I'm too lazy today to automate this in a loop"
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"      -o generated/Laser/kerf-0.025/cam_6mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"       -o generated/Laser/kerf-0.025/cam_6mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" -o generated/Laser/kerf-0.025/cam_6.35mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\" -o generated/Laser/kerf-0.025/cam_6.35mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"  -o generated/Laser/kerf-0.025/cam_8mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"     -o generated/Laser/kerf-0.025/cam_8mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"      -o generated/Laser/kerf-0.025/cam_12mm_double_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"    -o generated/Laser/kerf-0.2/cam_6mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"       -o generated/Laser/kerf-0.2/cam_6mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" generated/Laser/kerf-0.2/cam_6.35mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\" -o generated/Laser/kerf-0.2/cam_6.35mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"  -o generated/Laser/kerf-0.2/cam_8mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"     -o generated/Laser/kerf-0.2/cam_8mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"      -o generated/Laser/kerf-0.2/cam_12mm_double_d.svg $(PREFIX).scad
	echo "Now do the DXF's"
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"       -o generated/Laser/kerf-0.025/cam_6mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"        -o generated/Laser/kerf-0.025/cam_6mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" -o generated/Laser/kerf-0.025/cam_6.35mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\"  -o generated/Laser/kerf-0.025/cam_6.35mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"     -o generated/Laser/kerf-0.025/cam_8mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"      -o generated/Laser/kerf-0.025/cam_8mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"       -o generated/Laser/kerf-0.025/cam_12mm_double_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"         -o generated/Laser/kerf-0.2/cam_6mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"          -o generated/Laser/kerf-0.2/cam_6mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\"   -o generated/Laser/kerf-0.2/cam_6.35mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\"    -o generated/Laser/kerf-0.2/cam_6.35mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"       -o generated/Laser/kerf-0.2/cam_8mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"        -o generated/Laser/kerf-0.2/cam_8mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"         -o generated/Laser/kerf-0.2/cam_12mm_double_d.dxf $(PREFIX).scad
	echo "Laser cam centers"
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"      -o generated/Laser/kerf-0.025/cam_center_6mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"       -o generated/Laser/kerf-0.025/cam_center_6mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" -o generated/Laser/kerf-0.025/cam_center_6.35mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\" -o generated/Laser/kerf-0.025/cam_center_6.35mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"  -o generated/Laser/kerf-0.025/cam_center_8mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"     -o generated/Laser/kerf-0.025/cam_center_8mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"      -o generated/Laser/kerf-0.025/cam_center_12mm_double_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"    -o generated/Laser/kerf-0.2/cam_center_6mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"       -o generated/Laser/kerf-0.2/cam_center_6mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" generated/Laser/kerf-0.2/cam_center_6.35mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\" -o generated/Laser/kerf-0.2/cam_center_6.35mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"  -o generated/Laser/kerf-0.2/cam_center_8mm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"     -o generated/Laser/kerf-0.2/cam_center_8mm_d.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"      -o generated/Laser/kerf-0.2/cam_center_12mm_double_d.svg $(PREFIX).scad
	echo "Now do the DXF's"
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"       -o generated/Laser/kerf-0.025/cam_center_6mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"        -o generated/Laser/kerf-0.025/cam_center_6mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\" -o generated/Laser/kerf-0.025/cam_center_6.35mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\"  -o generated/Laser/kerf-0.025/cam_center_6.35mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"     -o generated/Laser/kerf-0.025/cam_center_8mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"      -o generated/Laser/kerf-0.025/cam_center_8mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"       -o generated/Laser/kerf-0.025/cam_center_12mm_double_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"zero\"         -o generated/Laser/kerf-0.2/cam_center_6mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\" -Dparam_d_shaft=\"one\"          -o generated/Laser/kerf-0.2/cam_center_6mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"zero\"   -o generated/Laser/kerf-0.2/cam_center_6.35mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"one\"    -o generated/Laser/kerf-0.2/cam_center_6.35mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"zero\"       -o generated/Laser/kerf-0.2/cam_center_8mm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\" -Dparam_d_shaft=\"one\"        -o generated/Laser/kerf-0.2/cam_center_8mm_d.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_cam_center\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\" -Dparam_d_shaft=\"two\"         -o generated/Laser/kerf-0.2/cam_center_12mm_double_d.dxf $(PREFIX).scad


lbases: directories $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -Dparam_d_shaft=\"auto\" -o generated/Laser/kerf-0.025/laser_base_b_nema23.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\"     -o generated/Laser/kerf-0.025/laser_base_b_nema23.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\"       -o generated/Laser/kerf-0.025/laser_base_b_bldc.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"worm\"       -o generated/Laser/kerf-0.025/laser_base_b_worm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\"      -o generated/Laser/kerf-0.025/laser_base_b_wiper.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -o generated/Laser/kerf-0.2/laser_base_b_nema23.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\"     -o generated/Laser/kerf-0.2/laser_base_b_nema23.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\"       -o generated/Laser/kerf-0.2/laser_base_b_bldc.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"worm\"       -o generated/Laser/kerf-0.2/laser_base_b_worm.svg $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\"      -o generated/Laser/kerf-0.2/laser_base_b_wiper.svg $(PREFIX).scad
	echo "now do the dxf's"
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23-635\" -o generated/Laser/kerf-0.025/laser_base_b_nema23.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"nema23\"     -o generated/Laser/kerf-0.025/laser_base_b_nema23.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"bldc\"       -o generated/Laser/kerf-0.025/laser_base_b_bldc.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"worm\"       -o generated/Laser/kerf-0.025/laser_base_b_worm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.025 -Dparam_motor=\"wiper\"      -o generated/Laser/kerf-0.025/laser_base_b_wiper.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23-635\" -o generated/Laser/kerf-0.2/laser_base_b_nema23.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"nema23\"     -o generated/Laser/kerf-0.2/laser_base_b_nema23.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"bldc\"       -o generated/Laser/kerf-0.2/laser_base_b_bldc.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"worm\"       -o generated/Laser/kerf-0.2/laser_base_b_worm.dxf $(PREFIX).scad
	$(OPENSCAD) $(OPENSCAD_OPTIONS) -Dparam_part=\"laser_base_b\" -Dparam_kerf=0.2 -Dparam_motor=\"wiper\"      -o generated/Laser/kerf-0.2/laser_base_b_wiper.dxf $(PREFIX).scad
