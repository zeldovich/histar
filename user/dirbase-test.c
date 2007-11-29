#include <stdio.h>
#include <string.h>
#include <inc/fs.h>

static void
t(const char *s, const char *dx, const char *bx)
{
    char buf[1024];
    strcpy(&buf[0], s);

    const char *dir, *base;
    fs_dirbase(&buf[0], &dir, &base);

    const char *stat = (strcmp(dir, dx) || strcmp(base, bx))
	? "*** FAIL" : "    PASS";
    printf("%s: %s -> %s %s\n", stat, s, dir, base);
}

int
main(int ac, char **av)
{
    t("/", "/", "");
    t("/x", "/", "x");
    t("//x", "/", "x");
    t("//x/", "/", "x");
    t("//x///", "/", "x");
    t("////x/////", "/", "x");
    t("x", "", "x");
    t("x/", "", "x");
    t("x//", "", "x");
    t("x///", "", "x");
    t("x/y", "x", "y");
    t("x//y//", "x", "y");
    t("//x//y", "//x", "y");
    t("//x/y/", "//x", "y");
    t("/x//y///", "/x", "y");
    t("x/y/z", "x/y", "z");
    t("x//y//z", "x//y", "z");
    t("x/y/z//", "x/y", "z");
    t("//x//y/z", "//x//y", "z");
    t("/x/y//z//", "/x/y", "z");
}
