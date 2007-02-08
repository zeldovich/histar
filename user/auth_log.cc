extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>

#include <stdio.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

static void __attribute__((noreturn))
authlog_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    parm->param_buf[sizeof(parm->param_buf) - 1] = '\0';
    FILE *f = fopen("/authlog_self/log/log.out", "a");
    fprintf(f, "%s\n", parm->param_buf);
    fclose(f);
    gr->ret(0, 0, 0);
}

int
main(int ac, char **av)
{
    try {
	int64_t new_mtab_id =
	    sys_segment_copy(start_env->fs_mtab_seg, start_env->shared_container,
			     0, "private mount table");
	error_check(new_mtab_id);
	start_env->fs_mtab_seg = COBJ(start_env->shared_container, new_mtab_id);

	fs_inode shared_ct;
	fs_get_root(start_env->shared_container, &shared_ct);
	fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "authlog_self", shared_ct);

	int64_t log_h;
	error_check(log_h = handle_alloc());
	label log_label(1);
	log_label.set(log_h, 0);

	fs_inode log_dir;
	fs_mkdir(shared_ct, "log", &log_dir, log_label.to_ulabel());

	label gt_label;
	label gt_clear;
	thread_cur_label(&gt_label);
	thread_cur_clearance(&gt_clear);
	gt_label.set(start_env->process_grant, 1);

	gate_create(start_env->shared_container, "authlog",
		    &gt_label, &gt_clear, 0,
		    &authlog_entry, 0);
	process_report_exit(0, 0);
    	thread_halt();
    } catch (std::exception &e) {
    	printf("authd: %s\n", e.what());
    	return -1;
    }
}
