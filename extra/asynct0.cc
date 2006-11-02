#include <sfslite/callback.h>

void
printstrings (char *a, char *b, char *c)
{
    printf ("%s %s %s\n", a, b, c);
}
int
main ()
{
    callback<void, char *>::ref cb1 = wrap (printstrings, (char *)"cb1a", (char *)"cb1b");
    callback<void, char *, char *>::ref cb2 = wrap (printstrings, (char *)"cb2a");
    callback<void, char *, char *, char *>::ref cb3 = wrap (printstrings);
    (*cb1) ("cb1c");        
    (*cb2) ("cb2b", "cb2c");          // prints: cb2a cb2b cb2c
    (*cb3) ("cb3a", "cb3b", "cb3c");  // prints: cb3a cb3b cb3c
    return 0;
}
