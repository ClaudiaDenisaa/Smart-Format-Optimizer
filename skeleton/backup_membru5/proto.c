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
#include "proto.h"

#define DEBUG

msgHeaderType peekMsgHeader(int sock) {
	size_t nb;
	msgHeaderType h;
	h.msgSize = htonl(sizeof(h));
	nb = recv(sock, &h, sizeof(h), MSG_PEEK | MSG_WAITALL);

	h.msgSize = ntohl(h.msgSize);
	h.clientID = ntohl(h.clientID);
	h.opID = ntohl(h.opID);

	if (nb == (size_t)-1) {
		h.opID = h.clientID = -1;
	}
	if (nb == 0) {
		h.opID = h.clientID = OPR_BYE;
	}
#ifdef DEBUG
	fprintf(stderr, "\tReceived msgHeader: %d %d, %d (%ld)\n",
		h.msgSize, h.clientID, h.opID, nb);
#endif
	return h;
}

int readSingleInt(int sock, msgIntType* m) {
	size_t nb;
	singleIntMsgType s;
	nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
	if (nb <= 0) {
		m->msg = -1;
		return -1;
	}
	m->msg = ntohl(s.i.msg);
	return (int)nb;
}

int writeSingleInt(int sock, msgHeaderType h, int i) {
	singleIntMsgType s;
	size_t nb;

	s.header.clientID = htonl(h.clientID);
	s.header.opID = htonl(h.opID);
	s.i.msg = htonl(i);
	s.header.msgSize = htonl(sizeof(s));

	nb = send(sock, &s, sizeof(s), 0);
	if (nb == (size_t)-1) {
		return -1;
	}
	if (nb == 0) {
		return -1;
	}
	return (int)nb;
}

int readMultiInt(int sock, msgIntType* m1, msgIntType* m2) {
	size_t nb;
	multiIntMsgType s;

	nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
	if (nb <= 0) {
		m1->msg = -1;
		m2->msg = -1;
		return -1;
	}
	m1->msg = ntohl(s.i.msg1);
	m2->msg = ntohl(s.i.msg2);
	return (int)nb;
}

int writeMultiInt(int sock, msgHeaderType h, int i1, int i2) {
	multiIntMsgType s;
	size_t nb;

	s.header.clientID = htonl(h.clientID);
	s.header.opID = htonl(h.opID);
	s.i.msg1 = htonl(i1);
	s.i.msg2 = htonl(i2);
	s.header.msgSize = htonl(sizeof(s));

	nb = send(sock, &s, sizeof(s), 0);
	if (nb == (size_t)-1) {
		return -1;
	}
	if (nb == 0) {
		return -1;
	}
	return (int)nb;
}

int readSingleString(int sock, msgStringType* str) {
	size_t nb;
	msgIntType m;

	nb = readSingleInt(sock, &m);
	fprintf(stderr, "The string size was received: %d\n", m.msg);

	str->msg = (char*)malloc((size_t)m.msg + 1);
	if (str->msg == NULL) {
		return -1;
	}

	nb = recv(sock, str->msg, (size_t)m.msg, MSG_WAITALL);
	fprintf(stderr, "\tReceived stream is {%ld}\n", nb);

	str->msg[m.msg] = '\0';
	fprintf(stderr, "\tReceived message is {%s}\n", str->msg);
	return (int)nb;
}

int writeSingleString(int sock, msgHeaderType h, char* str) {
	size_t nb;
	int strSize = (int)strlen(str);

	nb = writeSingleInt(sock, h, strSize);
	if (nb == (size_t)-1) {
		return -1;
	}
	if (nb == 0) {
		return -1;
	}

	fprintf(stderr, "\tSent size notification [%d]\n", strSize);
	nb = write(2, str, (size_t)strSize);
	(void)nb;

	nb = send(sock, str, (size_t)strSize, 0);
	fprintf(stderr, "|\t[%ld/%ld//%ld]\n",
		nb, sizeof(singleStringType), sizeof(msgStringType));
	return (int)nb;
}

int send_all(int sock, const void* buf, size_t len) {
	size_t total_sent = 0;
	const char* ptr = (const char*)buf;

	while (total_sent < len) {
		ssize_t sent = send(sock, ptr + total_sent, len - total_sent, 0);
		if (sent <= 0) {
			return -1;
		}
		total_sent += (size_t)sent;
	}

	return 0;
}

int recv_all(int sock, void* buf, size_t len) {
	size_t total_received = 0;
	char* ptr = (char*)buf;

	while (total_received < len) {
		ssize_t received = recv(sock, ptr + total_received, len - total_received, 0);
		if (received <= 0) {
			return -1;
		}
		total_received += (size_t)received;
	}

	return 0;
}

int writeImageMessage(int sock, msgHeaderType h, const char* filename,
	const unsigned char* data, int file_size) {
	msgHeaderType wire_h;
	int filename_len;
	int net_filename_len;
	int net_file_size;

	if (filename == NULL || data == NULL || file_size < 0) {
		return -1;
	}

	filename_len = (int)strlen(filename);
	net_filename_len = htonl(filename_len);
	net_file_size = htonl(file_size);

	wire_h.msgSize = htonl((int)(sizeof(msgHeaderType) +
		sizeof(int) +
		filename_len +
		sizeof(int) +
		file_size));
	wire_h.clientID = htonl(h.clientID);
	wire_h.opID = htonl(h.opID);

	if (send_all(sock, &wire_h, sizeof(msgHeaderType)) < 0) {
		return -1;
	}

	if (send_all(sock, &net_filename_len, sizeof(int)) < 0) {
		return -1;
	}

	if (send_all(sock, filename, (size_t)filename_len) < 0) {
		return -1;
	}

	if (send_all(sock, &net_file_size, sizeof(int)) < 0) {
		return -1;
	}

	if (send_all(sock, data, (size_t)file_size) < 0) {
		return -1;
	}

	return 0;
}

int readImageMessage(int sock, char** filename,
	unsigned char** data, int* file_size) {
	msgHeaderType h;
	int net_filename_len;
	int filename_len;
	int net_file_size;
	int local_file_size;

	if (filename == NULL || data == NULL || file_size == NULL) {
		return -1;
	}

	*filename = NULL;
	*data = NULL;
	*file_size = 0;

	if (recv_all(sock, &h, sizeof(msgHeaderType)) < 0) {
		return -1;
	}

	h.msgSize = ntohl(h.msgSize);
	h.clientID = ntohl(h.clientID);
	h.opID = ntohl(h.opID);

	if (h.opID != OPR_SEND_IMAGE) {
		return -1;
	}

	if (recv_all(sock, &net_filename_len, sizeof(int)) < 0) {
		return -1;
	}

	filename_len = ntohl(net_filename_len);
	if (filename_len <= 0 || filename_len > 255) {
		return -1;
	}

	*filename = (char*)malloc((size_t)filename_len + 1);
	if (*filename == NULL) {
		return -1;
	}

	if (recv_all(sock, *filename, (size_t)filename_len) < 0) {
		free(*filename);
		*filename = NULL;
		return -1;
	}
	(*filename)[filename_len] = '\0';

	if (recv_all(sock, &net_file_size, sizeof(int)) < 0) {
		free(*filename);
		*filename = NULL;
		return -1;
	}

	local_file_size = ntohl(net_file_size);
	if (local_file_size <= 0) {
		free(*filename);
		*filename = NULL;
		return -1;
	}

	*data = (unsigned char*)malloc((size_t)local_file_size);
	if (*data == NULL) {
		free(*filename);
		*filename = NULL;
		return -1;
	}

	if (recv_all(sock, *data, (size_t)local_file_size) < 0) {
		free(*filename);
		free(*data);
		*filename = NULL;
		*data = NULL;
		return -1;
	}

	*file_size = local_file_size;
	return 0;
}