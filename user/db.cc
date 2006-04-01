extern "C" {
#include <inc/syscall.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/error.h>

#include <stdio.h>
#include <string.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/db_schema.hh>
#include <inc/scopeguard.hh>

static int64_t db_table_grant;
static int64_t db_table_ct;

static void
db_insert(label *v, db_query *dbq, db_reply *dbr, uint64_t reply_ct)
{
    error_check(sys_self_addref(db_table_ct));
    scope_guard<int, cobj_ref> unref(sys_obj_unref, COBJ(db_table_ct, thread_id()));

    db_row *row = 0;
    uint64_t row_bytes;
    error_check(segment_map(dbq->obj, SEGMAP_READ, (void **) &row, &row_bytes));
    if (row_bytes != sizeof(*row))
	throw error(-E_INVAL, "bad row segment size %ld\n", row_bytes);
    if (v->get(row->dbr_taint) != LB_LEVEL_STAR)
	throw error(-E_INVAL, "verify label doesn't have taint handle");

    cprintf("inserting row, somehow..\n");
    // ...
}

static void
db_lookup(db_query *dbq, db_reply *dbr)
{
    // traverse gates in db_table_ct.
    // for each gate, ask it for its public information.
    // match on zip code, append to result segment.
}

static void
db_handle_query(db_query *dbq, db_reply *dbr, uint64_t reply_ct)
{
    label verify;
    thread_cur_verify(&verify);

    switch (dbq->reqtype) {
    case db_req_insert:
	db_insert(&verify, dbq, dbr, reply_ct);
	break;

    case db_req_lookup_zip:
	db_lookup(dbq, dbr);
	break;

    default:
	throw error(-E_INVAL, "unknown req type %d", dbq->reqtype);
    }
}

static void __attribute__((noreturn))
db_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    db_query *dbq = (db_query *) &gcd->param_buf[0];
    db_reply *dbr = (db_reply *) &gcd->param_buf[0];

    try {
	db_reply db_rep;
	memset(&db_rep, 0, sizeof(db_rep));

	db_handle_query(dbq, &db_rep, gcd->taint_container);

	*dbr = db_rep;
    } catch (error &e) {
	cprintf("db_entry: %s\n", e.what());
	dbr->status = e.err();
    } catch (std::exception &e) {
	cprintf("db_entry: %s\n", e.what());
	dbr->status = -1;
    }

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

    error_check((db_table_ct =
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
