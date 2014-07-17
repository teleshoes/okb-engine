#! /usr/bin/python3
# -*- coding: utf-8 -*-

import sys
import configparser as ConfigParser

def get_params(fname):
    cp = ConfigParser.SafeConfigParser()
    cp.read(fname)
    params = dict()
    for s in [ "default", "portrait" ]:
        for key, value in cp[s].items():
            params[key] = value
    return params

if __name__ == "__main__":
    params = get_params(sys.argv[1])
    for key, value in params.items():
        print(key, value)
