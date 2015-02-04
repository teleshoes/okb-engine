#! /usr/bin/python3
# -*- coding: utf-8 -*-

"""
This file is part of OKBoard project

Copyright (c) 2015, Eric Berenguier <eb@cpbm.eu>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

==============================================================================

Fast & compact N-grams storage

This is an implementation of this (excellent) paper:

http://nlp.cs.berkeley.edu/pubs/Pauls-Klein_2011_LM_paper.pdf
Faster and Smaller N-Gram Language Models
Adam Pauls Dan Klein
Computer Science Division
University of California, Berkeley
{adpauls,klein}@cs.berkeley.edu

It's meant to require only simple code on the device for decoding / searching

Note: this will allow to store stock n-grams, but not user n-grams used for
learning because this is a read-only structure
"""

import struct
import time
import sys

class TableLine:
    def __init__(self, wid, context_offset, count, context_key = None):
        self.wid = wid
        self.context_offset = context_offset
        self.count = count
        self.context_key = context_key

class BlockCompressor():
    def __init__(self, block_size = 128):
        self.buf = bytes()
        self.block_size = block_size
        self.buf = bytes()
        self.current_line = [ ]
        self.current_block = [ ]
        self.block_bits = -1
        self.block_count = 0

    def set_offsets(self, table_offset, context_offset):
        self.table_offset = table_offset
        self.context_offset = context_offset

    def next_block(self):
        if self.block_bits >= 0:
            # write previous block

            b = 0
            v = 0
            acc = 0
            pos = 0
            block = bytes()

            self.current_block.append((0, 8))  # dummy padding
            while b or self.current_block:
                if b == 0: v, b = self.current_block.pop(0)
                if pos == 8:
                    block += bytes([acc])
                    pos = 0
                    acc = 0
                n = min(8 - pos, b)
                if n > 0:  # write n bits to current block
                    acc <<= n
                    b -= n
                    acc += (v >> b)
                    v &= ((1 << b) - 1)
                    pos += n

            if len(block) > self.block_size - 8: raise Exception("Block overflow")

            if len(block) < self.block_size - 8:
                block += bytes(self.block_size - 8 - len(block))

            self.buf += block

        # begin a new block
        self.current_block = [ ]
        self.block_bits = 0
        self.block_count += 1

        # write new block header
        self.buf += struct.pack('>II', self.table_offset, self.context_offset)

    def next_line(self):
        line_bits = sum([ x[1] for x in self.current_line ])
        if self.block_bits < 0 or self.block_bits + line_bits >= (self.block_size - 10) * 8:
            # current block is full
            self.next_block()

        self.block_bits += line_bits
        self.current_block += self.current_line
        self.current_line = []

    def add_int(self, value, bits):
        if value & ((1 << bits) - 1) != value: raise Exception("Not enough bits (%d) to store integer %d" % (bits, value))
        self.current_line.append((value, bits))

    def add_variable_length(self, value, base_bits):
        bits = max(1, int(value).bit_length())
        numbers = int((bits + base_bits - 1) / base_bits)

        self.add_int(value + (1 << (numbers * base_bits)), numbers * base_bits + numbers)

    def get_bytes(self):
        return self.buf

class NGramTableEncoder:
    def __init__(self, max_wid, base_bits = 1, block_size = 128, prefix = None, progress = None):
        self.base_bits = base_bits
        self.nodes = []
        self.wids = set()
        self.out = None
        self.block_size = block_size
        self.prefix = prefix
        self.max_wid = max_wid + 1
        self.progress = progress

    def add(self, wid, context_offset, count, context_key = None):
        wid = int(wid)
        if wid < 0 or wid > self.max_wid: raise Exception("Bad word ID: %d" % wid)

        self.wids.add(wid)
        self.nodes.append(TableLine(wid, context_offset, count, context_key))

    def get_bytes(self):
        if not self.out: self._eval()
        return self.out

    def get_context2offset(self):
        if not self.out: self._eval()
        return self.context2offset

    def _eval(self):
        self.context2offset = dict()

        blocks = BlockCompressor(block_size = self.block_size)

        word2offset = dict()
        cur_offset = 0

        lastp = time.time()

        w2nodes = dict()
        for node in sorted(self.nodes, key = lambda x: x.context_offset):
            wid = node.wid
            if wid not in w2nodes: w2nodes[wid] = []
            w2nodes[wid].append(node)

        i = 0
        for wid in sorted(self.wids):
            i += 1
            begin_offset = cur_offset
            nodes = w2nodes[wid]

            if self.progress and time.time() > lastp + 10:
                sys.stderr.write("%d/%d [%d]\n" % (i, len(self.wids), cur_offset))
                lastp += 10

            last_context_offset = 0

            for node in nodes:
                blocks.set_offsets(table_offset = cur_offset, context_offset = node.context_offset)
                blocks.add_variable_length(node.context_offset - last_context_offset, base_bits = self.base_bits)
                blocks.add_variable_length(node.count, base_bits = self.base_bits)
                blocks.next_line()

                if self.prefix:
                    print("%s, block %d, offset %d, wid %d, context_offset %d [%d], count %d" %
                          (self.prefix, blocks.block_count - 1, cur_offset, wid,
                           node.context_offset, node.context_offset - last_context_offset, node.count))

                last_context_offset = node.context_offset

                new_context_key = (node.context_key or ()) + (wid,)
                self.context2offset[tuple(new_context_key)] = cur_offset

                cur_offset += 1

            end_offset = cur_offset - 1

            if end_offset >= begin_offset: word2offset[wid] = (begin_offset, end_offset)

        # write closing block
        blocks.set_offsets(table_offset = cur_offset, context_offset = 0)
        blocks.next_block()

        max_wid = max(word2offset.keys()) + 1
        out = bytes()
        blocks_bytes = blocks.get_bytes()
        out += struct.pack('>III', max_wid, len(blocks_bytes), blocks.block_count)
        for i in range(0, max_wid):
            word_offsets = word2offset.get(i, (-1, -1))
            out += struct.pack('>ii', word_offsets[0], word_offsets[1])
        out += blocks_bytes
        self.out = out
        if len(self.out) % 4: raise Exception("Bad alignment in table")


class NGramEncoder:
    def __init__(self, base_bits, block_size, verbose = False, progress = False):
        self.base_bits = base_bits
        self.block_size = block_size
        self.ngrams = []
        self.verbose = verbose
        self.max_wid = 0
        self.progress = progress

    def add_ngram(self, ngram, count):
        ngram = list(ngram)
        while len(ngram) > len(self.ngrams):
            self.ngrams.append(dict())

        count = int(count)

        while ngram:
            key = tuple([ int(x) for x in ngram ])
            n = len(ngram) - 1

            self.max_wid = max(self.max_wid, max(key) + 1)

            if key in self.ngrams[n]: self.ngrams[n][key] = max(self.ngrams[n][key], count)
            else: self.ngrams[n][key] = count

            count = 0  # smaller n-gram are initialized with 0 value
            ngram.pop(-1)

    def get_bytes(self):
        out = bytes()
        out += struct.pack('>HBB', self.block_size, self.base_bits, len(self.ngrams))

        tables = bytes()
        context2offset = None
        for n in range(0, len(self.ngrams)):
            table_encoder = NGramTableEncoder(self.max_wid, self.base_bits, self.block_size,
                                              prefix = "table n=%d" % n if self.verbose else None,
                                              progress = self.progress)
            for gram, count in self.ngrams[n].items():
                context_key = gram[0:-1]
                table_encoder.add(gram[-1], context2offset[context_key] if context2offset else 0, count, context_key)

            table_bytes = table_encoder.get_bytes()
            context2offset = table_encoder.get_context2offset()

            tables += struct.pack('>I', len(table_bytes))
            tables += table_bytes

        out += tables
        return out

class BitReader:
    def __init__(self, content, base_bits = 1):
        self.content = content
        self.pos = 0
        self.idx = 0
        self.base_bits = base_bits

    def read_bits(self, count):
        value = 0
        while True:
            if self.pos == 8:
                self.pos = 0
                self.idx += 1

            n = min(count, 8 - self.pos)
            value <<= n
            value += (self.content[self.idx] >> (8 - n - self.pos)) & ((1 << n) - 1)
            self.pos += n
            count -= n

            if not count: return value

    def get_variable_length(self):
        b = self.base_bits
        while self.read_bits(1) == 0:
            b += self.base_bits
        return self.read_bits(b)

class NGramDecoder:
    def __init__(self, content, verbose = False):
        self.bytes = content
        (self.block_size, self.base_bits, self.table_count) = struct.unpack('>HBB', self.bytes[0:4])
        ptr = 4
        self.table = []
        for i in range(0, self.table_count):
            table_len = struct.unpack('>I', self.bytes[ptr:ptr + 4])[0]
            table_bytes = self.bytes[ptr + 4:ptr + 4 + table_len]

            table_info = self.parse_table(table_bytes)
            self.table.append(table_info)

            ptr += 4 + table_len

        self.verbose = verbose

    def parse_table(self, content):
        max_wid, blocks_bytes, blocks_count = struct.unpack('>III', content[0:12])

        wordoffsets = dict()
        ptr = 12
        for i in range(0, max_wid):
            offsets = struct.unpack('>ii', content[ptr:ptr + 8])
            if offsets[0] >= 0: wordoffsets[i] = offsets
            ptr += 8

        blocks = []
        blocks_bytes = content[ptr:]
        for bi in range(0, blocks_count):
            blk_bytes = blocks_bytes[self.block_size * bi:self.block_size * (bi + 1)]
            (table_offset, context_offset) = struct.unpack('>II', blk_bytes[0:8])
            blocks.append(dict(table_offset = table_offset, context_offset = context_offset, data = blk_bytes[8:]))

        return dict(blocks_count = blocks_count, blocks = blocks, wordoffsets = wordoffsets)

    def table_lookup(self, table, wid, target_context_offset = None, prefix = None):
        # step 1: find block dichotomy = O(log(n))
        blocks = table["blocks"]
        a = 0
        b = len(blocks) - 1

        if target_context_offset is None: target_context_offset = 0

        word_offset_begin, word_offset_end = table["wordoffsets"].get(wid, (-1, -1))
        if word_offset_begin == -1: return None, None  # word not found

        while b > a + 1:
            c = int((a + b) / 2)
            table_offset = blocks[c]["table_offset"]
            context_offset = blocks[c]["context_offset"]

            if word_offset_end < table_offset:
                b = c
                continue

            if word_offset_begin >= table_offset:
                a = c
                continue

            # our target line is in [a, b[ blocks (requested word spans across all these blocks)

            if context_offset > target_context_offset: b = c
            else: a = c

        if prefix:
            print("%s, wid %d [%d->%d], target_context_offset %d, block %d [offset=%d context_offset=%d]" %
                  (prefix, wid, word_offset_begin, word_offset_end, target_context_offset,
                   a, blocks[a]["table_offset"], blocks[a]["context_offset"]))

        # parse block to find the line with the right context
        data = blocks[a]["data"]
        block_offset = offset = blocks[a]["table_offset"]
        end_offset = blocks[a + 1]["table_offset"]
        context_offset = blocks[a]["context_offset"]

        br = BitReader(data, base_bits = self.base_bits)
        while offset < end_offset:

            stored_context_offset = cur_context_offset = br.get_variable_length()
            count = br.get_variable_length()

            if offset == block_offset: cur_context_offset = context_offset  # first block item is easy: just get context information from the block header
            elif offset > word_offset_begin: cur_context_offset += context_offset  # we store context offset deltas (except for first context of each word)

            ok = (offset >= word_offset_begin and offset <= word_offset_end and cur_context_offset == target_context_offset)

            if prefix:
                print("%s,  ... offset %d, context_offset %d [%d] (count=%d)%s" %
                      (prefix, offset, cur_context_offset if offset >= word_offset_begin else -1, stored_context_offset, count,
                       " OK" if ok else ""))

            if ok: return offset, count  # found ! :-)

            if offset > word_offset_end or (cur_context_offset > target_context_offset and offset >= word_offset_begin):
                return None, None  # oops we went too far

            offset += 1
            context_offset = cur_context_offset


        return None, None  # not found

    def search(self, ngram):
        context_offset = None
        ti = 0
        count = None
        if self.verbose: print("Search: %s" % list(ngram))
        for w in ngram:
            context_offset, count = self.table_lookup(self.table[ti], w, context_offset, prefix = "    table n=%d" % ti if self.verbose else None)
            if context_offset is None: break  # not found
            ti += 1

        if self.verbose:
            print("Search: %s -> %s" % (list(ngram), count))
            print()

        return count
