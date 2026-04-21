# detectam sistemul de operare curent
OS := $(shell uname -s)

# variabile pentru directoare suplimentare de include si librarii
EXTRA_INCLUDES =
EXTRA_LIBS =

# libraria pentru thread-uri
THREAD_LIB =

# utilitarul gSOAP folosit pentru generarea fisierelor
SOAPCPP2 = soapcpp2

# prefixul unde este instalat gSOAP
GSOAP_PREFIX =

# setari speciale pentru macOS
ifeq ($(OS), Darwin)
    # luam automat calea unde este instalat gsoap prin brew
    GSOAP_PREFIX = $(shell brew --prefix gsoap 2>/dev/null)

    # directoare suplimentare pentru headere
    EXTRA_INCLUDES = -I/opt/homebrew/include -I$(GSOAP_PREFIX)/include -I$(GSOAP_PREFIX)/share/gsoap/import

    # directoare suplimentare pentru librarii
    EXTRA_LIBS = -L/opt/homebrew/lib -L$(GSOAP_PREFIX)/lib

    # pe mac folosim pthread
    THREAD_LIB = -lpthread

    # calea completa spre soapcpp2
    SOAPCPP2 = $(GSOAP_PREFIX)/bin/soapcpp2
endif

# setari pentru Linux
ifeq ($(OS), Linux)
    THREAD_LIB = -lpthread
endif

# lista fisierelor obiect necesare pentru server
OBJECTS = unixds.o inetds.o soapds.o proto.o queue.o image_processor.o

# afiseaza ajutor pentru comenzile disponibile
help:
	@echo "Use one of these"
	@echo "	make serverds -- to build the server"
	@echo "	make inetclient -- to build the inet test client"
	@echo "	make clean -- to remove unwanted files"

# construieste serverul principal
serverds: threeds.c $(OBJECTS) SampleServices.nsmap
	gcc -g threeds.c soapC.c soapServer.c $(OBJECTS) $(EXTRA_INCLUDES) $(EXTRA_LIBS) $(THREAD_LIB) -lgsoap -Wall -o serverds -lconfig -lturbojpeg -lpng -lz

# compileaza modulul pentru socketul UNIX
unixds.o: unixds.c
	gcc -g unixds.c -c -o unixds.o -Wall $(EXTRA_INCLUDES)

# compileaza modulul de retea TCP/INET
inetds.o: inetds2.c proto.o queue.o
	gcc -g inetds2.c -c -o inetds.o -Wall $(EXTRA_INCLUDES)

# compileaza partea de protocol
proto.o: proto.c proto.h
	gcc -g proto.c -c -o proto.o -Wall $(EXTRA_INCLUDES)

# compileaza implementarea cozii
queue.o: queue.c queue.h
	gcc -g queue.c -c -o queue.o -Wall $(EXTRA_INCLUDES)

# compileaza modulul de procesare a imaginilor
image_processor.o: image_processor.c image_processor.h queue.h proto.h
	gcc -g image_processor.c -c -o image_processor.o -Wall $(EXTRA_INCLUDES)

# compileaza modulul soap
soapds.o: soapds.c SampleServices.nsmap
	gcc -g soapds.c -c -o soapds.o -Wall $(EXTRA_INCLUDES)

# genereaza fisierele necesare pentru gSOAP
SampleServices.nsmap: sclient.h
	$(SOAPCPP2) -S -c -x sclient.h

# construieste clientul de test pentru inet
inetclient: inetsample2.c proto.o
	gcc -g inetsample2.c proto.o -Wall $(EXTRA_INCLUDES) -o inetclient

# sterge fisierele generate la compilare
clean:
	rm -f $(OBJECTS) *.nsmap soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h *.xsd *.wsdl serverds inetclient