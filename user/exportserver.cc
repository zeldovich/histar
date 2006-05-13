extern "C" {
#include <inc/types.h>    
#include <inc/debug.h>
#include <inc/fs.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/scopeguard.hh>

static const char conn_debug = 1;

class export_server
{
public:
    export_server(int port) : port_(port), running_(false) {}

    bool running(void) const { return running_;}
    void running_is(bool running);

private:
    void connection_loop(void);
    void spawn_proxy(int socket);

    uint16_t port_;
    bool     running_;    
};

void 
export_server::running_is(bool running)
{
    if (running == running_)
        return;
    running_ = running;
    if (running_)
        connection_loop();    
}

void
export_server::connection_loop(void)
{
    int s;
    error_check(s = socket(AF_INET, SOCK_STREAM, 0));
    scope_guard<int, int> close_sock(close, s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port_);
    error_check(bind(s, (struct sockaddr *)&sin, sizeof(sin)));
    error_check(listen(s, 5));
    
    debug_print(conn_debug, "waiting for connections");
    while (running_) {
        socklen_t socklen = sizeof(sin);
        int ss;
        error_check(ss = accept(s, (struct sockaddr *)&sin, &socklen));

        char *ip = (char *)&sin.sin_addr.s_addr;
        debug_print(conn_debug, "connection from %d.%d.%d.%d", 
               ip[0], ip[1], ip[2], ip[3]);

        try {
            spawn_proxy(ss);
            close(ss);
        }
        catch (basic_exception e) {
            printf("export_server::connection_loop: %s\n", e.what());    
        }
    }
}

void
export_server::spawn_proxy(int s)
{
    if (fork())
        return;
        
    try {
        char pn[] = "/bin/exportproxy";
        char socket[32];
        sprintf(socket, "%d", s);
        char *const av[] = { pn, socket, 0 };
        char *const ep[] = { 0 };
        error_check(execve("/bin/exportproxy", av, ep));    
    } catch (basic_exception e) {
        printf("export_server::spawn_proxy: %s\n", e.what());    
    }
    exit(0);
}

int
main(int ac, char **av)
{
    export_server server(8888);
    try {
        // make a test file
        fs_inode tmp, test_file;
        error_check(fs_namei("/tmp", &tmp));
        error_check(fs_create(tmp, "test_file", &test_file, 0));
        const char *test_data = "some test data";
        error_check(fs_pwrite(test_file, test_data, strlen(test_data), 0));
        
        server.running_is(true);
    } catch (basic_exception e) {
        printf("export server error: %s\n", e.what());    
    }
    
    printf("export server done!\n");    
    return 0;    
}
