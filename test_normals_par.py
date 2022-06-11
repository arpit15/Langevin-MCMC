from os.path import join
import numpy as np
import lmc
import matplotlib.pyplot as plt
from skimage.io import imsave
import os
from ipdb import set_trace

PDMSIOR = 1.421
EPOXYIOR = 1.49
num_layers = 3

vis = False

# fname = join("scenes", "simple_test_scenes", "scene4_v1.xml")
fname = "/home/arpit/projects/round_sensor_scene_files/fingertipsensor_recon_test/test_indenter.xml"

params = dict()
scene = lmc.PyScene(fname, "myfn", params)

h = scene.pixelHeight
w = scene.pixelWidth
rayorg = np.zeros((h,w,3), dtype=np.float32)
raydir = np.zeros((h,w,3), dtype=np.float32)
scene.camera_rays(rayorg, raydir)

shading_normals = np.zeros((h,w,3), dtype=np.float32)
pos = np.zeros((h,w,3), dtype=np.float32)

def normalize(vec):
  mag = np.linalg.norm(vec, axis=-1, keepdims=True)
  vec = vec/mag
  return vec

def refract(wi, n, eta, pos):
  outorg = pos.copy()
  wi = normalize(wi)
  n = normalize(n)

  cos_theta = np.sum(wi*n, axis=-1, keepdims=True)
  sin_theta2 = 1. - cos_theta*cos_theta
  sin_thetaT2 = 1. - eta*eta*sin_theta2

  scalar_term1 = np.sqrt(np.maximum(sin_thetaT2, 0.))
  wo = n*scalar_term1 + eta*(wi - cos_theta*n)
  wo = normalize(wo)
  return outorg, wo

eta1 = 1.0/EPOXYIOR
eta2 = EPOXYIOR/PDMSIOR

shading_normals, pos = scene.ray_intersect_all(rayorg, raydir)

if num_layers>1:
  outrayorg1, outraydir1 = refract(raydir, -shading_normals, eta1, pos)
  shading_normals1, pos1 = scene.ray_intersect_all(outrayorg1, outraydir1)

if num_layers>2:
  outrayorg2, outraydir2 = refract(outraydir1, -shading_normals1, eta2, pos1)
  shading_normals2, pos2 = scene.ray_intersect_all(outrayorg2, -outraydir2)
  
print(os.getcwd())

if vis:
  plt.imshow((shading_normals+1)/2)
  plt.title("shading_normals")
  plt.show()

imsave("shading_normals.exr", (shading_normals+1)/2)
np.save("shading_normals.npy", shading_normals)

if num_layers>1:
  if vis:
    plt.imshow((shading_normals1+1)/2)
    plt.title("shading_normals1")
    plt.show()

  imsave("shading_normals1.exr", (shading_normals1+1)/2)
  np.save("shading_normals1.npy", shading_normals1)

if num_layers>2:
  if vis:
    plt.imshow((shading_normals2+1)/2)
    plt.title("shading_normals2")
    plt.show()

  imsave("shading_normals2.exr", (shading_normals2+1)/2)
  np.save("shading_normals2.npy", shading_normals2)