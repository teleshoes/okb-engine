#! /usr/bin/python

from distutils.core import setup, Extension

module1 = Extension('cdb',
                     sources = ['phash.c', 'cdb_python.c', 'pack.c'])

setup(name = 'cfdb',
      version = '1.0',
      author = 'Eric Berenguier',
      author_email = 'eb@cpbm.eu',
      description = 'Really just a fast & persistent hash map for python',
      ext_modules = [module1])
