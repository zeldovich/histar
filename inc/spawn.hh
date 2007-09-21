#ifndef JOS_INC_SPAWN_HH
#define JOS_INC_SPAWN_HH

extern "C" {
#include <inc/lib.h>
}

#include <inc/cpplabel.hh>

#define SPAWN_NO_AUTOGRANT	0x01	/* user_grant & user_taint */
#define SPAWN_UINIT_STYLE	0x02

struct spawn_descriptor {
 public:
    spawn_descriptor() : 
	ct_(0), elf_ino_(),
	fd0_(0), fd1_(0), fd2_(0),
	ac_(0), av_(0), envc_(0), envv_(0),
	cs_(0), ds_(0), cr_(0), dr_(0), co_(0),
	spawn_flags_(0), 
	fs_mtab_seg_(COBJ(0,0)),
	fs_root_(), fs_cwd_()
    {
	elf_ino_.obj = COBJ(0, 0);
	fs_root_.obj = COBJ(0, 0);
	fs_cwd_.obj = COBJ(0, 0);
    }

    uint64_t ct_;
    struct fs_inode elf_ino_;

    int fd0_;
    int fd1_;
    int fd2_;
    
    int ac_;
    const char **av_;
    int envc_;
    const char **envv_;

    label *cs_;
    label *ds_;
    label *cr_;
    label *dr_;
    label *co_;

    int spawn_flags_;
    struct cobj_ref fs_mtab_seg_;
    struct fs_inode fs_root_;
    struct fs_inode fs_cwd_;

 private:
    spawn_descriptor(const spawn_descriptor&);
    spawn_descriptor &operator=(const spawn_descriptor&);
};

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
	  int spawn_flags = 0,
	  struct cobj_ref fs_mtab_seg = COBJ(0, 0)
	);

struct child_process spawn(spawn_descriptor *sd);

#endif
