#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "../include/net/rpmsg.h"

#define M3_CORE0 (0)

#define NUMLOOPS 1000

long diff(struct timespec start, struct timespec end)
{
    long    usecs;

    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    usecs = temp.tv_sec * 1000 + temp.tv_nsec / 1000;
    return usecs;
}

int main(void)
{
       int sock, err;
       struct sockaddr_rpmsg src_addr, dst_addr;
       socklen_t len;
       const char *msg = "Hello there!";
       char buf[512];
        struct timespec   start,end;
        long              elapsed=0,delta;
       int i;

       /* create an RPMSG socket */
       sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
       if (sock < 0) {
            printf("socket failed: %s (%d)\n", strerror(errno), errno);
            return -1;
       }

       /* connect to remote service */
       memset(&dst_addr, 0, sizeof(dst_addr));
       dst_addr.family = AF_RPMSG;
       dst_addr.vproc_id = M3_CORE0;
       dst_addr.addr = 51; // use 51 for ping_tasks;
       //dst_addr.addr = 61; // use 61 for messageQ transport;

       printf("Connecting to address 0x%x on processor %d\n",
                           dst_addr.addr, dst_addr.vproc_id);

       len = sizeof(struct sockaddr_rpmsg);
       err = connect(sock, (struct sockaddr *)&dst_addr, len);
       if (err < 0) {
            printf("connect failed: %s (%d)\n", strerror(errno), errno);
            return -1;
       }

       /* let's see what local address did we get */
       err = getsockname(sock, (struct sockaddr *)&src_addr, &len);
       if (err < 0) {
            printf("getpeername failed: %s (%d)\n", strerror(errno), errno);
            return -1;
       }

       printf("Our address: socket family: %d, proc id = %d, addr = %d\n",
                 src_addr.family, src_addr.vproc_id, src_addr.addr);

       printf("Sending \"%s\"\n", msg);

    for (i = 0; i < NUMLOOPS; i++) {
       //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
       clock_gettime(CLOCK_REALTIME, &start);

       err = send(sock, msg, strlen(msg) + 1, 0);
       if (err < 0) {
            printf("sendto failed: %s (%d)\n", strerror(errno), errno);
            return -1;
       }

       memset(&src_addr, 0, sizeof(src_addr));

       len = sizeof(src_addr);

       // printf("Awaiting a response...\n");
       err = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&src_addr, &len);
       if (err < 0) {
            printf("recvfrom failed: %s (%d)\n", strerror(errno), errno);
            return -1;
       }
       if (len != sizeof(src_addr)) {
            printf("recvfrom: got bad addr len (%d)\n", len);
            return -1;
       }

        //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
        clock_gettime(CLOCK_REALTIME, &end);
        delta = diff(start,end);
        elapsed += delta;

       /*
       printf ("Message time: %ld usecs\n", delta);
       printf("Received a msg from address 0x%x on processor %d\n",
                           src_addr.addr, src_addr.vproc_id);
       printf("Message content: \"%s\".\n", buf);
       */
    }
       printf ("Avg time: %ld usecs over %d iterations\n", elapsed / i, i);

       close(sock);

       return 0;
}
