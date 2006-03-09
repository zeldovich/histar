extern "C" {
#include <inc/syscall.h>
#include <stdio.h>
}

#include <inc/privstore.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

enum { nhandles = 5 };
int64_t handles[nhandles];

static void
print_label()
{
    label l;
    thread_cur_label(&l);
    printf("%s\n", l.to_string());
}

int
main(int ac, char **av)
try
{
    int64_t h = sys_handle_create();
    error_check(h);
    privilege_store ps(h);

    printf("Starting, ps handle %lu\n", h);
    print_label();

    for (int i = 0; i < nhandles; i++) {
	handles[i] = sys_handle_create();
	error_check(handles[i]);
	ps.store_priv(handles[i]);

	printf("Stored %d (%lu)\n", i, handles[i]);
	print_label();

	thread_drop_star(handles[i]);
    }

    for (int i = 0; i < nhandles; i++) {
	ps.fetch_priv(handles[i]);

	printf("Fetched %d (%lu)\n", i, handles[i]);
	print_label();

	thread_drop_star(handles[i]);
    }

    printf("All done\n");
    print_label();
} catch (std::exception &e) {
    printf("Exception: %s\n", e.what());
}
