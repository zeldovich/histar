void __main(void);

void __attribute__ ((__section__ (".prom_stage2.text"), noreturn))
__main(void)
{ 
    for (;;) {}
}
