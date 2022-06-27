from os.path import join, expanduser, dirname
import numpy as np
import lmc
import matplotlib.pyplot as plt
from skimage.io import imsave, imread
import os
from ipdb import set_trace

PDMSIOR = 1.421
EPOXYIOR = 1.49
num_layers = 3

vis = False

# fname = join("scenes", "simple_test_scenes", "scene4_v1.xml")
fname = expanduser("scenes/simple_test_scenes/scene4_v1.xml")

params = {}
scene = lmc.PyScene(fname, "myfn", params)
cam_trafo = scene.camera_trafo()
print("type", type(cam_trafo))
print(cam_trafo[:3, :3])
