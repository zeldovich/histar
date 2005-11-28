#!/bin/sh
qemu-system-x86_64 -serial stdio -hda ./obj/kern/bochs.img -hdb ./obj/fs/fs.img -m 32
