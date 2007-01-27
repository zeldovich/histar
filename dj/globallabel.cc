extern "C" {
#include <inc/fs.h>
#include <netinet/in.h>
#include <stdio.h>    
#include <string.h>
#include <assert.h>
}

#include <dj/globallabel.hh>

#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>

static char
level_to_char(level_t lv)
{
    char lbuf[4];
    if (lv == LB_LEVEL_STAR)
        snprintf(&lbuf[0], sizeof(lbuf), "*");
    else
        snprintf(&lbuf[0], sizeof(lbuf), "%d", lv);
    return lbuf[0];
}

global_label::global_label(struct ulabel *ul, global_converter *get_global) 
    : serial_(0), string_(0), local_label_(0)
{
    // compact, ignore entries at def. level
    entries_ = 0;
    for (uint32_t i = 0 ; i < ul->ul_nent; i++)
	if (LB_LEVEL(ul->ul_ent[i]) != ul->ul_default)
	    entries_++;

    entry_ = new global_entry[entries_];
    default_ = ul->ul_default;
    memset(entry_, 0, sizeof(global_entry) * entries_);
    try {
        for (uint64_t i = 0; i < ul->ul_nent; i++) {
            uint64_t h = LB_HANDLE(ul->ul_ent[i]);
            level_t l = LB_LEVEL(ul->ul_ent[i]);
	    if (l == default_) {
		continue;
	    }
            (*get_global)(h, &entry_[i].global);
            entry_[i].level = l;
        }
    }
    catch (basic_exception e) {
        delete entry_;
        throw e;    
    }
}

global_label::global_label(const char *serial) 
    : serial_(0), string_(0), local_label_(0)
{
    copy_from(serial);
}

void
global_label::cleanup(void)
{
    if (entry_)
	delete entry_;
    if (serial_)
	delete serial_;    
    if (string_)
	delete string_;    
}

void
global_label::copy_from(const char *serial)
{
    entries_ = ntohl(((uint32_t *)serial)[0]);
    default_ = serial[4];
    
    entry_ = new global_entry[entries_];
    memset(entry_, 0, sizeof(global_entry) * entries_);
    
    uint32_t off = 5;
    memcpy(entry_, &serial[off], entries_ * sizeof(global_entry));
}

const label *
global_label::local_label(global_to_local *g2l, void *arg)
{
    if (!local_label_) {
	global_label *me = (global_label *)this;
        me->gen_local_label(g2l, arg);
    }
    return local_label_;
}

const char *
global_label::serial(void) const 
{
    if (!serial_) {
        global_label *me = (global_label *)this;
        me->gen_serial();
    }
    return serial_;
}

void
global_label::serial_is(const char *serial)
{
    cleanup();
    copy_from(serial);
    return;
}
    
uint32_t         
global_label::serial_len(void) const
{
    if (!serial_) {
        global_label *me = (global_label *)this;
        me->gen_serial();
    }
    return serial_len_;    
}

void 
global_label::gen_local_label(global_to_local *g2l, void *arg)
{
    local_label_ = new label(default_);
    try {
	for (uint32_t i = 0; i < entries_; i++) {
	    int64_t local;
	    error_check(local = g2l(&entry_[i].global, arg));
	    local_label_->set(local, entry_[i].level);
	}
    } catch (error &e) {
	delete local_label_;
	local_label_ = 0;
	throw e;
    }
}

void
global_label::gen_serial(void)
{
    static const uint32_t  bufsize = 256;
    char buf[bufsize];

    assert(bufsize >= sizeof(global_entry) * entries_ + 8);

    ((uint32_t*)buf)[0] = htonl(entries_);
    buf[4] = default_;

    uint32_t off = 5;
    memcpy(&buf[off], entry_, entries_ * sizeof(global_entry));
    off += entries_ * sizeof(global_entry);
    
    serial_ = new char[off];
    memcpy(serial_, buf, off);
    serial_len_ = off;
}

const char *
global_label::string_rep(void) const 
{
    if (string_)
        return string_;
    
    static const int bufsize = 256;
    char buf[bufsize];

    uint32_t off = 0;
    off += snprintf(&buf[off], bufsize - off, "{ ");
    for (uint32_t i = 0; i < entries_; i++) {
        level_t lv = entry_[i].level;
        if (lv == default_)
            continue;
        off += snprintf(&buf[off], bufsize - off, "(%ld, %ld):%c ",
                entry_[i].global.k, entry_[i].global.original, level_to_char(lv));
    }
    off += snprintf(&buf[off], bufsize - off, "%c }",
            level_to_char(default_));

    global_label *me = (global_label *)this;
    me->string_ = new char[strlen(buf) + 1];
    strcpy(string_, buf);
    return string_;
}
