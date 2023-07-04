/*
 * demonstration server, sockets and all that.
 * Does some i/o with each client, and copies other clients on the output.
 * Detects disconnections, uses select() properly, etc.
 *
 * Alan J Rosenthal, December 2000
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>

int port = 3098;

static int listenfd;

struct client
{
    int fd;
    struct in_addr ipaddr;
    struct client *next;
} *top = NULL;
int howmany = 1; /* server counts as one person who knows the muffin man */

static char greeting[] =
    "Do you know the muffin man,\r\n"
    "the muffin man, the muffin man,\r\n"
    "Do you know the muffin man\r\n"
    "who lives in Drury Lane?\r\n";
static char happyreply[] =
    "Now %d of us know the muffin man,\r\n"
    "the muffin man, the muffin man,\r\n"
    "Now %d of us know the muffin man\r\n"
    "who lives in Drury Lane!\r\n";

static void addclient(int fd, struct in_addr addr);
static void removeclient(int fd);
static void broadcast(char *s, int size);

int main(int argc, char **argv)
{
    int c;
    struct client *p;
    extern void bindandlisten(), newconnection(), activity(struct client * p);

    while ((c = getopt(argc, argv, "p:")) != EOF)
    {
        if (c == 'p')
        {
            if ((port = atoi(optarg)) == 0)
            {
                fprintf(stderr, "%s: port number must be a positive integer\n",
                        argv[0]);
                return (1);
            }
        }
        else
        {
            fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
            return (1);
        }
    }

    bindandlisten(); /* aborts on error */

    /* the only way the server exits is by being killed */
    while (1)
    {
        fd_set fds;
        int maxfd = listenfd;
        FD_ZERO(&fds);
        FD_SET(listenfd, &fds);
        for (p = top; p; p = p->next)
        {
            FD_SET(p->fd, &fds);
            if (p->fd > maxfd)
                maxfd = p->fd;
        }
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
        }
        else
        {
            for (p = top; p; p = p->next)
                if (FD_ISSET(p->fd, &fds))
                    break;
            /*
             * it isn't very likely that more than one client will drop at
             * once, so it's not a big loss that we process only one each
             * select(); we'll get it later...
             */
            if (p)
                activity(p); /* might remove p from list, so can't be in the loop */
            if (FD_ISSET(listenfd, &fds))
                newconnection();
        }
    }

    return (0);
}

void bindandlisten() /* bind and listen, abort on error */
{
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r))
    {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5))
    {
        perror("listen");
        exit(1);
    }
}

void newconnection() /* accept connection, sing to them, get response, update
                      * linked list */
{
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;
    int len, i, c;
    char buf[sizeof happyreply + 40];

    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0)
    {
        perror("accept");
    }
    else
    {
        printf("connection from %s\n", inet_ntoa(r.sin_addr));
        addclient(fd, r.sin_addr);

        /* sing */
        sprintf(buf, "Dear %s,\r\n", inet_ntoa(r.sin_addr));
        broadcast(buf, strlen(buf));
        broadcast(greeting, sizeof greeting - 1);

        /* wait for and get response -- look for first non-whitespace char */
        /* (buf is a good enough size for reads -- it probably will all fit
         * into one read() for any non-trivial size.) */
        c = -2; /* (neither a valid char nor the -1 error signal below) */
        while (c == -2)
        {
            fd_set fds;
            struct timeval tv;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            tv.tv_sec = 15;
            tv.tv_usec = 0;
            if (select(fd + 1, &fds, NULL, NULL, &tv) != 1)
            {
                c = -1;
                break;
            }
            if ((len = read(fd, buf, sizeof buf)) == 0)
            {
                c = -1;
            }
            else if (len < 0)
            {
                if (errno != EINTR)
                    c = -1;
            }
            else
            {
                for (i = 0; i < len; i++)
                {
                    if (isascii(buf[i]) && !isspace(buf[i]))
                    {
                        c = buf[i];
                        break;
                    }
                }
            }
        }

        /* react to response */
        if (c == 'y' || c == 'Y')
        {
            printf("They do know the muffin man!!\n");
            fflush(stdout);
            sprintf(buf, happyreply, howmany, howmany);
            broadcast(buf, strlen(buf));
        }
        else
        {
            if (c == -1)
            {
                printf("Communication error or timeout\n");
                fflush(stdout);
                removeclient(fd);
            }
            else
            {
                printf("Unfortunately, they don't know the muffin man.\n");
                fflush(stdout);
            }
            sprintf(buf, "Goodbye %s\r\n", inet_ntoa(r.sin_addr));
            broadcast(buf, strlen(buf));
            if (c != -1)
            { /* i.e. if didn't removeclient(fd) above */
                removeclient(fd);
                sleep(10);
            }
            close(fd);
        }
    }
}

void activity(struct client *p) /* select() said activity; check it out */
{
    char garbage[80];
    int len = read(p->fd, garbage, sizeof garbage);
    if (len > 0)
    {
        /* discard (probably more of the "yes!!!!" string) */
    }
    else if (len == 0)
    {
        char buf[80];
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        fflush(stdout);
        sprintf(buf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        removeclient(p->fd);
        broadcast(buf, strlen(buf));
    }
    else
    {
        /* shouldn't happen */
        perror("read");
    }
}

static void addclient(int fd, struct in_addr addr)
{
    struct client *p = malloc(sizeof(struct client));
    if (!p)
    {
        fprintf(stderr, "out of memory!\n"); /* highly unlikely to happen */
        exit(1);
    }
    printf("Adding client %s\n", inet_ntoa(addr));
    fflush(stdout);
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    top = p;
    howmany++;
}

static void removeclient(int fd)
{
    struct client **p;
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    if (*p)
    {
        struct client *t = (*p)->next;
        printf("Removing client %s\n", inet_ntoa((*p)->ipaddr));
        fflush(stdout);
        free(*p);
        *p = t;
        howmany--;
    }
    else
    {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
}

static void broadcast(char *s, int size)
{
    struct client *p;
    for (p = top; p; p = p->next)
        write(p->fd, s, size);
    /* should probably check write() return value and perhaps remove client */
}
