extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/db_schema.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: dbc (insert|lookup)\n");
    exit(-1);
}

int
main(int ac, char **av)
try
{
    if (ac != 2)
	usage();

    int64_t db_ct, db_gt;
    error_check(db_ct = container_find(start_env->root_container, kobj_container, "db"));
    error_check(db_gt = container_find(db_ct, kobj_gate, "db gate"));
    cobj_ref g = COBJ(db_ct, db_gt);

    const char *opstr = av[1];

    gate_call_data gcd;
    db_query *dbq = (db_query *) &gcd.param_buf[0];
    db_row *dbr = 0;

    if (!strcmp(opstr, "insert")) {
	label row_l(1);

	dbq->reqtype = db_req_insert;
	error_check(segment_alloc(start_env->shared_container, sizeof(*dbr),
				  &dbq->obj, (void **) &dbr,
				  row_l.to_ulabel(), "insert row"));

	if (!start_env->user_taint)
	    throw basic_exception("not logged into authd");

	dbr->dbr_taint = start_env->user_taint;
	dbr->dbr_zipcode = 95060;
	sprintf(&dbr->dbr_nickname[0], "Test nickname");
	sprintf(&dbr->dbr_name[0], "Real name, or not");
	for (int i = 0; i < db_row_match_ents; i++)
	    dbr->dbr_match_vector[i] = i % 64;

	label grant_taint(3);
	grant_taint.set(dbr->dbr_taint, LB_LEVEL_STAR);

	gate_call(g, 0, &grant_taint, 0).call(&gcd, &grant_taint);
    } else if (!strcmp(opstr, "lookup")) {
	dbq->reqtype = db_req_lookup_all;
	error_check(segment_alloc(start_env->shared_container, sizeof(*dbr),
				  &dbq->obj, (void **) &dbr, 0, "query row"));
	dbr->dbr_zipcode = 95060;
	for (int i = 0; i < db_row_match_ents; i++)
	    dbr->dbr_match_vector[i] = (i * 2 + 5) % 64;

	int64_t query_tainth;
	error_check(query_tainth = handle_alloc());

	label query_taint(0);
	query_taint.set(query_tainth, 2);

	gate_call gc(g, &query_taint, 0, 0);
	gc.call(&gcd, 0);

	dbr = 0;
	uint64_t reply_bytes = 0;
	error_check(segment_map(dbq->obj, 0, SEGMAP_READ, (void **) &dbr, &reply_bytes, 0));
	for (uint32_t i = 0; i < reply_bytes / sizeof(*dbr); i++) {
	    printf("ID: %ld, Zip: %d, Nick: %s, Dot-product: %d\n",
		    dbr[i].dbr_id,
		    dbr[i].dbr_zipcode,
		    dbr[i].dbr_nickname,
		    dbr[i].dbr_match_dot);
	}
    } else {
	usage();
    }

    return 0;
} catch (std::exception &e) {
    printf("dbc: %s\n", e.what());
    return -1;
}
