/* this is the place to simulate an OpenBoot PROM */

unsigned int romvec;

void __main(void);

void __attribute__ ((noreturn))
__main(void)
{ 
    extern void start(unsigned int);
    start(romvec);

    for (;;) {}
}
