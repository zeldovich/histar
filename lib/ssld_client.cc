extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/ssld.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/fd.h>
#include <inc/netd.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/spawn.hh>
#include <inc/ssldclnt.hh>

const char *default_server_pem = "/bin/server.pem";
const char *default_password = "password";
const char *default_calist_pem = "/bin/root.pem";
const char *default_dh_pem = "/bin/dh.pem";

static uint64_t shared_ct;
static struct cobj_ref ssld_shared_gate;

static const char nullio = 0;

struct cobj_ref
ssld_new_server(uint64_t ct, const char *server_pem, const char *password, 
		const char *calist_pem, const char *dh_pem,
		label *cs, label *ds, label *cr, label *dr, label *co)
{
    struct fs_inode ssl_ino;
    error_check(fs_namei("/bin/ssld", &ssl_ino));

    int io;
    if (nullio) {
	int nullfd = open("/dev/null", O_RDONLY);
	if (nullfd < 0)
	    throw basic_exception("cannot open /dev/null: %s\n", strerror(errno));
	io = nullfd;
    } else
	io = 0;
    
    const char *argv[] = { "ssld", server_pem, password, dh_pem, calist_pem };
    struct child_process cp = spawn(ct, ssl_ino,
				    io, io, io,
				    5, &argv[0],
				    0, 0,
				    cs, ds, cr, dr, co);
    int64_t exit_code = 0;
    process_wait(&cp, &exit_code);
    if (exit_code)
	throw error(exit_code, "error starting ssld");

    int64_t ssld_gt;
    error_check(ssld_gt = container_find(cp.container, kobj_gate, "ssld"));
    
    return COBJ(cp.container, ssld_gt);
}

struct cobj_ref
ssld_shared_server(void)
{
    if (shared_ct != start_env->shared_container ||
	ssld_shared_gate.object == 0) {
	
	ssld_shared_gate = ssld_new_server(start_env->shared_container,
					   default_server_pem, default_password, 
					   default_calist_pem, default_dh_pem,
					   0, 0, 0, 0, 0);
	shared_ct = start_env->shared_container;
    }
    return ssld_shared_gate;
}

// XXX requires that ssld_shared_server be called
struct cobj_ref
ssld_shared_cow(void)
{
    assert(shared_ct == start_env->shared_container);
    
    int64_t cow_gt;
    error_check(cow_gt = container_find(ssld_shared_gate.container, kobj_gate, "ssld-cow"));
    return COBJ(ssld_shared_gate.container, cow_gt);
}

struct cobj_ref
ssld_cow_call(struct cobj_ref gate, uint64_t root_ct, 
	      label *cs, label *ds, label *dr)
{
    gate_call c(gate, cs, ds, dr);
    
    struct gate_call_data gcd;
    int64_t *arg = (int64_t *)gcd.param_buf;
    *arg = root_ct;
    
    c.call(&gcd, 0);
    error_check(*arg);
    
    return COBJ(root_ct, *arg);
}

extern "C" int
ssld_call(struct cobj_ref gate, struct ssld_op_args *a)
{
    try {
	gate_call c(gate, 0, 0, 0);
	
	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(c.call_ct(), sizeof(*a), &seg, &va,
				  0, "netd_call() args"));
	memcpy(va, a, sizeof(*a));
	segment_unmap(va);
	
	struct gate_call_data gcd;
	gcd.param_obj = seg;
	c.call(&gcd, 0);
	
	va = 0;
	error_check(segment_map(gcd.param_obj, 0, SEGMAP_READ, &va, 0, 0));
	memcpy(a, va, sizeof(*a));
	segment_unmap(va);
    } catch (error &e) {
	cprintf("ssld_call: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("ssld_call: %s\n", e.what());
	return -1;
    }
    return a->rval;
}
