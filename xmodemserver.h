#include <stdio.h>
/* This header file should be included in your xmodemserver.c file.
 * You are welcome to add to it.
 */

/* Enumeration constants are similar to constants created with #define
 * It is a good way of being able to give a name to a state in this case.
 * For example, you can use the enum below as follows:
 *     //first declare a variable:
 *     enum recstate state;
 *     //then give it a value (note: the value is not a string)
 *     state = check_block;
 */
enum recstate
{
    initial,
    pre_block,
    get_block,
    check_block,
    finished
};

/* The client struct contains the state that the server needs for each client.
 */
struct client
{
    int fd;              // socket file descriptor for this client
    char buf[2048];      // buffer to hold data being read from client
    int inbuf;           // index into buf
    char filename[20];   // name of the file being transferred
    FILE *fp;            // file pointer for where the file is written to
    enum recstate state; // current state of data transfer for this client
    int blocksize;       // the size of the current block
    int current_block;   // the block number of the current block
    struct client *next; // a pointer to the next client in the list
};

#define XMODEM_KEY 0x1021

// ASCII characters
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 21
