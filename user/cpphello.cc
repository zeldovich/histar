extern "C" {
#include <inc/lib.h>
#include <stdio.h>
};

#include <inc/cpplabel.hh>
#include <inc/nethelper.hh>
#include <inc/error.hh>

int
main(int ac, char **av)
{
    printf("Hello world -- from C++.\n");

    label l(1);
    l.set(5, LB_LEVEL_STAR);
    l.set(7, 0);
    printf("label says %s\n", l.to_string());

    label *l2 = new label(2);
    l2->set(9, LB_LEVEL_STAR);
    printf("label 2 says %s\n", l2->to_string());

    label o;
    l.merge(l2, &o, label::max, label::leq_starlo);
    printf("max<starlo>: %s\n", o.to_string());

    l.merge(l2, &o, label::max, label::leq_starhi);
    printf("max<starhi>: %s\n", o.to_string());

    l.merge(l2, &o, label::min, label::leq_starhi);
    printf("min<starhi>: %s\n", o.to_string());

    try {
	int x = 5;
	throw x;
    } catch (int &x) {
	printf("caught a %d\n", x);
    } catch (...) {
	printf("caught something\n");
    }

    try {
	url u("foo://bar");
    } catch (basic_exception &e) {
	e.print_where();
    }

    return 0;
}
