#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define SIZE 4096
#define ERROR 0
#define SUCCESS 1

unsigned short checksum(unsigned short *addr, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <hostname>" << std::endl;
        return 1;
    }
    const char *cPing = argv[1];
    struct hostent *host = gethostbyname(cPing);
    if (host == NULL) {
        std::cout << ("Failed to resolve host: %s", cPing) << std::endl;
        return ERROR;
    }
    int seq = 1;
    int n;
    while (1) {
        struct timeval *tval;
        int maxfds = 0;
        fd_set readfds;

        struct sockaddr_in addr;
        struct sockaddr_in from;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = *(in_addr_t *)host->h_addr;

        int sockfd;
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sockfd < 0) {
            std::cout << ("ip:%s, socket error", cPing) << std::endl;
            return ERROR;
        }
        struct timeval timeo;
        timeo.tv_sec = 10000 / 1000;
        timeo.tv_usec = 10000 % 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1) {
            std::cout << ("ip:%s, setsockopt error", cPing) << std::endl;
            return ERROR;
        }
        char sendpacket[SIZE];
        char recvpacket[SIZE];
        memset(sendpacket, 0, sizeof(sendpacket));

        pid_t pid;
        pid = getpid();

        struct ip *iph;
        struct icmp *icmp;
        icmp = (struct icmp *)sendpacket;
        icmp->icmp_type = ICMP_ECHO;
        icmp->icmp_code = 0;
        icmp->icmp_cksum = 0;
        icmp->icmp_seq = 0;
        icmp->icmp_id = pid;
        tval = (struct timeval *)icmp->icmp_data;
        gettimeofday(tval, NULL);
        icmp->icmp_cksum = checksum((unsigned short *)icmp, sizeof(struct icmp));
        n = sendto(sockfd, (char *)&sendpacket, sizeof(struct icmp), 0, (struct sockaddr *)&addr, sizeof(addr));
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        maxfds = sockfd + 1;
        n = select(maxfds, &readfds, NULL, NULL, &timeo);
        if (n <= 0) {
            std::cout << ("ip:%s, Time out error", cPing) << std::endl;
            close(sockfd);
            return ERROR;
        }
        memset(recvpacket, 0, sizeof(recvpacket));
        int fromlen = sizeof(from);
        n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
        std::cout << "64 bytes from " << cPing << ": icmp_seq=" << seq << " time=1 ms" << std::endl;

        usleep(1000000);

        if (n < 1) {
            return ERROR;
        }
        seq += 1;
        close(sockfd);
    }

    return SUCCESS;
}
