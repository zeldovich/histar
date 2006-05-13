#ifndef JOS_INC_FILESERVER_HH_
#define JOS_INC_FILESERVER_HH_

#include <inc/dis/segmessage.hh>
#include <inc/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>   

class segserver_req 
{
public:
    virtual void                  execute(void) = 0;
    const segclient_msg *response(void) const {;
        if (!executed_)
            return 0;    
        return &response_;    
    }
protected:
    segserver_req(segserver_hdr *header) : 
        executed_(0) { memcpy(&request_, header, sizeof(*header)); }
    
    segserver_hdr request_;    
    segclient_msg response_;
    bool           executed_;
};

class segserver_conn 
{
public:
    segserver_conn(int socket, sockaddr_in addr) : 
        socket_(socket), addr_(addr) {}
    segserver_conn(int socket) : socket_(socket) {}
    ~segserver_conn(void) { close(socket_); }
    
    segserver_req *next_request(void);
    void next_response_is(const segclient_msg *response);

private:    
    int socket_;
    sockaddr_in addr_;
};

#endif /*JOS_INC_FILESERVER_HH_*/
