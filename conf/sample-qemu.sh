#!/bin/sh
qemu-system-x86_64 \
	-serial stdio \
	-hda ./obj/boot/bochs.img \
	-hdb ./obj/fs/fs.img \
	-m 256 \
	-redir tcp:9923::23 \
	-redir tcp:9922::22 \
	-redir tcp:9980::80

# To run qemu from an ISO image:
#
# qemu-system-x86_64 -cdrom obj/boot/boot.iso ...

