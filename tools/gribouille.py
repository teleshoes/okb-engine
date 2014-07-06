#! /usr/bin/python2
# -*- coding: utf-8 -*-

"""
Letter trees management
(only used for tree structure creation)
"""

import os
import array
import re
import unicodedata

class LetterNode:
    def __init__(self, index = 0, tree = None, letter = None):
        self.index = index
        self.tree = tree
        self.letter = letter

    def isLeaf(self):
        info = self.tree._read_node(self.index)
        return info["last_child"] and info["payload"]

    def getChilds(self):
        result = [ ]
        index = self.index
        while True:
            info = self.tree._read_node(index)
            if not info["payload"]:
                result.append(LetterNode(index = info["index"],
                                         letter = info["letter"],
                                         tree = self.tree))
            if info["last_child"]: return result
            index += 4

    def getPayload(self):
        return self.tree._getPayload(self.index)

    def getChar(self):
        return self.letter

class LetterTree:
    def __init__(self):
        self.data = array.array('B')

    def _extend(self, index):
        if len(self.data) < index:
            self.data.extend([0] * (index - len(self.data)))

    def _read_node(self, index):
        value = dict(letter = chr(96 + self.data[index] % 64),
                     last_child = ((self.data[index] & 0x40) != 0),
                     payload = ((self.data[index] & 0x80) != 0),
                     # struct module does not handle bit fields ? :-(
                     index = 4 * (self.data[index + 1] + (self.data[index + 2] << 8) + (self.data[index + 3] << 16)))
        return value

    def _write_node(self, index, letter, last_child, payload, dest_index):
        if dest_index % 4: raise Exception("not aligned index")
        dest_index >>= 2
        if dest_index >= (1 << 24) - 10: raise Exception("overflow")
        self._extend(index + 4)
        self.data[index] = (ord(letter) - 96 if letter else 0) + (0x40 if last_child else 0) + (0x80 if payload else 0)
        self.data[index + 1] = dest_index & 0xff
        self.data[index + 2] = (dest_index >> 8) & 0xff
        self.data[index + 3] = (dest_index >> 16) & 0xff

    def _getPayload(self, index):
        info = self._read_node(index)
        if not info["payload"]: return None
        pi = info["index"]
        l = self.data[pi] + (self.data[pi + 1] << 8)
        out = bytes([self.data[pi + 2 + i] for i in range(0, l) ])
        return out.decode('utf-8', 'ignore')


    def loadFromFile(self, filename):
        self.data = array.array('B')
        with open(filename, 'rb') as f:
            size = os.path.getsize(filename)
            self.data.fromfile(f, size)

    def saveFile(self, fileName):
        with open(fileName + '.tmp', 'wb') as f:
            self.data.tofile(f)
        os.rename(fileName + '.tmp', fileName)

    def getRoot(self):
        return LetterNode(index = 0, tree = self, letter = None)

    def beginLoad(self):
        self.tree = dict(childs = dict())

    def addWord(self, word):

        # remove accents & duplicates, remove non letter characters (spaces, "-", "'", etc.), and convert to lowercase
        letters = ''.join(c for c in unicodedata.normalize('NFD', word.decode('utf-8')) if unicodedata.category(c) != 'Mn')
        letters = re.sub(r'[^a-z]', '', letters.lower())
        letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
        if not letters: return
        letters = letters.encode('utf-8')

        if word == letters: word = '='

        if not re.match(r'^[a-z]+$', letters): raise Exception("Bad word: " + letters)

        # update tree
        x = self.tree
        for letter in letters:
            if letter not in x["childs"]:
                x["childs"][letter] = dict(childs = dict())
            x = x["childs"][letter]

        if "payload" not in x: x["payload"] = [ ]
        x["payload"].append(word)

    def endLoad(self):
        self.cur_index = 0
        self.data = array.array('B')
        self._rec_load(self.tree)
        self.tree = None

    def _rec_load(self, x, pre = ""):
        nchilds = len(x["childs"])
        n = nchilds + (1 if "payload" in x else 0)
        if not n: raise Exception("something bad has occured")

        index = start_index = self.cur_index
        self.cur_index += 4 * n

        if "payload" in x:
            payload = ','.join(x["payload"])
            self._write_node(index, letter = None, last_child = (nchilds == 0), payload = True, dest_index = self.cur_index)
            l = len(payload)
            real_len = ((6 + l) & ~0x3)
            self._extend(self.cur_index + real_len)
            self.data[self.cur_index] = l & 0xff
            self.data[self.cur_index + 1] = l >> 8
            for i in range(0, l):
                self.data[self.cur_index + 2 + i] = ord(payload[i])
            self.data[self.cur_index + 2 + l] = 0  # caller may assume payload is zero-terminated
            self.cur_index += real_len
            index += 4

        for letter in sorted(x["childs"]):
            child = x["childs"][letter]
            child_index = self._rec_load(child, pre + letter)
            nchilds -= 1
            self._write_node(index, letter = letter, last_child = (nchilds == 0), payload = False, dest_index = child_index)
            index += 4

        return start_index

    def dump(self, prefix="", node = None):
        if not node: node = self.getRoot()
        prefix += node.getChar() or "*"
        index = node.index
        while True:
            info = self._read_node(index)
            if info["payload"]:
                print prefix, index, info, "payload:", node.getPayload()
            else:
                child = LetterNode(index = info["index"],
                                   letter = info["letter"],
                                   tree = self)
                print prefix, index, info, "child"
                self.dump(prefix, child)
            if info["last_child"]: return
            index += 4
