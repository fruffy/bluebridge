#ifndef PROJECT_UTILS
#define PROJECT_UTILS
void *get_in_addr(struct sockaddr *sa);

int getLine(char *prmpt, char *buff, size_t sz);

unsigned char *gen_rdm_bytestream(size_t num_bytes);

void sigchld_handler(int s);

void sendMsg(int sockfd, char * sendBuffer, int msgBlockSize);

int receiveMsg(int sockfd, char * receiveBuffer, int msgBlockSize);

int printBytes(char * receiveBuffer);

#endif