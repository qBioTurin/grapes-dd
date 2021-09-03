CC=g++
NAME=grapes_dd
LIB=./lib/

INCLUDES=-I$(LIB)GRAPES/vflib2/include -I$(LIB)GRAPES/RILib -I$(LIB)GRAPES/GRAPESLib -I$(LIB)GRAPES/GRAPESLib/data -I$(LIB)GRAPES/GRAPESLib/pp
INCLUDES += -I$(LIB)meddly/include
LINKING=-L$(LIB)GRAPES/vflib2/lib -lvf -lstdc++ -lm 
LINKING += lib/meddly/lib/libmeddly.a -pthread

WARNS=-Wall -Wextra
# DEBUG=-g -ggdb 
OPTIMIZE=-O3 -funroll-loops

SETTINGS=$(OPTIMIZE) $(DEBUG)# $(WARNS)


all: grapes_dd dd_one orderings heu metrics

metrics: metrics.o heuristics.o mtmdd.o matching.o 
	$(CC) -o metrics $^ $(LINKING) $(SETTINGS)

metrics.o: metrics.cpp	
	$(CC) -c metrics.cpp $(INCLUDES) $(SETTINGS)

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
