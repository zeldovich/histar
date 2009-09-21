#!/bin/sh
branch=$( git symbolic-ref -q HEAD |                sed -e s,refs/heads/,,)
echo Running $branch
qemu-system-x86_64 \
	-serial stdio \
	-m 768 \
	-redir tcp:9923::23 \
	-redir tcp:9922::22 \
	-redir tcp:9980::80 \
	-hda ./obj.$branch.amd64/boot/bochs.img
#	-kernel ./obj.$branch.amd64/boot/bimage.init \
#	-nographic

# To run qemu from an ISO image:
#
# qemu-system-x86_64 -cdrom obj/boot/boot.iso ...

