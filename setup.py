from setuptools import setup, Extension
#from distutils.core import setup, Extension
import numpy
import time

"""
Note: on OSX you must do this:

    export ARCHFLAGS="-arch x86_64"

...or else Python will try to build in 32-bit mode, which will fail.
"""

# to build:       python ./setup.py build
# to install:     python ./setup.py install
# to develop:     python ./setup.py develop



####################################################################
# Safety code to prevent accidental uploading of ths private project
import sys
def forbid_publish():
    argv = sys.argv
    blacklist = ['register', 'upload']

    for command in blacklist:
        if command in argv:
            values = {'command': command}
            print('Command "%(command)s" has been blacklisted, exiting...' %
                  values)
            sys.exit(2)
forbid_publish()



libmatrix_utils = Extension("matrix_utils.libmatrix_utils",
                [
                 "./src/matrix_utils.cpp", 
                 "./src/python_interface.cpp", 
                ], 
                extra_compile_args=["-march=x86-64", "-mavx", "-msse2", "-g", "-O3", "-fPIC", "-std=c++11"],
                extra_link_args=["-lm", "-lstdc++"],
                include_dirs=["src/", "/usr/include/python2.7", numpy.get_include()]
                )

setup(name="matrix_utils",
      version=time.strftime('%Y.%m.%d.%H.%M'),
      packages = ["matrix_utils"],
      ext_modules=[libmatrix_utils],
      author="University of Bristol",
      author_email="(add email address here)",
      url="(add URL here)",
      description="Some matrix utilities.",
      )
