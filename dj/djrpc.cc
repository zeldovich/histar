#include <dj/dis.hh>
#include <dj/djcall.h>

void
dj_call_sink(message_sender *s, dj_call_service srv,
	     const dj_message_args &a, uint64_t selftoken)
{
    dj_call_msg cm;
    if (!str2xdr(cm, a.msg)) {
	warn << "cannot unmarshal dj_call_msg\n";
	return;
    }

    dj_message_args reply;
    reply.msg_ct = cm.return_ct;
    reply.token = selftoken;

    srv(a, str(cm.buf.base(), cm.buf.size()), &reply);

    s->send(a.sender, cm.return_ep, reply, 0);
}
