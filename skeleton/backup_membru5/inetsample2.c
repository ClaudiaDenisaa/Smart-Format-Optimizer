#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "proto.h"

#define PORT 18081
#define SERVERHOST "127.0.0.1"

void init_sockaddr(struct sockaddr_in* name, const char* hostname, uint16_t port) {
    struct hostent* hostinfo;

    name->sin_family = AF_INET;
    name->sin_port = htons(port);
    hostinfo = gethostbyname(hostname);
    if (hostinfo == NULL) {
        fprintf(stderr, "Unknown host %s.\n", hostname);
        exit(EXIT_FAILURE);
    }
    name->sin_addr = *(struct in_addr*)hostinfo->h_addr;
}

static const char* extract_filename(const char* path) {
    const char* slash1 = strrchr(path, '/');
    const char* slash2 = strrchr(path, '\\');
    const char* base = path;

    if (slash1 != NULL && slash2 != NULL) {
        base = (slash1 > slash2) ? slash1 + 1 : slash2 + 1;
    }
    else if (slash1 != NULL) {
        base = slash1 + 1;
    }
    else if (slash2 != NULL) {
        base = slash2 + 1;
    }

    return base;
}

static int read_file(const char* path, unsigned char** buffer, int* size) {
    FILE* f;
    long file_size;
    unsigned char* data;

    if (path == NULL || buffer == NULL || size == NULL) {
        return -1;
    }

    *buffer = NULL;
    *size = 0;

    f = fopen(path, "rb");
    if (f == NULL) {
        perror("fopen");
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    file_size = ftell(f);
    if (file_size < 0) {
        fclose(f);
        return -1;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    data = (unsigned char*)malloc((size_t)file_size);
    if (data == NULL) {
        fclose(f);
        return -1;
    }

    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return -1;
    }

    fclose(f);
    *buffer = data;
    *size = (int)file_size;
    return 0;
}

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in servername;
    msgHeaderType h;
    msgIntType m;
    int clientID = 0;

    unsigned char* file_data = NULL;
    int file_size = 0;
    const char* image_path;
    const char* filename_only;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    image_path = argv[1];
    filename_only = extract_filename(image_path);

    if (read_file(image_path, &file_data, &file_size) != 0) {
        fprintf(stderr, "Could not read image file: %s\n", image_path);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Loaded image: %s (%d bytes)\n", filename_only, file_size);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket (client)");
        free(file_data);
        exit(EXIT_FAILURE);
    }

    init_sockaddr(&servername, SERVERHOST, PORT);
    if (0 > connect(sock, (struct sockaddr*)&servername, sizeof(servername))) {
        perror("connect (client)");
        free(file_data);
        close(sock);
        exit(EXIT_FAILURE);
    }

    h.clientID = 0;
    h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &m);
    clientID = m.msg;
    fprintf(stderr, "Got a clientID: %d\n", clientID);

    h.clientID = clientID;
    h.opID = OPR_SEND_IMAGE;

    fprintf(stderr, "Sending image to server...\n");
    if (writeImageMessage(sock, h, filename_only, file_data, file_size) != 0) {
        fprintf(stderr, "Failed to send image.\n");
        free(file_data);
        close(sock);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Image sent successfully.\n");

    h.clientID = clientID;
    h.opID = OPR_ECHO;

    {
        msgStringType reply;
        if (readSingleString(sock, &reply) < 0) {
            fprintf(stderr, "Failed to receive server confirmation.\n");
            free(file_data);
            close(sock);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Server reply: %s\n", reply.msg);
        free(reply.msg);
    }

    h.clientID = clientID;
    h.opID = OPR_BYE;
    fprintf(stderr, "Sending BYE...\n");
    writeSingleInt(sock, h, 0);

    free(file_data);
    close(sock);
    return EXIT_SUCCESS;
}