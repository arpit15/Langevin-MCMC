#!/bin/bash python
from argparse import ArgumentParser
import xml.etree.ElementTree as ET
from copy import deepcopy
import os
from os.path import dirname, exists, join, expanduser
import numpy as np
from skimage.io import imread
from time import time
import subprocess
from glob import glob
from subprocess import Popen, PIPE
import OpenEXR
import Imath
import matplotlib.pyplot as plt

from mitsuba.core import Spectrum, InterpolatedSpectrum

import hdr_to_color_mit

from ipdb import set_trace

SPECTRUM_MIN_WAVELENGTH = 360
SPECTRUM_MAX_WAVELENGTH = 830
SPECTRUM_RANGE = SPECTRUM_MAX_WAVELENGTH - SPECTRUM_MIN_WAVELENGTH
 
exe = expanduser("~/projects/Langevin-MCMC/build-Release/dpt")
# exe = expanduser("~/projects/mitsuba/dist_spectral/mitsuba")

def generate_fullpath(fname, pathlist):
    for currpath in pathlist:
        currfullpath = join(currpath, fname)
        if (exists(currfullpath)):
            return currfullpath

    print(f"Can't find {fname}")
    exit(0)

def parseRGB(rgb_str):
    
    # check if the str has ,
    if rgb_str.find(",") != -1:
        pieces = rgb_str.split(",")
    else: 
        pieces = rgb_str.split()

    # print(rgb_str, pieces)
    # check if single val
    if(len(pieces) == 1):
        return float(pieces[0]), float(pieces[0]), float(pieces[0])
    else:
        return float(pieces[0]), float(pieces[1]), float(pieces[2])

def generate_rgb2spec(fname, num_spec_sample, pathlist=[]):
    # print(fname)
    stepSize = SPECTRUM_RANGE/num_spec_sample
    lam_list = np.linspace(SPECTRUM_MIN_WAVELENGTH+stepSize/2, SPECTRUM_MAX_WAVELENGTH - stepSize/2, num_spec_sample)
    
    pathlist.append(dirname(fname))
    main_tree = ET.parse(fname)
    main_root = main_tree.getroot()
    # create num_spec xmls
    tree_list = []
    root_list = []
    # rgb_iters = []
    rgb_iters = [main_root.findall(".//*rgb")]
    spec_iters = [main_root.findall(".//*spectrum")]
    include_iters = [main_root.findall(".//include")]
    for _ in range(num_spec_sample):
        curr_tree = deepcopy(main_tree)
        curr_root = curr_tree.getroot()
        tree_list.append(curr_tree)
        root_list.append(curr_root)

        rgb_iters.append(curr_root.findall(".//*rgb"))
        spec_iters.append(curr_root.findall(".//*spectrum"))
        include_iters.append(curr_root.findall(".//include"))
    # ----------

    # find all rgb tags
    print("Parsing RGB data")
    for ii in range(len(rgb_iters[0])):
        ele_m = rgb_iters[0][ii]
        
        rgb_data = ele_m.attrib["value"]
        # ----
        r, g, b = parseRGB(rgb_data)
        spec_data_tmp = Spectrum()
        # TODO: arpita1 use parent
        spec_data_tmp.fromLinearRGB(r, g, b, Spectrum.EReflectance)
        # obtain by evaluating the values at known wv
        spec_data = np.zeros((lam_list.size,1))
        for i in range(lam_list.size):
            spec_data[i] = (spec_data_tmp.eval(lam_list[i]))
        # ----
        # update each xml tree
        for jj in range(len(rgb_iters[1:])):
            ele_ch = rgb_iters[jj+1][ii]
            ele_ch.attrib["value"] = str(spec_data[jj][0].astype("float32"))


    # find all spectrum tags
    print("Parsing spectrum data")
    #  need to get the parent so that we can replace spectrum with rgb element
    for ii in range(len(spec_iters[0])):
        # need to have this attribute
        # TODO: (arpita1) Support other ways of specifying spectrum data in xml
        ele_m = spec_iters[0][ii]
        spec_fn = ele_m.attrib["filename"]
        # check relative path
        # spec_data = np.loadtxt(spec_fn)
        # spec_data_interp = sample_spec(spec_data, num_spec_sample)

        print(spec_fn)
        interp = InterpolatedSpectrum(generate_fullpath(spec_fn, pathlist))
        interp.zeroExtend()
        discrete =  Spectrum()
        discrete.fromContinuousSpectrum(interp)
        discrete.clampNegative()
        # obtain by evaluating the values at known wv
        spec_data = np.zeros((lam_list.size,1))
        for i in range(lam_list.size):
            spec_data[i] = discrete.eval(lam_list[i])
            # spec_data[i] = interp.eval(lam_list[i])

        # -----------
        # update each xml tree
        # for j, ele_ch in enumerate(ele_o):
        for jj in range(len(spec_iters[1:])):
            ele_ch = spec_iters[jj+1][ii]
            ele_ch.tag = "rgb"
            # remove filename attrib
            del ele_ch.attrib["filename"]
            ele_ch.attrib["value"] = str(spec_data[jj][0].astype("float32"))

            # print(f"{lam_list[jj]:.3f} - {spec_data[jj][0]}")

    # iterate if you find include
    for ii in range(len(include_iters[0])):
        ele_m = include_iters[0][ii]
        filename_inc = str(ele_m.attrib["filename"]) # need to make a mutable version
        # print("first element include name: ",filename_inc)
        generate_rgb2spec(generate_fullpath(filename_inc, pathlist), num_spec_sample)
        # set_trace()
        # update each xml tree
        for jj in range(len(include_iters[1:])):
            
            ele_ch = include_iters[jj+1][ii]
            lam = lam_list[jj] #- stepSize
            
            # print("replaced element include name: ",filename_inc.replace(".xml", f"_lam{lam:.3f}.xml"))
            # set_trace()
            ele_ch.attrib["filename"] = filename_inc.replace(".xml", f"_lam{lam:.3f}.xml")
            # print(ele_ch.attrib["filename"])

    # write the each file
    output_fname_list = []
    for tree, lam in zip(tree_list, lam_list):
        fn = fname.replace(".xml", f"_lam{lam:.3f}.xml")
        output_fname_list.append(fn)
        tree.write(open(fn, 'wb'))

    # set_trace()

    return output_fname_list


def render(fname, paramlist):
    bashCommand = exe
    # set_trace()
    for key in paramlist:
        bashCommand += " -D" + key[0] 

    bashCommand += f" {fname}"
    print(bashCommand)
    process = Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()
    # print(process.returncode)
    # print(output, error)
    if(process.returncode): print("FAILED!")

# save multichannel image
def imsave(fname, wvs, img):
    stepSize = wvs[1] - wvs[0]
    sz = img.shape
    
    pt = Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT))
    outdict = {}
    channels = {}
    for ii in range(sz[2]):
        channel_names = f"{wvs[ii]-stepSize/2:.3f}-{wvs[ii]+stepSize/2:.3f}"
        # print(f"channel name : {channel_names[ii]}")
        outdict[channel_names] = img[...,ii].tobytes()
        channels[channel_names] = pt
    

    header = OpenEXR.Header(sz[1], sz[0])
    header['channels'] = channels
    
    out = OpenEXR.OutputFile(fname, header)
    out.writePixels(outdict)
    out.close()
    print("Done!")


def main(fname, output, param_list, spec_num=30):

    outfn = output
    if (outfn == None):
        outfn = fname.replace(".xml", ".exr")

    # clean up before creating new files and data
    clear_list = glob(join(dirname(fname), "*_lam*.xml"))
    clear_list += glob(join(dirname(fname), "*_lam*.exr.exr"))
    clear_list += glob(join(dirname(fname), "*_lam*.exr.png"))

    for fn in clear_list:
        os.remove(fn)

    start = time()
    # take the input string from loading xml recursively
    output_fname_list = generate_rgb2spec(fname, spec_num)
    filegen_end = time()
    print(f"spectral filegen time : {filegen_end - start}")
    # run render command
    start_render = time()
    for i, fn in enumerate(output_fname_list):
        # skip for wv > 720
        if i<4 or i>21:
            continue
        render(fn, param_list)
    end_render = time()
    print(f"Spectral Render time : {end_render - start_render}")
    # join the images to create a hyperspectral image
    
    firstTime = True
    start_join = time()
    for ii, xml_fn in enumerate(output_fname_list):
        # LMC
        img_fn = xml_fn.replace(".xml", ".exr.exr")
        # MITSUBA
        # img_fn = xml_fn.replace(".xml", ".exr")
        # # remove decimal part
        # end_id = img_fn.rfind(".")
        # start_id = img_fn[:end_id].rfind(".")
        # img_fn_new = img_fn[:start_id] + img_fn[end_id:]
        # # print(img_fn_new)
        # img_fn = img_fn_new
        # --------
        if (exists(img_fn)):
            print(img_fn)
            exr = imread(img_fn)
            if (firstTime):
                h,w,_ = exr.shape
                hyperspectral_img = np.zeros((h,w,spec_num), "float32")
                firstTime = False
            hyperspectral_img[...,ii] = exr[...,0]
            
    end_join = time()
    print(f"Join time : {end_join - start_join}")
    stepSize = SPECTRUM_RANGE/spec_num
    lam_list = np.linspace(SPECTRUM_MIN_WAVELENGTH+stepSize/2, SPECTRUM_MAX_WAVELENGTH - stepSize/2, spec_num)
   
    imsave(outfn, lam_list, hyperspectral_img)

    # convert to rpiv1 3 ch image
    hdr_to_color_mit.main(outfn)

if __name__ == "__main__":
    parser = ArgumentParser(description="mitsuba like cmd prompt")
    parser.add_argument('fname', help="input filename", type=str)
    parser.add_argument('--spec_num', help="Number of spectral samples", type=int, default=30)
    parser.add_argument( '-o',dest='output', type=str, help="output filename. Defaults to input file with exr ext")
    parser.add_argument("-D",
                        metavar="KEY=VALUE",
                        nargs='*', action='append',
                        help="Set a number of key-value pairs "
                             "(do not put spaces before or after the = sign). "
                             "If a value contains spaces, you should define "
                             "it with double quotes: "
                             'foo="this is a sentence". Note that '
                             "values are always treated as strings.")


    args = parser.parse_args()
    main(args.fname, args.output, args.D, args.spec_num)

