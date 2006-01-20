// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <machine/x86.h>
#include <machine/pmap.h>
#include <kern/lib.h>
#include <dev/console.h>
#include <kern/monitor.h>
#if 0
#include <kern/trap.h>
#include <kern/stabs.h>
#endif

#define CMDBUF_SIZE	80	// enough for one VGA text line

static int mon_exit (int argc, char **argv, struct Trapframe *tf);

struct Command
{
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func) (int argc, char **argv, struct Trapframe * tf);
};

static struct Command commands[] = {
  {"help", "Display this list of commands", mon_help},
  {"kerninfo", "Display information about the kernel", mon_kerninfo},
  {"backtrace", "Display a stack backtrace", mon_backtrace},
  {"bt", "Display a stack backtrace", mon_backtrace},
  {"exit", "Exit the kernel monitor", mon_exit},
};

#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))



/***** Implementations of basic kernel monitor commands *****/

int
mon_help (int argc, char **argv, struct Trapframe *tf)
{
  for (uint32_t i = 0; i < NCOMMANDS; i++)
    cprintf ("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int
mon_kerninfo (int argc, char **argv, struct Trapframe *tf)
{
  extern char start[], etext[], edata[], end[];

  cprintf ("Special kernel symbols:\n");
  cprintf ("  start %p (virt)  %p (phys)\n", start, start - KERNBASE);
  cprintf ("  etext  %p (virt)  %p (phys)\n", etext, etext - KERNBASE);
  cprintf ("  edata  %p (virt)  %p (phys)\n", edata, edata - KERNBASE);
  cprintf ("  end    %p (virt)  %p (phys)\n", end, end - KERNBASE);
  cprintf ("Kernel executable memory footprint: %ldKB\n",
	   (end - start + 1023) / 1024);
  return 0;
}

int
mon_backtrace (int argc, char **argv, struct Trapframe *tf)
{
#if 0
  const uint32_t *ebp =
    (tf ? (const uint32_t *) tf->tf_ebp : (const uint32_t *) read_ebp ());
  int fr = 0;
  struct Eipinfo info;

  while (ebp > (const uint32_t *) 0x800000) {
    /* arbitrary cutoff */

    // print this stack frame
    cprintf ("%3d: eip %08x  ebp %08x  args", fr, ebp[1], ebp[0]);
    for (int i = 0; i < 4; i++)
      cprintf (" %08x", ebp[2 + i]);
    cprintf ("\n");

    if (stab_eip (ebp[1], &info) >= 0)
      cprintf ("         %s:%d: %.*s+%x\n", info.eip_file, info.eip_line,
	       info.eip_fnlen, info.eip_fn, ebp[1] - info.eip_fnaddr);
    else
      break;

    // move to next lower stack frame
    ebp = (const uint32_t *) ebp[0];
    fr++;
  }

#endif
  return 0;
}

int
mon_exit (int argc, char **argv, struct Trapframe *tf)
{
  return -1;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

/*static*/ int
runcmd (char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr (WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS - 1) {
      cprintf ("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr (WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (uint32_t i = 0; i < NCOMMANDS; i++) {
    if (strcmp (argv[0], commands[i].name) == 0)
      return commands[i].func (argc, argv, tf);
  }
  cprintf ("Unknown command '%s'\n", argv[0]);
  return 0;
}

void
monitor (struct Trapframe *tf)
{
  //char *buf;

  cprintf ("Welcome to the JOS kernel monitor!\n");
  cprintf ("Type 'help' for a list of commands.\n");

#if 0
  if (tf != NULL)
    print_trapframe (tf);

  while (1) {
    buf = readline ("K> ");
    if (buf != NULL)
      if (runcmd (buf, tf) < 0)
	break;
  }
#endif
}
