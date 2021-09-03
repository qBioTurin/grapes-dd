#include <iostream>
#include <random>
#include <meddly.h>
#include <meddly_expert.h>

#include "buffer.hpp"

#define NUM_BUFFER_SLOTS 1000
#define MAX_TERMINAL_VALUE 30 


void fill_ev(MEDDLY::dd_edge& edge, Buffer& buffer);

inline void fill_slot(Buffer& buffer, int_vector*& slot, int_vector& dbounds) {
    using it_t = int_vector::iterator; 

    for (it_t bit = dbounds.begin(), sit = slot->begin() + 1; bit != dbounds.end(); ++sit, ++bit) {
        *sit = std::rand() % *bit; 
    }
    buffer.save_value(rand() % MAX_TERMINAL_VALUE); 
}


int main() {
    std::srand(2); 


    MEDDLY::initialize();



    int_vector bounds{3, 3, 5}; 
    const int num_vars = bounds.size() - 1; 

    MEDDLY::forest::policies policy(false); 
    MEDDLY::domain *domain = MEDDLY::createDomainBottomUp(bounds.data(), bounds.size()); 
    MEDDLY::forest *forest = domain->createForest(
        false, 
        MEDDLY::forest::INTEGER, 
        MEDDLY::forest::EVPLUS, 
        policy
    ); 
    MEDDLY::dd_edge edge1(forest), edge2(forest), res(forest);
    MEDDLY::FILE_output meddlyout(stdout); 

    // Buffer buffer(3, num_vars + 1, true); 
    // int_vector *slot = nullptr; 

    int_vector v1 {0, 2, 1, 3}; 
    int_vector v2 {0, 1, 1, 2};
    long term = 3;  

    int *p = v1.data(); 


    forest->createEdge(&p, &term, 1, edge1); 
    term = 2;
    // p = v2.data(); 
    forest->createEdge(&p, &term, 1, edge2); 

    // res = edge1 + edge2; 




    //dd1
    // fill_ev(edge1, buffer); 
    // // forest->createEdge(buffer.data(), buffer.values_data(), buffer.num_elements(), edge1);
    // buffer.flush(); 

    // fill_ev(edge2, buffer); 
    // // forest->createEdge(buffer.data(), buffer.values_data(), 1, edge2);
    // buffer.flush(); 

    std::cout << "\nedge 1 :\n";
    edge1.show(meddlyout, 2); 

    std::cout << "\nedge 2: \n"; 
    edge2.show(meddlyout, 2); 


    // res = edge2 + edge1;

    MEDDLY::apply(MEDDLY::PLUS, edge1, edge2, res); 

    std::cout << "\ntheir sum:\n"; 
    res.show(meddlyout, 2); 


    // res.writePicture("res", "pdf");
    // edge1.writePicture("edge1", "pdf");
    // edge2.writePicture("edge2", "pdf");



/*
    while (buffer.get_slot(slot)) 
        fill_slot(buffer, slot, bounds); 
    
    fill_slot(buffer, slot, bounds); 
    forest->createEdge(buffer.data(), buffer.values_data(), buffer.num_elements(), edge1);

    std::cout << "ev #1:\n";
    edge1.show(meddlyout, 2); 
    buffer.flush(); 

    fill_ev(edge2, buffer); 

    


    MEDDLY::dd_edge tmp(forest), res(forest); 

    forest->createEdge(buffer.data(), buffer.values_data(), buffer.num_elements(), tmp); 
    std::cout << "showing temporary ev-dd:\n";
    tmp.show(meddlyout, 2);

    edge1 += tmp;  
    MEDDLY::apply(MEDDLY::PLUS, tmp, edge1, edge2);

    std::cout << "showing final ev-dd:\n";
    
    edge1.show(meddlyout, 2);

    return 0; */ 
}



void fill_ev(MEDDLY::dd_edge& edge, Buffer& buffer) {
    int_vector *slot = nullptr, bounds; 
    int num_vars = edge.getForest()->getDomain()->getNumVariables();
    const MEDDLY::domain *domain = edge.getForest()->getDomain();

    for (int i = 1; i <= num_vars; ++i)
        bounds.push_back(domain->getVariableBound(i));

    while (buffer.get_slot(slot)) 
        fill_slot(buffer, slot, bounds); 
    fill_slot(buffer, slot, bounds); 

    edge.getForest()->createEdge(buffer.data(), buffer.values_data(), buffer.num_elements(), edge); 
}