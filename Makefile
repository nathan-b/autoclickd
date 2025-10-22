DBFLAGS=-g -O0 -DDEBUG
NDBFLAGS=-O2
CPPFLAGS=-Wall -Werror
OUTPUT=ac
TEST_OUTPUT=test_ac

CFILES=autoclick.c
TEST_CFILES=test_autoclick.c

LIBS=-lX11 -lXtst -lXi
TEST_LIBS=-lcmocka

debug: $(CFILES)
	gcc $(CPPFLAGS) $(DBFLAGS) $(INCLUDES) -o $(OUTPUT) $(CFILES) $(LIBS)

release: $(CFILES)
	gcc $(CPPFLAGS) $(NDBFLAGS) -o $(OUTPUT) $(CFILES) $(LIBS)

test: $(TEST_CFILES) $(CFILES)
	gcc $(CPPFLAGS) $(DBFLAGS) -DTEST_BUILD -o $(TEST_OUTPUT) $(TEST_CFILES) $(LIBS) $(TEST_LIBS)
	./$(TEST_OUTPUT)

clean:
	-rm $(OUTPUT) $(TEST_OUTPUT)

