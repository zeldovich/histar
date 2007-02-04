#ifndef JOS_INC_ERROR_HH
#define JOS_INC_ERROR_HH

#include <exception>
#include <inc/backtracer.hh>

class basic_exception : public std::exception {
public:
    basic_exception() { get_backtrace(false); }
    basic_exception(const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));

    void set_msg(const char *msg);
    virtual const char *what() const throw () { return &msg_[0]; }
    void print_where() const;
    void force_backtrace() { get_backtrace(true); }

private:
    void get_backtrace(bool force);

    char msg_[256];
    backtracer *bt_;
};

class error : public basic_exception {
public:
    error(int r, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));

    int err() const { return err_; }

private:
    int err_;
};

#define error_check(expr)				\
    do {						\
	int64_t __r = (expr);				\
	if (__r < 0)					\
	    throw error(__r, "%s:%u: %s",		\
			__FILE__, __LINE__, #expr);	\
    } while (0)

#endif
