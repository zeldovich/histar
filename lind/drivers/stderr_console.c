#include <linux/init.h>
#include <linux/console.h>

#include <archcall.h>

static int use_stderr_console = 1;

static void stderr_console_write(struct console *console, const char *string,
				 unsigned len)
{
	arch_write_stderr(string, len);
}

static struct console stderr_console = {
	.name		= "stderr",
	.write		= stderr_console_write,
	.flags		= CON_PRINTBUFFER,
};

static int __init stderr_console_init(void)
{
	if (use_stderr_console)
		register_console(&stderr_console);
	return 0;
}
console_initcall(stderr_console_init);

static int stderr_setup(char *str)
{
	if (!str)
		return 0;
	use_stderr_console = simple_strtoul(str,&str,0);
	return 1;
}
__setup("stderr=", stderr_setup);
