CC=g++
NAME=grapes_dd

INCLUDES=-I./GRAPES/vflib2/include -I./GRAPES/RILib -I./GRAPES/GRAPESLib -I./GRAPES/GRAPESLib/data -I./GRAPES/GRAPESLib/pp
INCLUDES += -I./meddly/include
LINKING=-L./GRAPES/vflib2/lib -lvf -lstdc++ -lm 
LINKING += meddly/lib/libmeddly.a -pthread

WARNS=-Wall -Wextra
# DEBUG=-g -ggdb 
OPTIMIZE=-O3 -funroll-loops

SETTINGS=$(OPTIMIZE) $(DEBUG)# $(WARNS)


all: grapes_dd dd_one orderings heu


heu: heu.o heuristics.o mtmdd.o matching.o
	$(CC) -o heu $^ $(LINKING) $(SETTINGS)

heuristics.o: heuristics.cpp 
	$(CC) -c heuristics.cpp $(INCLUDES) $(SETTINGS)

heu.o: heu.cpp
	$(CC) -c heu.cpp $(INCLUDES) $(SETTINGS)


dd_one: dd_one.o mtmdd.o matching.o
	$(CC) -o dd_one $^ $(LINKING) $(SETTINGS)

dd_one.o: dd_one.cpp
	$(CC) -c dd_one.cpp $(INCLUDES) $(SETTINGS)

orderings: orderings.o mtmdd.o matching.o
	$(CC) -o orderings $^ $(LINKING) $(SETTINGS)

orderings.o: orderings.cpp
	$(CC) -c orderings.cpp $(INCLUDES) $(SETTINGS)



entropy: entropy.o mtmdd.o matching.o
	$(CC) -o entropy $^ $(LINKING) $(SETTINGS)

entropy.o: entropy.cpp
	$(CC) -c entropy.cpp $(INCLUDES) $(SETTINGS)

grapes_dd: grapes_dd.o mtmdd.o  matching.o
	$(CC) -o $(NAME) $^ $(LINKING) $(SETTINGS)

grapes_dd.o: grapes_dd.cpp 
	$(CC) -c grapes_dd.cpp $(INCLUDES) $(SETTINGS)

mtmdd.o: mtmdd.cpp mtmdd.hpp buffer.hpp 
	$(CC) -c mtmdd.cpp $(INCLUDES) $(SETTINGS)

matching.o: matching.cpp matching.hpp buffer.hpp
	$(CC) -c matching.cpp $(INCLUDES) $(SETTINGS)

clean: 
	rm -f *.o *.gch

clean_all:
	rm -f $(NAMES) *.o *.gch *.pdf *.dot
