On Raspbian GNU/Linux 10 (buster) on a Pi 3B+

sudo apt update
sudo apt install pkg-config libgl1-mesa-dev libgles2-mesa-dev    python3-setuptools libgstreamer1.0-dev git-core    gstreamer1.0-plugins-{bad,base,good,ugly}    gstreamer1.0-{omx,alsa} python3-dev libmtdev-dev    xclip xsel libjpeg-dev
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
python3 -m pip install --upgrade --user pip setuptools
python3 -m pip install --upgrade --user Cython==0.29.14 pillow
python3 -m pip install --user kivy
python3 -m pip install --user kivy.garden.graph
cp -rp ~/.local/lib/python3.7/site-packages/kivy_garden/graph ~/.local/lib/python3.7/site-packages/kivy/garden/

git clone https://github.com/Arcus-3d/cosv.git
cd cosv/software/pi4_7touch/
python3 ./COSVTouch.py 

