#ifndef JOS_INC_SPAWN_HH
#define JOS_INC_SPAWN_HH

extern "C" {
#include <inc/lib.h>
}

#include <inc/cpplabel.hh>

#define SPAWN_NO_AUTOGRANT	0x01
#define SPAWN_UINIT_STYLE	0x02

struct child_process
    spawn(uint64_t container, struct fs_inode elf,
	  int fd0, int fd1, int fd2,
	  int ac, const char **av,
	  int envc, const char **envv,
	  label *cs,	// null is effectively { * }
	  label *ds,	// null is effectively { 3 }
	  label *cr,	// null is effectively { 3 }
	  label *dr,	// null is effectively { 0 }
	  label *co,	// null is effectively { 0 } -- contaminate objects
	  int spawn_flags = 0
	);

#endif
