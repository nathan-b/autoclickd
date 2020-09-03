DBFLAGS=-g -O0 -DDEBUG
NDBFLAGS=-O2
CPPFLAGS=-Wall -Werror
OUTPUT=ac

CFILES=autoclick.c

LIBS=-lX11 -lXtst -lXi

debug: $(CFILES)
	gcc $(CPPFLAGS) $(DBFLAGS) $(INCLUDES) -o $(OUTPUT) $(CFILES) $(LIBS)

release: $(CFILES)
	gcc $(CPPFLAGS) $(NDBFLAGS) -o $(OUTPUT) $(CFILES) $(LIBS)

clean:
	-rm $(OUTPUT)

