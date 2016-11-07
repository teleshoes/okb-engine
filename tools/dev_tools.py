#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Development tools (log & configuration management) implementation """

import os
import configparser as ConfigParser

class Tools:
    def __init__(self, params = dict(), verbose = True):
        self.params = params
        cp = ConfigParser.SafeConfigParser()
        self._cpfile = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", "okboard.cf"))
        cp.read([ self._cpfile ])
        if "main" not in cp: raise Exception("No [main] section in okboard.cf")
        self._cp = cp
        self._cache = dict()
        self._db = None

        self.verbose = verbose
        self.messages = [ ]

    def set_db(self, db):
        self._db = db
        self._cache = dict()

    def cf(self, key, default_value = None, cast = None):
        if key in self._cache: return self._cache[key]

        if key in self.params:
            ret = self.params[key]
        elif self._db and self._db.get_param(key):
            ret = self._db.get_param(key)  # per language DB parameter values
        elif key in self._cp["main"]:
            ret = self._cp["main"][key]
        elif default_value is not None:
            ret = default_value
        else:
            raise Exception("No value for parameter: %s" % key)

        if cast: ret = cast(ret)
        self._cache[key] = ret
        if key not in self.params: self.params[key] = ret

        return ret

    def set_param(self, key, value):
        self._cache.pop(key, None)
        self.params[key] = value
        self._cp["main"][key] = "%.4f" % value if isinstance(value, float) else str(value)

    def log(self, *args, **kwargs):
        if not args: return
        message = ' '.join(map(str, args))
        if self.verbose: print(message)
        self.messages.append(message)
        if "logfile" in self.params:
            with open(self.params["logfile"], "a") as f:
                f.write(message + "\n")

    def save(self, suffix = ".updated"):
        file = self._cpfile + suffix if suffix else ""
        with open(file + ".tmp", 'w') as f:
            f.write("# OKboard default parameters\n\n")
            self._cp.write(f)
        os.rename(file + ".tmp", file)
        print("===> New configuration file saved to: %s" % file)
        print()
