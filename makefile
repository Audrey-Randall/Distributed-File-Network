# main
EXE=main

# Main target
all: $(EXE)

CFLG=-g
LIBS=-pthread
CLEAN=rm -f $(EXE) *.o *.a

# Compile
.cpp.o:
	g++ -c $(CFLG) $<  $(LIBS)
# Link
main: dfs.o dfc.o
	g++ -o dfs dfs.o  $(LIBS)
	g++ -o dfc dfc.o  $(LIBS)

#  Clean
clean:
	$(CLEAN)
