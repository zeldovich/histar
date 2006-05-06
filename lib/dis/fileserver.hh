#ifndef JOS_INC_FILESERVER_HH_
#define JOS_INC_FILESERVER_HH_

#ifdef JOS_USER
#include <inc/types.h>
#else
#include <sys/types.h>
#endif // JOS_USER

#include <netinet/in.h>
#include <lib/dis/filemessage.h>
#include <string.h>
#include <unistd.h>   

void fileserver_start(int port);

class fileserver_req 
{
public:
    virtual void                  execute(void) = 0;
    const fileclient_msg *response(void) const {;
        if (!executed_)
            return 0;    
        return &response_;    
    }
protected:
    fileserver_req(fileserver_hdr *header) : 
        executed_(0) { memcpy(&request_, header, sizeof(*header)); }
    
    fileserver_hdr request_;    
    fileclient_msg response_;
    bool           executed_;
};

class fileserver_conn 
{
public:
    fileserver_conn(int socket, sockaddr_in addr) : 
        socket_(socket), addr_(addr) {}
    ~fileserver_conn(void) { close(socket_); }
    
    fileserver_req *next_request(void);
    void next_response_is(const fileclient_msg *response);

private:    
    int socket_;
    sockaddr_in addr_;
};

#endif /*JOS_INC_FILESERVER_HH_*/
