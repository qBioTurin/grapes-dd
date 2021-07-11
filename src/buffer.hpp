/*
Copyright (c) 2020

GRAPES is provided under the terms of The MIT License (MIT):

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <iostream>
#include <vector>

#include "dd_utils.hpp"


using int_vector = std::vector<int>;

/** Buffer for the data of a single graph. */
class Buffer : private std::vector<int_vector> { 
    using base = std::vector<int_vector>; 
    using value_vector = std::vector<long>;

    //iterator to the current buffer 
    Buffer::iterator current; 
    
    //container for output values 
    value_vector values; 
    //iterator to the current return value 
    value_vector::iterator current_value; 
    //enable the use of values vector 
    const bool enable_values; 

    size_t num_current_elements; 

    std::vector<int*> pbuffer; 

public: 
    //size of an internal buffer 
    const size_t element_size; 

    //constructor for empty buffer
    inline Buffer(size_t elem_size, bool set_values); 
    //constructor for buffer of buffersize elements 
    inline Buffer(size_t buffersize, size_t elem_size, bool set_values);

    inline void flush();
    void flush(MEDDLY::dd_edge& edge, std::vector<long>* terminals = nullptr)  {
        if (num_elements() == 0)    
            return; 

        try {
            MEDDLY::forest *forest = edge.getForest(); 
            MEDDLY::dd_edge tmp(forest); 
            long* t = terminals ? terminals->data() : values_data(); 
            forest->createEdge(data(), t, num_elements(), tmp);

            MEDDLY::apply(MEDDLY::PLUS, edge, tmp, edge);
            tmp.clear();
            flush();
        } catch (MEDDLY::error& e) {
            std::cout << "Meddly error while flushing data in dd edge: " << e.getName() << std::endl; 
            throw MEDDLY::error(e); 
        }
    }

    /** Return a pair containing a pointer to the next element available of the buffer.
     * and a flag setted to true if there are other elements avaiable, false otherwise. */
    inline bool get_slot(int_vector*& slot);

    inline int_vector& push_slot(int value);

    inline Buffer::base::iterator begin() { return base::begin(); }
    inline Buffer::base::iterator end() { return base::end(); }

    inline void save_value(const long v);

    inline int** data() { return pbuffer.data(); }

    inline size_t size() { return base::size(); }

    inline long* values_data() {
        return enable_values ? values.data() : nullptr; 
    }

    inline unsigned num_elements() {
        return num_current_elements;
    }

    inline void show_content() const; 
}; 


Buffer::Buffer(size_t elem_size, bool set_values)
    :   enable_values(set_values),  
        num_current_elements(0), 
        element_size(elem_size) {
}

Buffer::Buffer(size_t buffersize, size_t elem_size, bool set_values) 
    : Buffer(elem_size, set_values) {

    resize(buffersize); 

    if (enable_values) {
        values.resize(buffersize); 
        current_value = values.begin();
    } 

    for (auto it = begin(); it != end(); ++it) {
        it->resize(element_size); 
        pbuffer.push_back(it->data());
    }

    current = begin();  
}


inline void Buffer::flush() {
    current = begin(); 
    num_current_elements = 0; 

    if (enable_values)
        current_value = values.begin(); 
}


inline bool Buffer::get_slot(int_vector*& slot) {
    slot = std::addressof(*current);
    ++num_current_elements; 

    if (++current == end()) {
        current = begin(); 
        return false; 
    }

    return true; 
}


inline int_vector& Buffer::push_slot(int value) {
    //append a new int_vector of length=element_size to the buffer
    emplace_back(element_size);
    int_vector& last_vector = back(); 
    pbuffer.push_back(last_vector.data()); 
    values.push_back(value); 
    ++num_current_elements; 
    return last_vector; 
}

inline void Buffer::save_value(const long v) {
    if (enable_values) {
        *current_value = v; 

        if (++current_value == values.end())
            current_value = values.begin(); 
    }
}

inline void Buffer::show_content() const {
    for (int i = 0; i < num_current_elements; ++i) {
        // const int_vector& v = at(i); 

        for (int val: at(i))
            std::cout << val << " ";
        if (enable_values)
            std::cout << "  ==> " << values.at(i); 
        std::cout << "\n"; 
    }
    std::cout << std::endl; 
}
#endif