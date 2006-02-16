extern "C" {
#include <inc/lib.h>
};

#include <inc/cpplabel.hh>
#include <new>

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
    delete l2;

    std::set_terminate (__gnu_cxx::__verbose_terminate_handler);

    try {
	int x = 5;
	throw x;
    } catch (int &x) {
	printf("caught a %d\n", x);
    } catch (...) {
	printf("caught something\n");
    }

    return 0;
}
