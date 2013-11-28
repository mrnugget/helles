#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

#include "ipc.h"

int send_fd(int socket, int *fd)
{
    // See: http://keithp.com/blogs/fd-passing/
    // See: libancillary
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    struct iovec nothing_ptr;

    struct {
        struct cmsghdr h;
        int fd[1];
    } buffer;

    char nothing = '!';

    nothing_ptr.iov_base = &nothing;
    nothing_ptr.iov_len = 1;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &nothing_ptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = &buffer;
    msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);

    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(cmsg), fd, sizeof(int));

    return(sendmsg(socket, &msghdr, 0) >= 0 ? 0 : -1);
}

int recv_fd(int socket, int *fd)
{
    // See: http://keithp.com/blogs/fd-passing/
    // See: libancillary
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    struct iovec nothing_ptr;

    struct {
        struct cmsghdr h;
        int fd[1];
    } buffer;

    char nothing;
    int data = -1;

    nothing_ptr.iov_base = &nothing;
    nothing_ptr.iov_len = 1;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &nothing_ptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = &buffer;
    msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);

    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    memcpy(CMSG_DATA(cmsg), &data, sizeof(int));

    if (recvmsg(socket, &msghdr, 0) < 0) {
        return -1;
    } else {
        memcpy(fd, CMSG_DATA(cmsg), sizeof(int));
        return 0;
    }
}
