#ifndef PROTO_H
#define PROTO_H

#include <stddef.h>

/* Simple defines for supporting a plain protocol */

#define OPR_CONNECT    0
#define OPR_ECHO       1
#define OPR_CONC       2
#define OPR_NEG        3
#define OPR_ADD        4
#define OPR_SEND_IMAGE 5
#define OPR_BYE        6
#define OPR_OPTIMIZE   7

typedef struct msgHeader {
    int msgSize;    /* dimensiunea mesajului curent */
    int clientID;   /* id-ul clientului */
    int opID;       /* operatia ceruta */
} msgHeaderType;

/* mesaj simplu cu un int */
typedef struct intMsg {
    int msg;
} msgIntType;

/* mesaj cu doua int-uri */
typedef struct int2Msg {
    int msg1;
    int msg2;
} msg2IntType;

/* mesaj simplu cu un sir */
typedef struct stringMsg {
    char *msg;
} msgStringType;

/* mesaj cu doua siruri */
typedef struct string2Msg {
    msgStringType msg1;
    msgStringType msg2;
} msg2StringType;

/* header + un int */
typedef struct singleIntMsg {
    msgHeaderType header;
    msgIntType i;
} singleIntMsgType;

/* header + doua int-uri */
typedef struct multiIntMsg {
    msgHeaderType header;
    msg2IntType i;
} multiIntMsgType;

/* header + un sir */
typedef struct singleStringMsg {
    msgHeaderType header;
    msgStringType msg;
} singleStringType;

/* header + doua siruri */
typedef struct multiStringMsg {
    msgHeaderType header;
    msg2StringType s;
} multiStringType;

/* mesaj binar pentru imagini */
typedef struct binaryMsg {
    int size;
    unsigned char *data;
} msgBinaryType;

/* functii deja folosite in proiect */
msgHeaderType peekMsgHeader(int sock);
int readSingleInt(int sock, msgIntType *m);
int readMultiInt(int sock, msgIntType *m1, msgIntType *m2);
int readSingleString(int sock, msgStringType *m);
int writeSingleInt(int sock, msgHeaderType h, int i);
int writeMultiInt(int sock, msgHeaderType h, int i1, int i2);
int writeSingleString(int sock, msgHeaderType h, char *s);
int writeMultiString(int sock, msgHeaderType h, char *s1, char *s2);
int send_all(int sock, const void *buf, size_t len);
int recv_all(int sock, void *buf, size_t len);

int writeImageMessage(int sock, msgHeaderType h, const char *filename,
                      const unsigned char *data, int file_size);

int readImageMessage(int sock, char **filename,
                     unsigned char **data, int *file_size);

int readBinary(int sock, msgBinaryType *b);
int writeBinary(int sock, msgHeaderType h, unsigned char *data, int size);
int detect_image_format(unsigned char *data, int size);

#endif