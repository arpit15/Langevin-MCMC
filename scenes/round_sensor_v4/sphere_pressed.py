#!/usr/bin/env python
import os
import subprocess
import numpy as np
from os.path import join

spp = int(2**14)
# spp = 4

use_mit = True
dpt_exe = os.path.expanduser('~/projects/Langevin-MCMC/build-Release/dpt')

mit_exe = os.path.expanduser('~/projects/mitsuba/build/release/mitsuba/mitsuba')
tonemap_exe = os.path.expanduser('~/projects/mitsuba/build/release/mitsuba/mtsutil')
outdir = "results"
# outdir = "results_h2mc"

exposure = 2**11

fname = "sensor_thick_epoxy_v1_withobject.xml"

xyzloc = [(0.005,0.018, -0.021), \
      (0, 0.027, 0.023), \
      (0.007, 0.018, -0.02), \
      (-0.013, 0.023, -0.017) \
      ]

for x,y,z in xyzloc:

    print(f"x : {x:.3f}, y : {y:.3f}, z : {z:.3f}")
    # render
    if not use_mit:
      bashCommand = dpt_exe
    else:
      bashCommand = mit_exe

    bashCommand += f" -Dspp={spp}"

    bashCommand += f" -Dspherex={x:.3f}"
    bashCommand += f" -Dspherey={y:.3f}"
    bashCommand += f" -Dspherez={z:.3f}"
    
    # for some neg numbers in fnames are not treated properly
    outFn = join(outdir, f"spherepress_x{abs(x):.3f}_y{abs(y):.3f}_z{abs(z):.3f}")
    bashCommand += f" -o {outFn}.exr"

    bashCommand += f" {fname}"

    print(bashCommand.split())
    process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()
    # print(output, error)
    # tonemap

    if use_mit:
      bashCommand = tonemap_exe
      bashCommand += f" tonemap -m {exposure}"
      bashCommand += f" {outFn}.exr"
      process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
      output, error = process.communicate()