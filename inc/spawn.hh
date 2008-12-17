#ifndef JOS_INC_SPAWN_HH
#define JOS_INC_SPAWN_HH

extern "C" {
#include <inc/lib.h>
}

#include <inc/cpplabel.hh>

#define SPAWN_NO_AUTOGRANT	0x01	/* user_grant & user_taint */
#define SPAWN_UINIT_STYLE	0x02
#define SPAWN_COPY_MTAB		0x04

struct spawn_descriptor {
 public:
    spawn_descriptor() : 
	ct_(0), ctname_(0), elf_ino_(),
	fd0_(0), fd1_(0), fd2_(0),
	ac_(0), av_(0), envc_(0), envv_(0),
	taint_(0), owner_(0), clear_(0),
	spawn_flags_(0), 
	fs_mtab_seg_(COBJ(0,0)),
	fs_root_(), fs_cwd_()
    {
	elf_ino_.obj = COBJ(0, 0);
	fs_root_.obj = COBJ(0, 0);
	fs_cwd_.obj = COBJ(0, 0);
    }

    uint64_t ct_;
    const char *ctname_;
    struct fs_inode elf_ino_;

    int fd0_;
    int fd1_;
    int fd2_;
    
    int ac_;
    const char **av_;
    int envc_;
    const char **envv_;

    label *taint_;
    label *owner_;
    label *clear_;

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
	  label *taint, label *owner, label *clear,
	  int spawn_flags = 0,
	  struct cobj_ref fs_mtab_seg = COBJ(0, 0)
	);

struct child_process spawn(spawn_descriptor *sd);

#endif
