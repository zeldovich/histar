#include <linux/module.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/binfmts.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/err.h>
#include <linux/fs.h>

static int 
load_lindfmt(struct linux_binprm *bprm,struct pt_regs *regs)
{
    return 0;
}

static struct linux_binfmt script_format = {
    .module		= THIS_MODULE,
    .load_binary	= load_lindfmt,
};

static int __init 
init_lind_binfmt(void)
{
    return register_binfmt(&script_format);
}

static void __exit 
exit_lind_binfmt(void)
{
    unregister_binfmt(&script_format);
}

core_initcall(init_lind_binfmt);
module_exit(exit_lind_binfmt);
