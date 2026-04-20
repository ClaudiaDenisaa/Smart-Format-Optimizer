
OS := $(shell uname -s)
EXTRA_INCLUDES = 
EXTRA_LIBS =
ifeq ($(OS), Darwin)
    EXTRA_INCLUDES = -I/opt/homebrew/include
    EXTRA_LIBS = -L/opt/homebrew/lib
endif

OBJECTS=unixds.o inetds.o soapds.o proto.o queue.o

help:
	@echo "Use one of these"
	@echo "	make serverds -- to build the server"
	@echo "	make inetclient -- to build the inet test client"
	@echo "	make clean -- to remove unwanted files"
	@echo ""
	@echo "Notice that you need some libraries to compile:"
	@echo "	sudo apt install gsoap libgsoap-dev"
	@echo "	sudo apt install libncurses-dev libncursesw-dev"
	@echo "	-- NCurses support was removed, however"
	@echo ""
	@echo "If something went wrong, you can try..."
	@echo "	sudo apt update"
	@echo "	sudo apt upgrade"
	
serverds: threeds.c queue.o ${OBJECTS} SampleServices.nsmap
	gcc -g threeds.c soapC.c soapServer.c ${OBJECTS} $(EXTRA_INCLUDES) $(EXTRA_LIBS) -l pthread -l gsoap -Wall -o serverds -lconfig

unixds.o: unixds.c 
	gcc -g unixds.c -c -o unixds.o -Wall -l pthread

inetds.o: inetds2.c proto.o
	gcc -g inetds2.c -c -o inetds.o -Wall  -l pthread

proto.o: proto.c proto.h
	gcc -g proto.c -c -o proto.o -Wall  -l pthread
	
queue.o: queue.c queue.h
	gcc -g queue.c -c -o queue.o -Wall -l pthread	

soapds.o: soapds.c SampleServices.nsmap
	gcc -g soapds.c -c -o soapds.o -Wall $(EXTRA_INCLUDES)

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

inetclient: inetsample2.c proto.o
	gcc -g inetsample2.c proto.o -Wall -o inetclient

clean:
	rm -f ${OBJECTS} *.nsmap soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h *.xsd *.wsdl

# NOTICE: use sudo apt-get install gsoap libgsoap-dev
# NOTICE: use sudo apt-get install libncurses5-dev libncursesw5-dev
# NOTICE: use the sample INET client (gcc inetsample.c -o inets; ./inets)
