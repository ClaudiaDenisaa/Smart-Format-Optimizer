/**
 * IR3 2026
 * Programul realizeaza functii ajutatoare pentru comunicarea prin socket
 * intre client si server.
 *
 * Sunt implementate functii pentru:
 * - citirea unui header de mesaj
 * - trimiterea si primirea valorilor int
 * - trimiterea si primirea stringurilor
 * - trimiterea si primirea datelor binare
 * - trimiterea si citirea unui mesaj complet pentru imagine
 * - detectarea formatului unei imagini primite
 *
 * Codul foloseste structurile definite in proto.h.
 */

#include <pthread.h>    /* utilizat pentru lucrul cu threaduri */
#include <stdio.h>      /* utilizat pentru: fprintf */
#include <ncurses.h>    /* utilizat pentru interfata text, daca este nevoie */
#include <stdlib.h>     /* utilizat pentru: malloc, free, EXIT_SUCCESS */
#include <string.h>     /* utilizat pentru: strlen */
#include <unistd.h>     /* utilizat pentru: write, close */
#include <sys/types.h>  /* tipuri de baza pentru socket */
#include <sys/socket.h> /* utilizat pentru: send, recv */
#include <sys/select.h> /* utilizat pentru select */
#include <netinet/in.h> /* structuri pentru adrese Internet */
#include <netdb.h>      /* utilizat pentru lucrul cu hosturi */
#include <arpa/inet.h>  /* utilizat pentru: htonl, ntohl */
#include "proto.h"      /* structurile si constantele protocolului */

#define DEBUG

msgHeaderType peekMsgHeader(int sock) {
	size_t nb;
	msgHeaderType h;

	/* incearca sa citeasca headerul fara sa il scoata din socket */
	h.msgSize = htonl(sizeof(h));
	nb = recv(sock, &h, sizeof(h), MSG_PEEK | MSG_WAITALL);

	/* converteste campurile din network byte order */
	h.msgSize = ntohl(h.msgSize);
	h.clientID = ntohl(h.clientID);
	h.opID = ntohl(h.opID);

	/* la eroare marcam valorile ca invalide */
	if (nb == (size_t)-1) {
		h.opID = h.clientID = -1;
	}

	/* daca recv intoarce 0, conexiunea a fost inchisa */
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

	/* citeste un mesaj care contine un singur int */
	nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
	if (nb <= 0) {
		m->msg = -1;
		return -1;
	}

	/* converteste valoarea la formatul local */
	m->msg = ntohl(s.i.msg);
	return (int)nb;
}

int writeSingleInt(int sock, msgHeaderType h, int i) {
	singleIntMsgType s;
	size_t nb;

	/* completeaza headerul mesajului */
	s.header.clientID = htonl(h.clientID);
	s.header.opID = htonl(h.opID);
	s.i.msg = htonl(i);
	s.header.msgSize = htonl(sizeof(s));

	/* trimite structura completa */
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

	/* citeste un mesaj cu doua valori int */
	nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
	if (nb <= 0) {
		m1->msg = -1;
		m2->msg = -1;
		return -1;
	}

	/* converteste cele doua valori */
	m1->msg = ntohl(s.i.msg1);
	m2->msg = ntohl(s.i.msg2);
	return (int)nb;
}

int writeMultiInt(int sock, msgHeaderType h, int i1, int i2) {
	multiIntMsgType s;
	size_t nb;

	/* construieste mesajul cu cele doua inturi */
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

	/* mai intai se citeste lungimea stringului */
	nb = readSingleInt(sock, &m);
	fprintf(stderr, "The string size was received: %d\n", m.msg);

	/* se aloca memorie pentru text + terminatorul de sir */
	str->msg = (char*)malloc((size_t)m.msg + 1);
	if (str->msg == NULL) {
		return -1;
	}

	/* se citeste continutul textului */
	nb = recv(sock, str->msg, (size_t)m.msg, MSG_WAITALL);
	fprintf(stderr, "\tReceived stream is {%ld}\n", nb);

	/* adaugam terminatorul de sir */
	str->msg[m.msg] = '\0';
	fprintf(stderr, "\tReceived message is {%s}\n", str->msg);

	return (int)nb;
}

int readBinary(int sock, msgBinaryType *b) {
    msgIntType m;

    /* citim mai intai dimensiunea datelor binare */
    if (readSingleInt(sock, &m) <= 0)
        return -1;

    b->size = m.msg;

    /* alocam memorie exact cat trebuie */
    b->data = malloc(b->size);
    if (!b->data)
        return -1;

    /* citim tot continutul binar */
    return recv(sock, b->data, b->size, MSG_WAITALL);
}

int writeSingleString(int sock, msgHeaderType h, char* str) {
	size_t nb;
	int strSize = (int)strlen(str);

	/* trimitem intai dimensiunea stringului */
	nb = writeSingleInt(sock, h, strSize);
	if (nb == (size_t)-1) {
		return -1;
	}
	if (nb == 0) {
		return -1;
	}

	fprintf(stderr, "\tSent size notification [%d]\n", strSize);

	/* afisare locala pentru debug */
	nb = write(2, str, (size_t)strSize);
	(void)nb;

	/* apoi trimitem continutul stringului */
	nb = send(sock, str, (size_t)strSize, 0);
	fprintf(stderr, "|\t[%ld/%ld//%ld]\n",
		nb, sizeof(singleStringType), sizeof(msgStringType));

	return (int)nb;
}

int writeBinary(int sock, msgHeaderType h, unsigned char *data, int size) {
    /* trimitem intai dimensiunea blocului binar */
    if (writeSingleInt(sock, h, size) <= 0)
        return -1;

    /* apoi trimitem efectiv datele */
    return send(sock, data, size, 0);
}

int detect_image_format(unsigned char *data, int size) {
    /* daca nu avem suficienti octeti, nu putem identifica formatul */
    if (size < 4)
        return 0;

    /* verificare semnatura JPEG */
    if (data[0] == 0xFF && data[1] == 0xD8)
        return 1;

    /* verificare semnatura PNG */
    if (data[0] == 0x89 && data[1] == 0x50)
        return 2;

    /* alt format sau necunoscut */
    return 0;
}

int send_all(int sock, const void* buf, size_t len) {
	size_t total_sent = 0;
	const char* ptr = (const char*)buf;

	/* trimite in bucla pana cand toate datele au fost expediate */
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

	/* citeste in bucla pana cand se primesc toate datele */
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

	/* verificam datele primite ca parametri */
	if (filename == NULL || data == NULL || file_size < 0) {
		return -1;
	}

	filename_len = (int)strlen(filename);
	net_filename_len = htonl(filename_len);
	net_file_size = htonl(file_size);

	/* calculam dimensiunea completa a mesajului */
	wire_h.msgSize = htonl((int)(sizeof(msgHeaderType) +
		sizeof(int) +
		filename_len +
		sizeof(int) +
		file_size));
	wire_h.clientID = htonl(h.clientID);
	wire_h.opID = htonl(h.opID);

	/* trimitem headerul */
	if (send_all(sock, &wire_h, sizeof(msgHeaderType)) < 0) {
		return -1;
	}

	/* trimitem lungimea numelui fisierului */
	if (send_all(sock, &net_filename_len, sizeof(int)) < 0) {
		return -1;
	}

	/* trimitem numele fisierului */
	if (send_all(sock, filename, (size_t)filename_len) < 0) {
		return -1;
	}

	/* trimitem dimensiunea imaginii */
	if (send_all(sock, &net_file_size, sizeof(int)) < 0) {
		return -1;
	}

	/* trimitem continutul imaginii */
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

	/* verificam daca pointerii de iesire sunt valizi */
	if (filename == NULL || data == NULL || file_size == NULL) {
		return -1;
	}

	*filename = NULL;
	*data = NULL;
	*file_size = 0;

	/* citim mai intai headerul mesajului */
	if (recv_all(sock, &h, sizeof(msgHeaderType)) < 0) {
		return -1;
	}

	/* convertim valorile la format local */
	h.msgSize = ntohl(h.msgSize);
	h.clientID = ntohl(h.clientID);
	h.opID = ntohl(h.opID);

	/* functia accepta doar mesaje de tip imagine */
	if (h.opID != OPR_SEND_IMAGE) {
		return -1;
	}

	/* citim lungimea numelui de fisier */
	if (recv_all(sock, &net_filename_len, sizeof(int)) < 0) {
		return -1;
	}

	filename_len = ntohl(net_filename_len);
	if (filename_len <= 0 || filename_len > 255) {
		return -1;
	}

	/* alocam memorie pentru numele fisierului */
	*filename = (char*)malloc((size_t)filename_len + 1);
	if (*filename == NULL) {
		return -1;
	}

	/* citim numele fisierului */
	if (recv_all(sock, *filename, (size_t)filename_len) < 0) {
		free(*filename);
		*filename = NULL;
		return -1;
	}
	(*filename)[filename_len] = '\0';

	/* citim dimensiunea imaginii */
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

	/* alocam memorie pentru continutul imaginii */
	*data = (unsigned char*)malloc((size_t)local_file_size);
	if (*data == NULL) {
		free(*filename);
		*filename = NULL;
		return -1;
	}

	/* citim tot fisierul in memorie */
	if (recv_all(sock, *data, (size_t)local_file_size) < 0) {
		free(*filename);
		free(*data);
		*filename = NULL;
		*data = NULL;
		return -1;
	}

	/* salvam dimensiunea finala */
	*file_size = local_file_size;
	return 0;
}


