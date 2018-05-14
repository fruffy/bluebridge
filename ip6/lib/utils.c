#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), malloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "utils.h"

/*
 * Gets the line from the command prompt
 * http://stackoverflow.com/questions/4023895/how-to-read-string-entered-by-user-in-c
 */
int getLine(char *prmpt, char *buff, size_t sz) {
    // Get line with buffer overrun protection.
    if (prmpt != NULL) {
        printf("%s", prmpt);
        fflush(stdout);
    }
    if (fgets(buff, sz, stdin) == NULL)
        return 0;

    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff) - 1] != '\n') {
        int extra = 0;
        int ch = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? 0 : 1;
    }

    // Otherwise remove newline and give string back to caller.
    buff[strlen(buff) - 1] = '\0';
    return 1;
}

/*
 * Gets random byte array with size num_bytes
 */
unsigned char *gen_rdm_bytestream(size_t num_bytes, int seed) {
    unsigned char *stream = (unsigned char *) malloc(num_bytes);
    size_t i;
    srand(seed);
    for (i = 0; i < num_bytes; i++) {
        stream[i] = rand();
    }

    return stream;
}

/*
 * Prints byte buffer until it hits terminating character
 */
int print_bytes(char * receiveBuffer) {
    int i = 0;

    while(receiveBuffer[i] != '\0') {
        printf("%02x", (unsigned char) receiveBuffer[i]);
        i++;
    }
    printf("\n");
    return i;
}

/*
 * Print reverse byte buffer including specified length
 */
int print_n_bytes(void *receiveBuffer, int num) {
    int i;

    // for (i = num-1; i>=0; i--) {
    //  printf("%02x", (unsigned char) receiveBuffer[i]);
    // }
    for (i =0; i<=num-1; i++) {
        printf("%02x", ((unsigned char *)receiveBuffer)[i]);
    }
    printf("\n");
    return i;
}

/*
 * Print reverse byte buffer including specified length
 */
int print_n_chars(void *receiveBuffer, int num) {
    int i;

    // for (i = num-1; i>=0; i--) {
    //  printf("%02x", (unsigned char) receiveBuffer[i]);
    // }
    for (i =0; i<=num-1; i++) {
        printf("%c", ((unsigned char *)receiveBuffer)[i]);
    }
    printf("\n");
    return i;
}

void print_ip_addr(struct in6_addr *ip_addr) {
        char s[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ip_addr, s, INET6_ADDRSTRLEN);
        printf("%s", s);
}