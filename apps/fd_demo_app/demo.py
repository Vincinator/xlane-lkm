#!/usr/bin/env python3


import mmap

counter = 0


with open('/dev/asgard_fd_tx_mem6', 'r+') as f:
    with mmap.mmap(f.fileno(), 0) as m:
        print('Before:\n{}'.format(m.rstrip()))
        m.seek(0)  # rewind

        loc = m.find(word)
        m[2] = counter
        
        counter += 1

        m.flush()
        m.seek(0)  # rewind
        f.seek(0)  # rewind
        