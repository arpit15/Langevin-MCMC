import sys, re, os

try:
    from skbuild import setup
    import nanobind
except ImportError:
    print("The preferred way to invoke 'setup.py' is via pip, as in 'pip "
          "install .'. If you wish to run the setup script directly, you must "
          "first install the build dependencies listed in pyproject.toml!",
          file=sys.stderr)
    raise

setup(
    name="lmc",
    version="0.0.0",
    author="Arpit Agarwal",
    author_email="arpit15945@gmail.com",
    description="Langevin MCMC ",
    packages=["lmc"],
    package_dir={'': 'python_bind'},
    cmake_args=[
        '-DBUILD_PYTHON:BOOL=ON', 
        '-DBUILD_EXE:BOOL=OFF', 
        '-DCMAKE_C_COMPILER=gcc-10',
        '-DCMAKE_CXX_COMPILER=g++-10'
        ],
    cmake_install_dir="python_bind/lmc",
    include_package_data=True,
    python_requires=">=3.8"
)
