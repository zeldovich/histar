#include <sfslite/callback.h>

void
printstrings (const char *a, const char *b, const char *c)
{
    printf ("%s %s %s\n", a, b, c);
}
int
main ()
{
    callback<void, const char *>::ref cb1 = wrap (printstrings, "cb1a", "cb1b");
    callback<void, const char *, const char *>::ref cb2 = wrap (printstrings, "cb2a");
    callback<void, const char *, const char *, const char *>::ref cb3 = wrap (printstrings);
    (*cb1) ("cb1c");        
    (*cb2) ("cb2b", "cb2c");          // prints: cb2a cb2b cb2c
    (*cb3) ("cb3a", "cb3b", "cb3c");  // prints: cb3a cb3b cb3c
    return 0;
}
