#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" wrapper to add call logging to any object (with in/out value)
    only for debugging """

class LogWrapper:
    def __init__(self, name, instance, parent = None, callback = None):
        self._name = name
        self._instance = instance
        self._parent = parent
        self._callback = callback

    def __getattr__(self, attr):
        value = getattr(self._instance, attr)
        if hasattr(value, '__call__'):
            return LogWrapper(self._name + "." + attr, value, parent = self, callback = self._callback)
        else:
            return value

    def __call__(self, *args, **kwargs):
        ret = self._instance.__call__(*args, **kwargs)
        args_txt = []
        if args: args_txt.append(repr(list(args)))
        if kwargs: args_txt.append(repr(kwargs))

        print ("> LOG %s(%s)=%s" % (self._name, ' '.join(args_txt), ret))
        if self._callback:
            self._callback()
        return ret
