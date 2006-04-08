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
#include <inc/gateclnt.hh>

enum { db_debug_visible = 1 };

static cobj_ref db_as;
static int64_t db_table_grant;
static int64_t db_table_ct;
static cobj_ref db_table_seg;

struct db_table_info {
    uint64_t dbt_next_id;
};

static void __attribute__((noreturn))
db_row_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    try {
	uint64_t row_seg_id = (uint64_t) arg;
	cobj_ref row_seg = COBJ(db_table_ct, row_seg_id);

	db_row *row = 0;
	error_check(segment_map(row_seg, SEGMAP_READ, (void **) &row, 0));
	scope_guard<int, void*> unmap(segment_unmap, row);

	db_row *qry = 0;
	uint64_t qbytes = 0;
	error_check(segment_map(gcd->param_obj, SEGMAP_READ, (void **) &qry, &qbytes));
	scope_guard<int, void*> unmap2(segment_unmap, qry);

	if (qbytes != sizeof(*qry))
	    throw basic_exception("bad query segment length, %ld != %ld\n",
				  qbytes, sizeof(*qry));

	db_row *out = 0;
	error_check(segment_alloc(gcd->taint_container, sizeof(*row),
				  &gcd->param_obj, (void **) &out, 0, 0));
	scope_guard<int, void*> unmap3(segment_unmap, out);

	out->dbr_taint = row->dbr_taint;
	out->dbr_id = row->dbr_id;
	out->dbr_zipcode = row->dbr_zipcode;

	for (int i = 0; i < db_row_match_ents; i++)
	    out->dbr_match_dot += qry->dbr_match_vector[i] * row->dbr_match_vector[i];

	memcpy(&out->dbr_nickname[0],
	       &row->dbr_nickname[0],
	       sizeof(out->dbr_nickname));
    } catch (std::exception &e) {
	cprintf("db_row_entry: %s\n", e.what());
    }

    gr->ret(0, 0, 0);
}

static void
db_insert(label *v, db_query *dbq, db_reply *dbr)
{
    error_check(sys_self_addref(db_table_ct));
    scope_guard<int, cobj_ref> unref(sys_obj_unref, COBJ(db_table_ct, thread_id()));

    db_row *row = 0;
    uint64_t row_bytes = 0;
    error_check(segment_map(dbq->obj, SEGMAP_READ | SEGMAP_WRITE,
		(void **) &row, &row_bytes));
    scope_guard<int, void*> unmap(segment_unmap, row);

    if (row_bytes != sizeof(*row))
	throw error(-E_INVAL, "bad row segment size %ld\n", row_bytes);
    if (v->get(row->dbr_taint) != LB_LEVEL_STAR)
	throw error(-E_INVAL, "verify label doesn't have taint handle");

    db_table_info *dbt = 0;
    uint64_t dbt_bytes = 0;
    error_check(segment_map(db_table_seg, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &dbt, &dbt_bytes));
    scope_guard<int, void*> unmap2(segment_unmap, dbt);

    row->dbr_id = dbt->dbt_next_id++;

    char name[KOBJ_NAME_LEN];
    snprintf(&name[0], sizeof(name), "row %ld", row->dbr_id);

    label row_label(1);
    row_label.set(db_table_grant, 0);
    row_label.set(row->dbr_taint, 3);

    int64_t copy_id;
    error_check(copy_id = sys_segment_copy(dbq->obj, db_table_ct,
					   row_label.to_ulabel(), name));
    scope_guard<int, cobj_ref> row_drop(sys_obj_unref, COBJ(db_table_ct, copy_id));

    label gate_clear(2);
    gate_clear.set(db_table_grant, 0);

    // Gate label: { P_T:*, DB_G:*, Row_T:*, ..., 1 }
    label gate_label;
    thread_cur_label(&gate_label);

    gatesrv_descriptor gd;
    gd.gate_container_ = db_table_ct;
    gd.name_ = name;
    gd.as_ = db_as;
    gd.label_ = &gate_label;
    gd.clearance_ = &gate_clear;
    gd.func_ = &db_row_entry;
    gd.arg_ = (void *) copy_id;

    gate_create(&gd);
    row_drop.dismiss();
}

static void
db_lookup(label *v, db_query *dbq, db_reply *dbr, uint64_t reply_ct)
{
    int64_t ct_slots;
    error_check(ct_slots = sys_container_get_nslots(db_table_ct));

    cobj_ref reply_seg;
    error_check(segment_alloc(reply_ct, 0, &reply_seg,
			      0, 0, "db reply"));
    uint64_t out_rows = 0;

    for (int64_t i = 0; i < ct_slots; i++) {
	int64_t id = sys_container_get_slot_id(db_table_ct, i);
	if (id == -E_NOT_FOUND)
	    continue;
	if (id < 0)
	    throw error(id, "sys_container_get_slot_id");

	int64_t type;
	error_check(type = sys_obj_get_type(COBJ(db_table_ct, id)));
	if (type != kobj_gate)
	    continue;

	gate_call_data gcd;
	memset(&gcd, 0, sizeof(gcd));
	gcd.param_obj = dbq->obj;

	gate_call gc(COBJ(db_table_ct, id), 0, 0, 0);
	gc.call(&gcd, 0);

	db_row *row = 0;
	error_check(segment_map(gcd.param_obj, SEGMAP_READ, (void **) &row, 0));
	scope_guard<int, void *> unmap(segment_unmap, row);

	out_rows++;
	uint64_t out_bytes = out_rows * sizeof(*row);
	error_check(sys_segment_resize(reply_seg, out_bytes, 0));

	db_row *out = 0;
	error_check(segment_map(reply_seg, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &out, &out_bytes));
	scope_guard<int, void *> unmap2(segment_unmap, out);

	memcpy(&out[out_rows - 1], row, sizeof(*row));
    }

    dbr->obj = reply_seg;
}

static void
db_handle_query(db_query *dbq, db_reply *dbr, uint64_t reply_ct)
{
    label verify;
    thread_cur_verify(&verify);

    switch (dbq->reqtype) {
    case db_req_insert:
	db_insert(&verify, dbq, dbr);
	break;

    case db_req_lookup_all:
	db_lookup(&verify, dbq, dbr, reply_ct);
	break;

    default:
	throw error(-E_INVAL, "unknown req type %d", dbq->reqtype);
    }
}

static void __attribute__((noreturn))
db_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    db_query dbq = *(db_query *) &gcd->param_buf[0];
    db_reply *dbr = (db_reply *) &gcd->param_buf[0];

    try {
	db_reply db_rep;
	memset(&db_rep, 0, sizeof(db_rep));

	db_handle_query(&dbq, &db_rep, gcd->taint_container);

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
    error_check(sys_self_get_as(&db_as));
    error_check((db_table_grant = sys_handle_create()));

    label db_table_label(1);
    if (!db_debug_visible)
	db_table_label.set(start_env->process_taint, 3);
    db_table_label.set(db_table_grant, 0);

    error_check((db_table_ct =
	sys_container_alloc(start_env->shared_container,
			    db_table_label.to_ulabel(),
			    "db table", 0)));

    error_check(segment_alloc(db_table_ct, sizeof(db_table_info),
			      &db_table_seg, 0, db_table_label.to_ulabel(),
			      "table metadata"));

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
