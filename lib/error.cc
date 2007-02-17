#include <inc/error.hh>

extern "C" {
#include <inc/stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
}

static int exception_enable_backtrace = 0;

basic_exception::basic_exception(const char *fmt, ...)
    : bt_(0)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(&msg_[0], sizeof(msg_), fmt, ap);
    va_end(ap);

    get_backtrace(false);
}

basic_exception::basic_exception(const basic_exception &o)
    : std::exception(o), bt_(0)
{
    memcpy(&msg_[0], &o.msg_[0], sizeof(msg_));
    if (o.bt_)
	bt_ = new backtracer(*o.bt_);
}

basic_exception::~basic_exception() throw ()
{
    if (bt_)
	delete bt_;
}

void
basic_exception::set_msg(const char *msg)
{
    snprintf(&msg_[0], sizeof(msg_), "%s", msg);
}

void
basic_exception::print_where() const
{
    if (bt_) {
	int depth = bt_->backtracer_depth();
	fprintf(stderr, "Backtrace for error %s:\n", what());
	for (int i = 0; i < depth; i++) {
	    void *addr = bt_->backtracer_addr(i);
	    fprintf(stderr, "  %p\n", addr);
	}
	fprintf(stderr, "End of backtrace\n");
    } else {
	fprintf(stderr, "basic_exception::print_where(): backtraces disabled\n");
    }
}

void
basic_exception::get_backtrace(bool force)
{
    if (exception_enable_backtrace || force) {
	if (!bt_)
	    bt_ = new backtracer();
    }
}

error::error(int r, const char *fmt, ...) : err_(r)
{
    char buf[256];
    int used = 0;
    va_list ap;

    va_start(ap, fmt);
    used += vsnprintf(&buf[0], sizeof(buf), fmt, ap);
    va_end(ap);

    used += snprintf(&buf[used], sizeof(buf) - used, ": %s", e2s(r));

    buf[sizeof(buf) - 1] = '\0';
    set_msg(&buf[0]);
}
