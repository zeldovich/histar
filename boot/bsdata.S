# Should be in the first sector of the image at offset 0x1F0
.data
pad:		.byte 0
setup_sects:	.byte 1		# sectors used by setup block (setup.S)
root_flags:	.word 0		
syssize:	.word 0
swap_dev:	.word 0
ram_size:	.word 0
vid_mode:	.word 0
root_dev:	.word 0
boot_flag:	.word 0xAA55	# magic boot signature
