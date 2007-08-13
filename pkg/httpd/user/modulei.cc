extern "C" {
#include <sys/stat.h>
#include <stdlib.h>
}

#include <inc/spawn.hh>
#include <inc/error.hh>

static const char *tar_pn = "/bin/tar";

static void
untar(const char *in_pn, const char *tar_fn)
{
    fs_inode ino;
    error_check(fs_namei(tar_pn, &ino));
    const char *argv[] = { tar_pn, "xm", "-C", in_pn, "-f", tar_fn };
    
    spawn_descriptor sd;
    sd.ct_ = start_env->shared_container;
    sd.elf_ino_ = ino;
    sd.ac_ = 6;
    sd.av_ = &argv[0];

    struct child_process cp = spawn(&sd);
    int64_t exit_code;
    error_check(process_wait(&cp, &exit_code));
    error_check(exit_code);
}

static void
create_ascii(const char *dn, const char *fn, uint64_t len)
{
    struct fs_inode dir;
    error_check(fs_namei(dn, &dir));
    struct fs_inode file;
    error_check(fs_create(dir, fn, &file, 0));

    void *buf = malloc(len);
    memset(buf, 'a', len);
    int r = fs_pwrite(file, buf, len, 0);
    free(buf);
    error_check(r);
}

int
main (int ac, char **av)
{
    untar("/", "/bin/a2ps.tar");
    untar("/", "/bin/gs.tar");
    
    create_ascii("/www/", "test.0", 0);
    create_ascii("/www/", "test.1", 1);
    create_ascii("/www/", "test.1024", 1024);
    create_ascii("/www/", "test.8192", 8192);
}
