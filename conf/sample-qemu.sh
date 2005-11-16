#!/bin/sh
qemu-system-x86_64 -parallel stdio -hda ./obj/kern/bochs.img -hdb ./obj/fs/fs.img -m 32
