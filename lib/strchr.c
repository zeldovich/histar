
char *
strchr (const char *p, int ch)
{
  for (;; ++p) {
    if (*p == ch)
      return (char *) p;
    if (!*p)
      return 0;
  }
}
