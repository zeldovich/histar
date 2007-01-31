#include <dj/dis.hh>

class dummyexec : public djcallexec {
 public:
    dummyexec(djprot::call_reply_cb cb) : cb_(cb) {}

    virtual void start(const dj_gatename &gate, const djcall_args &args) {
	warn << "dummyexec: data " << args.data << "\n";
	cb_(REPLY_DONE, args);
    }

    virtual void abort() {}

 private:
    djprot::call_reply_cb cb_;
};

ptr<djcallexec>
dj_dummy_exec(djprot::call_reply_cb cb)
{
    return New refcounted<dummyexec>(cb);
}
