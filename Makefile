# Makefile para cliente FTP 

CLTOBJ= TipanD-clienteFTP.o connectsock.o connectTCP.o passivesock.o passiveTCP.o errexit.o

all: TipanD-clienteFTP 

TipanD-clienteFTP:	${CLTOBJ}
	cc -o TipanD-clienteFTP ${CLTOBJ}

clean:
	rm $(CLTOBJ) 
