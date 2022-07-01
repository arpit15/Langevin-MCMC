#!/bin/bash

echo "MODE $1"

if [ $1 -eq 0 ]
then
sudo apt -y update
sudo apt -y upgrade
sudo apt install -y git ninja-build tmux vim cmake-curses-gui unzip

sudo apt install -y gcc-10 g++-10 google-perftools libgoogle-perftools-dev zlib1g-dev

sudo apt install -y libopenimageio-dev \
  libtbb-dev libeigen3-dev openimageio-tools \
  libopenexr-dev libfreeimage-dev \
  libglu1-mesa libxi-dev libxmu-dev libglu1-mesa-dev

# get cmake version 3.21
wget https://github.com/Kitware/CMake/releases/download/v3.24.0-rc1/cmake-3.24.0-rc1-linux-x86_64.sh
chmod u+x cmake-3.24.0-rc1-linux-x86_64.sh
sudo ./cmake-3.24.0-rc1-linux-x86_64.sh --skip-license --prefix=/usr/local
# tmux configs
wget https://gist.githubusercontent.com/arpit15/56a0f72823c93362fe54232808ad0aeb/raw/04aa0ce2048a9a8b1cac562900b7feaffc96e051/.tmux.conf
fi

if [ $1 -eq 1 ]
then
# 
mkdir ~/projects && cd ~/projects
git clone https://github.com/arpit15/lmc.git
cd lmc
git submodule update --init --recursive

# lmc build instructions
mkdir ~/projects/lmc/build-Release && cd ~/projects/lmc/build-Release
cmake -GNinja .. \
-DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON

ninja
./dpt -m

echo "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/ubuntu/projects/lmc/build-Release/embree" >> ~/.bashrc
fi

if [ $1 -eq 2 ]
then
# pyenv setup
sudo apt-get install -y make build-essential libssl-dev \
libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev \
libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev openssl

curl https://pyenv.run | bash
# pyenv setup
export PYENV_ROOT="$HOME/.pyenv"
command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init -)"
# restart the shell
exec "$SHELL"
# install python
pyenv install -v 3.8.2
pyenv global 3.8.2
# create virtual env
pyenv virtualenv 3.8.2 py382
pyenv activate py382
#
cd ~/projects/lmc
pip install -ve .

# python requirements
cd ~/projects/lmc
pip install -r requirements.txt

# scene setup
cd ~/projects
git clone https://github.com/arpit15/round_sensor_scene_files.git

# shape optim repo
git clone https://github.com/arpit15/fingertip_sensor_design_optim.git
cd ~/projects/fingertip_sensor_design_optim
pip install -r requirements.txt
fi