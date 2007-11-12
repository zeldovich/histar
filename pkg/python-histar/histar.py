from _histar import *

import os.path

def open_with_label(name, mode='r', buffering=-1, label=None):
    if mode.startswith('w') and label != None:
        dirname = os.path.dirname(os.path.abspath(name))
        filename = os.path.basename(os.path.abspath(name))
        fs_create(dirname, filename, label)
    return open(name, mode, buffering)

