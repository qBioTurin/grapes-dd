CC=g++
NAME=grapes_dd
WARNS=-Wall -Wextra
LIBS=-lmeddly -pthread 
#DEBUG=-g -ggdb
OPTIMIZE=-O3
LINK_GRAPES=-L./GRAPES/vflib2/lib -lvf -lstdc++ -lm $(LIBS)
INCLUDE_GRAPES=-I./GRAPES/vflib2/include -I./GRAPES/RILib -I./GRAPES/GRAPESLib -I./GRAPES/GRAPESLib/data -I./GRAPES/GRAPESLib/pp
SETTINGS=$(OPTIMIZE) $(DEBUG) # $(WARNS)

all: grapes_dd

ordeeeeer: try_order.o mtdds.o matching.o 
	$(CC) -o ordeeeeer $^ $(LINK_GRAPES) $(SETTINGS)

try_order.o: try_order.cpp
	$(CC) -c try_order.cpp $(INCLUDE_GRAPES) $(SETTINGS)

grapes_dd: grapes_dd.o mtdds.o  matching.o
	$(CC) -o $(NAME) $^ $(LINK_GRAPES) $(SETTINGS)

grapes_dd.o: grapes_dd.cpp 
	$(CC) -c grapes_dd.cpp $(INCLUDE_GRAPES) $(SETTINGS)

mtdds.o: mtdds.cpp mtdds.hpp single_buffer.hpp 
	$(CC) -c mtdds.cpp $(INCLUDE_GRAPES) $(SETTINGS)

matching.o: matching.cpp matching.hpp single_buffer.hpp
	$(CC) -c matching.cpp $(INCLUDE_GRAPES) $(SETTINGS)

clean: 
	rm -f *.o *.gch

clean_all:
	rm -f *.o *.gch *.pdf *.dot
