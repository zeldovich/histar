#!/bin/sh
qemu-system-x86_64 \
	-serial stdio \
	-hda ./obj/kern/bochs.img \
	-hdb ./obj/fs/fs.img \
	-m 128 \
	-redir tcp:9923::23 \
	-redir tcp:9922::22 \
	-redir tcp:9980::80
