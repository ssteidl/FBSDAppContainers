/*compile: c++ rctl_devd.cpp -o rctl_devd*/

/*Public domain*/

#include <stdbool.h>
#include <stdio.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <iostream>
#include <string>
int main(int argc, char** argv)
{
    struct sockaddr_un devd_addr;
    int s, error;

    /*Connect to devd's seq packet pipe*/
    memset(&devd_addr, 0, sizeof(devd_addr));
    devd_addr.sun_family = PF_LOCAL;
    std::string sockpath("/var/run/devd.seqpacket.pipe");
    strlcpy(devd_addr.sun_path, sockpath.c_str(), sizeof(devd_addr.sun_path));
    s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
    error = connect(s, (struct sockaddr*)&devd_addr, SUN_LEN(&devd_addr));

    if(error == -1)
    {
        perror("connect");
        exit(1);
    }

    char event[1024];
    for(;;)
    {
        memset(&event, 0, sizeof(event));
        /*SEQPACKET is connection oriented but maintains message
         * boundaries so only a single message will be received.
         */
        ssize_t len = recv(s, event, sizeof(event), 0);
        if(len == -1)
        {
            perror("recv");
            exit(1);
        }

        std::string event_msg(event, len);
        std::cerr << "Message of length: " << len << " received, msg: " << event_msg << std::endl;
    }

    exit(0);
}
