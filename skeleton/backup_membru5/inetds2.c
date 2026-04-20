#include <pthread.h>
#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include "proto.h"

int inet_socket(uint16_t port, short reuse) {
    int sock;
    struct sockaddr_in name;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        pthread_exit(NULL);
    }

    if (reuse) {
        int reuseAddrON = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseAddrON, sizeof(reuseAddrON)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            pthread_exit(NULL);
        }
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0) {
        perror("bind");
        pthread_exit(NULL);
    }

    return sock;
}

int get_client_id(int sock) {
    int size;
    long clientid;

    size = recv(sock, &clientid, sizeof(clientid), 0);
    fprintf(stderr, "gcid:\tReceived %ld\n", clientid);

    if (size == -1) {
        return -1;
    }

    return (int)clientid;
}

int write_client_int(int sock, long id) {
    int size;
    fprintf(stderr, "wcl:\tWriting %ld\n", id);
    if ((size = send(sock, &id, sizeof(id), 0)) < 0) {
        return -1;
    }
    return EXIT_SUCCESS;
}

int write_client_id(int sock, long id) {
    return write_client_int(sock, id);
}

int write_client_concat(int sock, char* o1, char* o2) {
    int size, bsize;
    char* b;

    bsize = (int)strlen(o1) + (int)strlen(o2) + 2;
    b = malloc(256);
    if (b == NULL) {
        return -1;
    }

    sprintf(b, "%s %s", o2, o1);
    b[bsize] = 0;

    if ((size = send(sock, b, (size_t)bsize, 0)) < 0) {
        free(b);
        return -1;
    }

    free(b);
    return EXIT_SUCCESS;
}

char* get_client_str(int sock, int* dsize) {
    char buffer[256];
    char* result = NULL;
    int isize = 0;
    int size;

    while ((size = recv(sock, &buffer, 256, 0)) >= 0) {
        result = realloc(result, (size_t)isize + (size_t)size + 1);
        if (result == NULL) {
            return NULL;
        }
        memcpy(result + isize, buffer, (size_t)size);
        isize += size;
        result[isize] = 0;
        if (size != 256) {
            break;
        }
    }

    if (isize < 0) {
        return NULL;
    }

    fprintf(stderr, "gcs:\tGot %s\n", result);
    *dsize = isize;
    return result;
}

int create_client_id(void) {
    char ctsmp[12];
    time_t rawtime;
    struct tm* timeinfo;
    int uuid;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(ctsmp, 12, "%s", timeinfo);

    uuid = atoi(ctsmp);
    return uuid;
}

int extract_client_operation(char* data) {
    (void)data;
    return -1;
}

static int read_images_processed_count(void) {
    FILE* f;
    char line[256];
    int count = 0;

    f = fopen("stats.txt", "r");
    if (f == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "images_processed=", 17) == 0) {
            count = atoi(line + 17);
            break;
        }
    }

    fclose(f);
    return count;
}

static void write_stats_file(int client_id, const char* filename, int file_size, const char* status) {
    FILE* f;
    int current_count = read_images_processed_count();
    int new_count = current_count + 1;

    f = fopen("stats.txt", "w");
    if (f == NULL) {
        perror("fopen stats.txt");
        return;
    }

    fprintf(f, "images_processed=%d\n", new_count);
    fprintf(f, "last_filename=%s\n", filename);
    fprintf(f, "last_filesize=%d\n", file_size);
    fprintf(f, "last_client_id=%d\n", client_id);
    fprintf(f, "last_status=%s\n", status);

    fclose(f);
}

void* inet_main(void* args) {
    int port = *((int*)args);
    int sock;
    size_t size;
    fd_set active_fd_set, read_fd_set;
    struct sockaddr_in clientname;

    if ((sock = inet_socket(port, 1)) < 0) {
        pthread_exit(NULL);
    }

    if (listen(sock, 1) < 0) {
        pthread_exit(NULL);
    }

    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    while (1) {
        int i;

        read_fd_set = active_fd_set;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            pthread_exit(NULL);
        }

        for (i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET(i, &read_fd_set)) {
                if (i == sock) {
                    int newfd;
                    size = sizeof(clientname);
                    newfd = accept(sock, (struct sockaddr*)&clientname, (socklen_t*)&size);
                    if (newfd < 0) {
                        pthread_exit(NULL);
                    }

                    FD_SET(newfd, &active_fd_set);
                }
                else {
                    int clientID;
                    msgHeaderType h = peekMsgHeader(i);

                    if ((clientID = h.clientID) < 0) {
                        fprintf(stderr, "Negative ClientID. Closing connection.\n");
                        close(i);
                        FD_CLR(i, &active_fd_set);
                    }
                    else {
                        if (clientID == 0) {
                            int newID;
                            msgIntType m;

                            newID = create_client_id();
                            fprintf(stderr, "\tDetected new client! New clientID: %d\n", newID);

                            if (readSingleInt(i, &m) < 0) {
                                close(i);
                                FD_CLR(i, &active_fd_set);
                            }

                            if (writeSingleInt(i, h, newID) < 0) {
                                close(i);
                                FD_CLR(i, &active_fd_set);
                            }
                        }
                        else {
                            int operation;

                            operation = h.opID;
                            if (operation == -1) {
                                close(i);
                                FD_CLR(i, &active_fd_set);
                                continue;
                            }

                            switch (operation) {
                            case OPR_ECHO:
                            {
                                msgStringType str;
                                if (readSingleString(i, &str) < 0) {
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                fprintf(stderr, "An echo value was received: %s\n", str.msg);

                                if (writeSingleString(i, h, str.msg) < 0) {
                                    free(str.msg);
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                fprintf(stderr, "An echo value was sent back: %s\n", str.msg);
                                free(str.msg);
                            }
                            break;

                            case OPR_CONC:
                                break;

                            case OPR_ADD:
                            {
                                msgIntType m1, m2, m;
                                if (readMultiInt(i, &m1, &m2) < 0) {
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                m.msg = m1.msg + m2.msg;

                                if (writeSingleInt(i, h, m.msg) < 0) {
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                fprintf(stderr, "An adder value was sent back: %d\n", m.msg);
                            }
                            break;

                            case OPR_NEG:
                            {
                                msgIntType m;
                                if (readSingleInt(i, &m) < 0) {
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                m.msg = -m.msg;

                                if (writeSingleInt(i, h, m.msg) < 0) {
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                fprintf(stderr, "A negative value was sent back: %d\n", m.msg);
                            }
                            break;

                            case OPR_SEND_IMAGE:
                            {
                                char* filename = NULL;
                                unsigned char* file_data = NULL;
                                int file_size = 0;
                                char response[] = "IMAGE_RECEIVED";

                                if (readImageMessage(i, &filename, &file_data, &file_size) < 0) {
                                    fprintf(stderr, "Could not read image message from client %d\n", clientID);
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }
                                
                                fprintf(stderr, "Image received from client %d\n", clientID);
                                fprintf(stderr, "Filename: %s\n", filename);
                                fprintf(stderr, "File size: %d bytes\n", file_size);

                                write_stats_file(clientID, filename, file_size, "IMAGE_RECEIVED");

                                h.opID = OPR_ECHO;
                                if (writeSingleString(i, h, response) < 0) {
                                    fprintf(stderr, "Could not send image confirmation to client %d\n", clientID);
                                    free(filename);
                                    free(file_data);
                                    close(i);
                                    FD_CLR(i, &active_fd_set);
                                    break;
                                }

                                free(filename);
                                free(file_data);
                            }
                            break;

                            case OPR_BYE:
                            default:
                                close(i);
                                FD_CLR(i, &active_fd_set);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
}