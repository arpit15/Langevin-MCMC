from os.path import join, basename, dirname, realpath
from glob import glob
import subprocess
from ipdb import set_trace

curr_dir = dirname(realpath(__file__))
folder = join(curr_dir, "..", "scenes", "simple_test_scenes")

fname_list = glob(join(folder, "*.xml"))

# set_trace()
for fname in fname_list:
    print(f"Rendering {basename(fname)}")
    subprocess.run([join(curr_dir, "../build-Release/dpt"), fname])