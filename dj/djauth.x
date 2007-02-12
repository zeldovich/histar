/*
 * A remote authentication protocol
 */

struct djauth_request {
    string username<16>;
    string password<16>;
};

struct djauth_reply {
    bool ok;
};
