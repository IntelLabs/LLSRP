#!sh

if [ "$1" = "" ] ; then
  echo Usage $0 :[VIRTUAL_DISPLAY_NUM]
  echo Like: $0 :12
  exit
fi
export DISPLAY=$1
Xvfb $1 -screen 0 1024x768x24 &
sudo x-window-manager &
