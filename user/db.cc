extern "C" {
#include <inc/syscall.h>
#include <inc/gateparam.h>

#include <stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/db_schema.hh>

static int64_t db_table_grant;
static int64_t db_table_rows_ct;

static void __attribute__((noreturn))
db_entry(void *arg, struct gate_call_data *gcd, gatesrv_return *gr)
{
    gr->ret(0, 0, 0);
}

int
main(int ac, char **av)
try
{
    error_check((db_table_grant = sys_handle_create()));

    label db_table_label(1);
    db_table_label.set(start_env->process_taint, 3);
    db_table_label.set(db_table_grant, 0);

    error_check((db_table_rows_ct =
	sys_container_alloc(start_env->shared_container,
			    db_table_label.to_ulabel(),
			    "db table")));

    label th_label, th_clear;
    thread_cur_label(&th_label);
    thread_cur_clearance(&th_clear);

    // Protect the gate entry state (text/data segment,
    // initial address space, etc).
    th_label.set(start_env->process_grant, 1);

    struct cobj_ref g =
	gate_create(start_env->shared_container, "db gate",
		    &th_label, &th_clear,
		    &db_entry, 0);

    thread_halt();
} catch (std::exception &e) {
    printf("db: %s\n", e.what());
    return -1;
}
