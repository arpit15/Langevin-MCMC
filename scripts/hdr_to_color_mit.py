import OpenEXR
import Imath
import colour
from os.path import dirname, join
from colour.plotting import plot_single_sd, plot_multi_sds
import numpy as np
import matplotlib.pyplot as plt
from skimage.io import imsave
import argparse

from ipdb import set_trace

MIT_START_WV = 360
MIT_END_WV = 830
MIT_NUM_WV = 30
MIT_DELTA_WV = (MIT_END_WV-MIT_START_WV)/(MIT_NUM_WV)
print(f"delta {MIT_DELTA_WV}")

extrapolate_params={ 'method': 'Linear', 'right': 0 }

# xyz/rpiv1/rpiv2/colorfilter
def get_cmfs(viewer_type="xyz"):
  filedir = dirname(__file__)
  if (viewer_type == "xyz" or viewer_type == "human"):
    if (viewer_type == "xyz"):
      cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']
    elif(viewer_type == "human"):
      cmfs = colour.MSDS_CMFS['Wright & Guild 1931 2 Degree RGB CMFs']
    # add wv info
    wv = np.arange(cmfs.shape.start, cmfs.shape.end+cmfs.shape.interval, cmfs.shape.interval).reshape(-1,1)
    data = cmfs.values
    data = np.concatenate((wv, data), axis=1)

  elif (viewer_type=="rpiv1"):
    fn = join(filedir, "cameras/Omnivision_OV5647_spectral_response.csv")
    data = np.loadtxt(fn, skiprows=1, delimiter=',')
    data[:,1:] /= 100.0
  elif (viewer_type=="rpiv2"):
    fn = join(filedir, "cameras/pagnutti_v2_cam1.csv")
    data = np.loadtxt(fn, skiprows=1, delimiter=',')
    data[:,1:] /= 100.0
  elif (viewer_type=="prosilica"):
    fn = join(filedir, "cameras/prosilica_website_monochrome_digitalized.csv")
    data = np.loadtxt(fn, skiprows=1, delimiter=',')
    data[:,1:] /= 100.0
  elif (viewer_type == "colorfilter"):
    # fractional intervals are not supported
    fn532_450 = join(filedir, "cameras", "color_filter_transmission_532_450.csv")
    data532_450 = np.loadtxt(fn532_450, skiprows=1, delimiter=',')
    # for wv
    sd_532 = colour.SpectralDistribution({A: B for A, B in zip(data532_450[:,0], data532_450[:,1])}, name="532nm")
    sd_450 = colour.SpectralDistribution({A: B for A, B in zip(data532_450[:,0], data532_450[:,2])}, name="450nm")

    fn660 = join(filedir, "cameras", "color_filter_transmission_660nm.csv")
    data660 = np.loadtxt(fn660, skiprows=1, delimiter=',')
    sd_660 = colour.SpectralDistribution({A: B for A, B in zip(data660[:,0], data660[:,1])}, name="660nm")
    
    # sd_660.align(sd_532.shape, extrapolator_kwargs=extrapolate_params, interpolator=colour.LinearInterpolator)
    sd_532.align(sd_660.shape, extrapolator_kwargs=extrapolate_params, interpolator=colour.LinearInterpolator)
    sd_450.align(sd_660.shape, extrapolator_kwargs=extrapolate_params, interpolator=colour.LinearInterpolator)

    # set_trace()
    # plot_single_sd(sd_660)
    # plot_single_sd(sd_532)
    # plot_single_sd(sd_450)

    # sd_660_arr = sd_660.values.reshape(-1,1)
    data = np.concatenate((data660, sd_532.values.reshape(-1,1)[::-1], sd_450.values.reshape(-1,1)[::-1]), axis=1)
      
    # curr_sd = {A: B for A, B in zip(data[:,0], data[:,1])}
    # curr_sd = colour.SpectralDistribution(curr_sd, name=str(1))
    # plot_single_sd(curr_sd)

    data[:,1:] /= 100.0 # convert % to [0,1]
  return data

# align to img wv
def get_aligned_cmfs_arr(data, spec_shape):
  print(spec_shape)
  out_spec_arr = []
  for i in range(1,data.shape[1]):
    curr_sd = {A: B for A, B in zip(data[:,0], data[:,i])}
    curr_sd = colour.SpectralDistribution(curr_sd, name=str(i))

    # get values in larger range for the next align to work to give values at exact wv
    curr_sd.align(colour.SpectralShape(spec_shape.start-10, spec_shape.end+10, 1), extrapolator_kwargs=extrapolate_params, interpolator=colour.SpragueInterpolator)
    curr_sd.align(spec_shape, extrapolator_kwargs=extrapolate_params, interpolator=colour.SpragueInterpolator)
    # plot_single_sd(curr_sd)
    curr_sd_arr = curr_sd.values.reshape(1,1,-1)
    out_spec_arr.append(curr_sd_arr)

  return out_spec_arr

def load_spec_exr(fn):
  file = OpenEXR.InputFile(fn)
  dw = file.header()['dataWindow']
  # print(file.header()['channels'])
  size = (dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1)
  pt = Imath.PixelType(Imath.PixelType.FLOAT)

  num_channels = len(file.header()['channels'])
  print(f"Num channels : {num_channels}")

  img = np.zeros((size[1], size[0], num_channels), np.float32)

  for i, name in enumerate(file.header()['channels']):
    # extract starting and ending wv
    start_wv, end_wv = name[:-2].split('-')
    start_wv = float(start_wv)
    end_wv = float(end_wv)

    mid_wv = (start_wv+end_wv)/2.

    datastr = file.channel(name, pt)
    data = np.frombuffer(datastr, dtype = np.float32)
    data.shape = (size[1], size[0]) 
    img[:,:,i] = data

  return img


def main(fn, cam="rpiv1"):

  ## load cmfs
  data = get_cmfs(cam)
  CIE_normalization = np.sum(data[:,2])
  # print(f"norm const : {CIE_normalization}")

  img = load_spec_exr(fn)

  ## align CMFS based on img wv
  # mit_sd_shape = colour.SpectralShape(MIT_START_WV+0*(MIT_DELTA_WV/2), MIT_END_WV-(MIT_DELTA_WV), MIT_DELTA_WV)
  # assuming the data is given at the middle the wavelength for imgs
  # interp params
  mit_sd_shape = colour.SpectralShape(MIT_START_WV+(MIT_DELTA_WV/2), MIT_END_WV-(MIT_DELTA_WV/2), MIT_DELTA_WV)
  out_spec_arr = get_aligned_cmfs_arr(data, mit_sd_shape)

  ## convert img from spec to 3ch data
  channels = []
  for spec_arr in out_spec_arr:
    ch = np.sum(img*spec_arr, axis=2)
    channels.append(ch)

  XYZ = MIT_DELTA_WV*np.stack(channels, axis=2)
  XYZ /= CIE_normalization
  
  rgb = XYZ

  imsave(fn.replace(".exr", "_spec2rgb.exr"), rgb.astype("float32"))
  

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Create RGB image from individual image')
  parser.add_argument('fname')
  parser.add_argument('--cam', '-c', type=str, 
          default = "rpiv1",
          help='camera type')

  args = parser.parse_args()

  main(args.fname, args.cam)