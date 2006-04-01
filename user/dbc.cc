extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/db_schema.hh>

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
	dbq->reqtype = db_req_insert;
	error_check(segment_alloc(start_env->shared_container, sizeof(*dbr),
				  &dbq->obj, (void **) &dbr, 0, "insert row"));

	if (!start_env->user_taint)
	    throw basic_exception("not logged into authd");

	dbr->dbr_taint = start_env->user_taint;
	dbr->dbr_zipcode = 95060;
	sprintf(&dbr->dbr_nickname[0], "Test nickname");
	sprintf(&dbr->dbr_name[0], "Real name, or not");
	for (int i = 0; i < 256; i++)
	    dbr->dbr_match_vector[i] = i % 64;

	label grant_taint(3);
	grant_taint.set(dbr->dbr_taint, LB_LEVEL_STAR);

	gate_call(g, &gcd, 0, &grant_taint, 0, &grant_taint);
    } else if (!strcmp(opstr, "lookup")) {
	dbq->reqtype = db_req_lookup_zip;
	error_check(segment_alloc(start_env->shared_container, sizeof(*dbr),
				  &dbq->obj, (void **) &dbr, 0, "query row"));
	dbr->dbr_zipcode = 95060;

	gate_call gc(g, &gcd, 0, 0, 0, 0);

	dbr = 0;
	uint64_t reply_bytes;
	error_check(segment_map(dbq->obj, SEGMAP_READ, (void **) &dbr, &reply_bytes));
	for (uint32_t i = 0; i < reply_bytes / sizeof(*dbr); i++) {
	    printf("ID %ld, Zip %d, Nick %s\n",
		    dbr[i].dbr_id,
		    dbr[i].dbr_zipcode,
		    dbr[i].dbr_nickname);
	}
    } else {
	usage();
    }

    return 0;
} catch (std::exception &e) {
    printf("dbc: %s\n", e.what());
    return -1;
}
