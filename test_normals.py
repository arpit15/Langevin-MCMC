from os.path import join
import numpy as np
import lmc
import matplotlib.pyplot as plt
from skimage.io import imsave

from ipdb import set_trace

PDMSIOR = 1.421
EPOXYIOR = 1.49
num_layers = 3

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

if num_layers > 1:
  shading_normals1 = np.zeros_like(shading_normals)
  pos1 = np.zeros_like(pos)
if num_layers > 2:
  shading_normals2 = np.zeros_like(shading_normals)
  pos2 = np.zeros_like(pos)



for y in range(h):
  for x in range(w):
    currorg, currdir = rayorg[y,x,:], raydir[y,x,:]
    scene.ray_intersect(
      currorg, currdir, 
      shading_normals[y,x,:], pos[y,x,:]
    )
    # print(f"Event1 - oldpos: {currorg} newpos: {pos[y,x,:]}")
    if num_layers>1:
      # refract 
      outrayorg1 = np.zeros_like(currorg)
      outraydir1 = np.zeros_like(currdir)
      # eta1 = EPOXYIOR/1.0
      eta1 = 1.0/EPOXYIOR
      # refraction in 3d presuming 
      # i: vector towards surface
      # n: from surface into the material
      # r: from surface into the material
      scene.refract(pos[y,x,:], currdir, 
        -shading_normals[y,x,:], eta1,
        outrayorg1, outraydir1
      )

      # print(f"Event1 - oldpos: {currorg} newpos: {pos[y,x,:]}")
      # print(f"Event1 - d1.d2: {np.dot(currdir, outraydir1)}")
      # print(f"Event1 - n.d1: {np.dot(currdir, -shading_normals[y,x,:])}")
      # print(f"Event1 - n.d2: {np.dot(outraydir1, -shading_normals[y,x,:])}")
      # set_trace()

      # ray trace
      scene.ray_intersect(
        outrayorg1, outraydir1, 
        shading_normals1[y,x,:], pos1[y,x,:]
      )

      # print(f"Event2 - oldpos: {outrayorg1} newpos: {pos1[y,x,:]}")

      if num_layers>2:
        # refract 
        outrayorg2 = np.zeros_like(currorg)
        outraydir2 = np.zeros_like(currdir)
        # eta2 = PDMSIOR/EPOXYIOR
        eta2 = EPOXYIOR/PDMSIOR
        scene.refract(pos1[y,x,:], outraydir1, 
          -shading_normals1[y,x,:], eta2,
          outrayorg2, outraydir2
        )

        # print(f"Event2 - oldpos: {outrayorg1} newpos: {pos1[y,x,:]}")
        # print(f"Event2 - d2.d3: {np.dot(outraydir1, outraydir2)}")
        # print(f"Event2 - n.d2: {np.dot(outraydir1, -shading_normals1[y,x,:])}")
        # print(f"Event2 - n.d3: {np.dot(outraydir2, -shading_normals1[y,x,:])}")
        # set_trace()

        # ray trace
        scene.ray_intersect(
          outrayorg2, -outraydir2, 
          shading_normals2[y,x,:], pos2[y,x,:]
        )

        # print(f"Event3 - oldpos: {outrayorg2} newpos: {pos2[y,x,:]}")
        # set_trace()

    
# plt.imshow((shading_normals+1)/2)
# plt.title("shading_normals")
# plt.show()

imsave("shading_normals.exr", (shading_normals+1)/2)
np.save("shading_normals.npy", shading_normals)

if num_layers>1:
  # plt.imshow((shading_normals1+1)/2)
  # plt.title("shading_normals1")
  # plt.show()

  imsave("shading_normals1.exr", (shading_normals1+1)/2)
  np.save("shading_normals1.npy", shading_normals1)

if num_layers>2:
  # plt.imshow((shading_normals2+1)/2)
  # plt.title("shading_normals2")
  # plt.show()

  imsave("shading_normals2.exr", (shading_normals2+1)/2)
  np.save("shading_normals2.npy", shading_normals2)