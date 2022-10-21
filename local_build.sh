#!/bin/bash

if [ "$PYVER" ]; then
	pyver=$PYVER
else
	pyver=`python --version 2>&1 | grep -oE '([[:digit:]].[[:digit:]])' | head -n1`
fi

echo "python ver: ${pyver}"

pyvernodot="${pyver//./}"
# echo "python ver: ${pyvernodot}"

pip install -v .
# HACK
cp _skbuild/linux-x86_64-${pyver}/cmake-build/python_bind/dpt_ext.cpython-${pyvernodot}-x86_64-linux-gnu.so python_bind/lmc