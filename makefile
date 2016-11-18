# main
EXE=main

# Main target
all: $(EXE)

CFLG=-g
LIBS=-pthread -lcrypto
CLEAN=rm -f dfs dfc *.o *.a

# Compile
.cpp.o:
	g++ -c $(CFLG) $<  $(LIBS)
# Link
main: dfs.o dfc.o
	g++ -o dfs dfs.o  $(LIBS)
	g++ -o dfc dfc.o  $(LIBS)

dfc:
	g++ -c $(CFLG) dfc.cpp  $(LIBS)
	g++ -o dfc dfc.o $(LIBS)

#  Clean
clean:
	$(CLEAN)
