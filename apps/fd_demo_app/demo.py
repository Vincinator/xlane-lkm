#!/usr/bin/env python3


import mmap

with open('/dev/sassy_fd_tx_mem6', 'r+') as f:
    with mmap.mmap(f.fileno(), 0) as m:
        print('Before:\n{}'.format(m.rstrip()))
        m.seek(0)  # rewind

        loc = m.find(word)
        m[2] = 42
        m.flush()

        m.seek(0)  # rewind
        print('After :\n{}'.format(m.readline().rstrip()))

        f.seek(0)  # rewind
        print('File  :\n{}'.format(f.readline().rstrip()))