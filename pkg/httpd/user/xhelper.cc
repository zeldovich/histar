extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
}

#include <inc/error.hh>
#include <inc/errno.hh>
#include <dj/gatesender.hh>
#include <dj/djops.hh>

static void
write_to_file(const char *pn, const strbuf &sb)
{
    int fd = open(pn, O_RDWR | O_CREAT, 0666);
    errno_check(fd);

    str s(sb);
    write(fd, s.cstr(), s.len());
    close(fd);
}

int
main(int ac, char **av)
{
    gate_sender gs;

    strbuf gs_key;
    gs_key << gs.hostkey();

    int64_t ct, id;
    error_check(ct = container_find(start_env->root_container, kobj_container, "djechod"));
    error_check(id = container_find(ct, kobj_container, "public call"));
    strbuf callct;
    callct << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djauthproxy"));
    error_check(id = container_find(ct, kobj_gate, "authproxy"));
    strbuf authgate;
    authgate << ct << "." << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djwebappd"));
    error_check(id = container_find(ct, kobj_gate, "djwebappd"));
    strbuf appgate;
    appgate << ct << "." << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djfsd"));
    error_check(id = container_find(ct, kobj_gate, "djfsd"));
    strbuf fsgate;
    fsgate << ct << "." << id << "\n";

    write_to_file("/www/dj_user_host", gs_key);
    write_to_file("/www/dj_app_host", gs_key);
    write_to_file("/www/dj_user_ct", callct);
    write_to_file("/www/dj_app_ct", callct);
    write_to_file("/www/dj_user_authgate", authgate);
    write_to_file("/www/dj_user_fsgate", fsgate);
    write_to_file("/www/dj_app_gate", appgate);

    system("adduser alice foo");
    system("adduser bob bar");
    warn << "All done, alice/foo, bob/bar\n";
}
