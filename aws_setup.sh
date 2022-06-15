sudo apt update
sudo apt upgrade
sudo apt install -y git ninja-build tmux vim cmake-curses-gui unzip

sudo apt install -y gcc-10 g++-10 google-perftools libgoogle-perftools-dev zlib1g-dev

sudo apt install -y libopenimageio-dev \
  libtbb-dev libeigen3-dev openimageio-tools \
  libopenexr-dev \
  libglu1-mesa libxi-dev libxmu-dev libglu1-mesa-dev

# get cmake version 3.21
wget https://github.com/Kitware/CMake/releases/download/v3.24.0-rc1/cmake-3.24.0-rc1-linux-x86_64.sh
chmod u+x cmake-3.24.0-rc1-linux-x86_64.sh
sudo ./cmake-3.24.0-rc1-linux-x86_64.sh --skip-license --prefix=/usr/local

# 
mkdir projects && cd projects
git clone https://github.com/arpit15/lmc.git
cd lmc
git submodule update --init --recursive

# lmc build instructions
cmake -GNinja .. \
-DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
# -DEMBREE_TBB_INCLUDE_DIR=/usr/include/tbb \

ninja
./dpt -m

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
pip install -r requirements.txt

# scene setup
cd ~/projects
git clone https://github.com/arpit15/round_sensor_scene_files.git

# shape optim repo
git clone https://github.com/arpit15/fingertip_sensor_design_optim.git
pip install -r requirements.txt