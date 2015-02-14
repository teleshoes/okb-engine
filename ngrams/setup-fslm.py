#! /usr/bin/python

from distutils.core import setup, Extension

module1 = Extension('cfslm',
                     sources = ['fslm.c', 'fslm_python.c', 'pack.c'])

setup(name = 'cfslm',
      version = '1.0',
      author = 'Eric Berenguier',
      author_email = 'eb@cpbm.eu',
      description = 'C implementation of fast & compact N-gram storage',
      ext_modules = [module1])
