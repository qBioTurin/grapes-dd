CC=g++
NAME=grapes_dd
WARNS=-Wall -Wextra
LIBS=-lmeddly -pthread 
#DEBUG=-g -ggdb
OPTIMIZE=-O3 -funroll-loops
LINK_GRAPES=-L./GRAPES/vflib2/lib -lvf -lstdc++ -lm $(LIBS)
INCLUDE_GRAPES=-I./GRAPES/vflib2/include -I./GRAPES/RILib -I./GRAPES/GRAPESLib -I./GRAPES/GRAPESLib/data -I./GRAPES/GRAPESLib/pp
SETTINGS=$(OPTIMIZE) $(DEBUG) # $(WARNS)

all: grapes_dd

entropy: entropy.o mtmdd.o matching.o
	$(CC) -o entropy $^ $(LINK_GRAPES) $(SETTINGS)

entropy.o: entropy.cpp
	$(CC) -c entropy.cpp $(INCLUDE_GRAPES) $(SETTINGS)

grapes_dd: grapes_dd.o mtmdd.o  matching.o
	$(CC) -o $(NAME) $^ $(LINK_GRAPES) $(SETTINGS)

grapes_dd.o: grapes_dd.cpp 
	$(CC) -c grapes_dd.cpp $(INCLUDE_GRAPES) $(SETTINGS)

mtmdd.o: mtmdd.cpp mtmdd.hpp buffer.hpp 
	$(CC) -c mtmdd.cpp $(INCLUDE_GRAPES) $(SETTINGS)

matching.o: matching.cpp matching.hpp buffer.hpp
	$(CC) -c matching.cpp $(INCLUDE_GRAPES) $(SETTINGS)

clean: 
	rm -f *.o *.gch

clean_all:
	rm -f *.o *.gch *.pdf *.dot
