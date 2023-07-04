#include "xmodemserver.h"
#include "crc16.h"
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#define MAXBUFFER 1024
#ifndef PORT
#define PORT 56285
#endif

FILE *open_file_in_dir(char *filename, char *dirname)
{
    char buffer[MAXBUFFER];
    strncpy(buffer, "./", MAXBUFFER);
    strncat(buffer, dirname, MAXBUFFER - strlen(buffer) - 1);

    // create the directory dirname; fail silently if directory exists
    if (mkdir(buffer, 0700) == -1)
    {
        if (errno != EEXIST)
        {
            perror("mkdir");
            exit(1);
        }
    }
    strncat(buffer, "/", MAXBUFFER - strlen(buffer));
    strncat(buffer, filename, MAXBUFFER - strlen(buffer));

    return fopen(buffer, "wb");
}

int howmany = 1;
struct client *top = NULL;

struct client *addclient(int fd, struct in_addr addr)
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
    p->inbuf = 0;
    p->fp = NULL;
    p->current_block = 0;
    p->next = top;

    top = p;
    howmany++;
    return p;
}
void removeclient(int fd)
{
    struct client **p;
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    if (*p)
    {
        struct client *t = (*p)->next;
        fflush(stdout);
        fclose((*p)->fp);
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

int main(int argc, char *argv[])
{
    // set up listening socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }
    // check
    int yes = 1;
    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1)
    {
        perror("setsockopt");
    }

    struct sockaddr_in server;
    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    // bind
    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("bind");
        return 1;
    }
    // listen
    if (listen(sockfd, 100) == -1)
    {
        perror("listen");
        return 1;
    }

    socklen_t len = sizeof(server);

    struct client *client1;

    enum recstate state = initial;

    while (1)
    {
        char temp;
        int read_count;
        int read_counter = 0;

        int fd;

        // from the block
        unsigned char curr_block;
        unsigned char inverse_block;
        unsigned char high_byte;
        unsigned char low_byte;
        unsigned short e_message;

        if (client1)
        {

            if (state == initial)
            {
                fd_set fds;
                int maxfd = sockfd;
                FD_ZERO(&fds);
                FD_SET(sockfd, &fds);
                if (client1 != NULL)
                {
                    for (client1 = top; client1 != NULL; client1 = client1->next)
                    {
                        FD_SET(client1->fd, &fds);
                        if (client1->fd > maxfd)
                            maxfd = client1->fd;
                    }
                }

                if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0)
                {
                    perror("select");
                }
                else
                {
                    for (client1 = top; client1; client1 = client1->next)
                        if (FD_ISSET(client1->fd, &fds))
                            break;

                    if (FD_ISSET(sockfd, &fds))
                    {

                        if ((fd = accept(sockfd, (struct sockaddr *)&server, &len)) == -1)
                        {
                            perror("accept\n");
                            state = finished;
                        }
                        client1 = addclient(fd, server.sin_addr);

                        /*Chech if \r\n are the last 2 characters and that the character count
                        does not excceed 21. if it does then chnage state to finished*/
                        char filename[30];
                        char temp2;
                        int last_checker;

                        while ((read_count = read(client1->fd, &temp, 1)) > 0)
                        {
                            if (temp == '\r')
                            {

                                last_checker = read_counter;
                                temp2 = temp;
                            }
                            if (temp2 == '\r' && temp == '\n' && read_counter == last_checker + 1)
                            {
                                break;
                            }
                            filename[read_counter] = temp;
                            read_counter++;
                        }
                        if (read_count <= 0)
                        {

                            state = finished;
                        }
                        /*handle error for exceedingly long filename*/
                        else if (read_counter >= 21)
                        {
                            state = finished;
                        }
                        else
                        {
                            filename[read_counter - 1] = 0;
                            strcpy(client1->filename, filename);

                            client1->fp = open_file_in_dir(client1->filename, "filestore");

                            // writing the C character back to the client
                            char character = 'C';
                            if (write(fd, &character, 1) < 0)
                            {
                                perror("write");
                                state = finished;
                            }
                            else
                            {
                                state = pre_block;
                            }
                        }
                    }
                }
            }

            // should work no bugs :)
            if (state == pre_block)
            {

                /*If there is an EOT signal then we get to finish state
                otherwise, for SOH and STX we set the appropriate blocksize for
                the each signal respectively and set the state to get_block */
                while ((read_count = read(fd, &temp, 1)) > 0)
                {
                    if (temp == EOT)
                    {
                        char ack = ACK;
                        if (write(fd, &ack, 1) < 0)
                        {
                            perror("write");
                            state = finished;
                        }
                        state = finished;
                        break;
                    }

                    if (temp == SOH)
                    {
                        client1->blocksize = 132;
                        state = get_block;
                        break;
                    }
                    if (temp == STX)
                    {
                        client1->blocksize = 1028;
                        state = get_block;
                        break;
                    }
                }
                if (read_count <= 0)
                {
                    state = finished;
                }
            }
            if (state == get_block)
            {
                /*read all the data to the buffer and increment the inbuf
                When it is done*/
                while (client1->inbuf < client1->blocksize)
                {
                    (read_count = read(client1->fd, &temp, 1));
                    client1->buf[client1->inbuf] = temp;
                    client1->inbuf++;
                }
                if (read_count < 0)
                {
                    state = finished;
                }

                state = check_block;
            }

            if (state == check_block)
            {
                /*read all the desired data from the client, namely the curr_block number
                the inverse_block number the crc high_byte the crc_low_byte. Compare it with the results
                of the sever, if they match carry on and set the appropriate values*/
                curr_block = client1->buf[0];
                inverse_block = client1->buf[1];
                high_byte = client1->buf[client1->blocksize - 2];
                low_byte = client1->buf[client1->blocksize - 1];
                e_message = crc_message(XMODEM_KEY, (unsigned char *)(client1->buf + 2), client1->blocksize - 4);
                unsigned char low_byte2 = e_message;

                if ((255 - curr_block) != inverse_block)
                {
                    state = finished;
                }
                else if (curr_block == client1->current_block)
                {
                    char ack2 = ACK;
                    write(client1->fd, &ack2, 1);
                }
                else if (curr_block % 256 != (client1->current_block + 1) % 256)
                {
                    state = finished;
                }
                else if (low_byte2 != low_byte || e_message >> 8 != high_byte)
                {
                    char nak = NAK;
                    write(client1->fd, &nak, 1);
                }
                else
                {
                    int i = 2;
                    while (client1->buf[i] != 26 && i < client1->blocksize - 2)
                    {
                        if (write(fileno(client1->fp), client1->buf + i, 1) < 0)
                        {
                            state = finished;
                        }
                        i++;
                    }

                    client1->current_block = curr_block;

                    char ack2 = ACK;
                    write(fd, &ack2, 1);
                    state = pre_block;
                    client1->inbuf = 0;
                }
            }

            /*Clean up if the state is finished. Drop the client and set the state to initial*/
            if (state == finished)
            {

                close(client1->fd);

                if (client1->fp)
                {
                    removeclient(client1->fd);
                }
                state = initial;
            }
        }
    }
}

/*
  printf("%x  is the server high_byte e_message\n", e_message >> 8);
            printf("%x is the client high_byte\n", high_byte);
            printf("%x is  the client low_byte\n", low_byte);
            printf("%x this is the server low_byte\n", low_byte2);
*/