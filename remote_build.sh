#!/bin/bash
#SBATCH --job-name=job
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=36

export PYENV_ROOT="$HOME/.pyenv"
command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init -)"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/ubuntu/projects/lmc/build-Release/embree
export LMCEXE=/home/ubuntu/projects/lmc/build-Release/dpt

cd ~/projects/lmc
pip install -ve .