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

#ifndef SINGLE_BUFFER_HPP
#define SINGLE_BUFFER_HPP

#include <iostream>
#include <vector>


/** Buffer for the data of a single graph. */
class SingleBuffer : private std::vector<int*> { 
    using base = std::vector<int*>; 

    //iterator to the current buffer 
    SingleBuffer::iterator current; 
    
    //container for output values 
    std::vector<int> values; 
    //iterator to the current element 
    std::vector<int>::iterator current_value; 
    //enable the use of values vector 
    const bool enable_values; 

    size_t num_current_elements; 

    inline int* allocate() {
        return new int[element_size]; 
    }

public: 
    using buffer_slot_t = std::pair<int*, bool>; 
    //size of an internal buffer 
    const size_t element_size; 

    inline SingleBuffer(size_t elem_size, bool set_values = true); 

    inline SingleBuffer(size_t buffersize, size_t elem_size, bool set_values=true);

    inline ~SingleBuffer() {
        for (auto it = begin(); it != end(); ++it)
            delete[] *it; 
    }

    inline void flush();

    /** Return a pair containing a pointer to the next element available of the buffer.
     * and a flag setted to true if there are other elements avaiable, false otherwise. */
    inline buffer_slot_t get_slot();

    inline buffer_slot_t push_slot(int value); 
    inline buffer_slot_t push_slot(); 

    inline SingleBuffer::base::iterator begin() { return base::begin(); }
    inline SingleBuffer::base::iterator end() { return base::end(); }



    inline void save_value(const int v);

    inline int** data() { return base::data(); }
    inline size_t size() { return base::size(); }

    inline int* values_data() {
        return enable_values ? values.data() : nullptr; 
    }

    inline unsigned num_elements() {
        return num_current_elements;
    }

    inline void show_content(); 
}; 


SingleBuffer::SingleBuffer(size_t elem_size, bool set_values) :
enable_values(set_values), 
num_current_elements(0), 
element_size(elem_size) {
    current = begin(); 
}

SingleBuffer::SingleBuffer(size_t buffersize, size_t elem_size, bool set_values) :
enable_values(set_values), 
num_current_elements(0), 
element_size(elem_size) {
    resize(buffersize); 

    if (enable_values) {
        values.resize(buffersize); 
        current_value = values.begin();
    } 

    for (auto it = begin(); it != end(); ++it)
        *it = allocate(); 

    current = begin();  
}


inline void SingleBuffer::flush() {
    current = begin(); 
    num_current_elements = 0; 

    if (enable_values)
        current_value = values.begin(); 
}

inline SingleBuffer::buffer_slot_t SingleBuffer::get_slot() {
    buffer_slot_t ret(*current, false); 
    ++num_current_elements;

    if ((ret.second = ++current != end()) == false)
        current = begin(); 

    return ret; 
}

inline SingleBuffer::buffer_slot_t SingleBuffer::push_slot(int value) {
    buffer_slot_t ret(this->push_slot()); 
    values.push_back(value);
    return ret; 
}

inline SingleBuffer::buffer_slot_t SingleBuffer::push_slot() {
    buffer_slot_t ret(allocate(), false); 
    base::push_back(ret.first); 
    ++num_current_elements; 
    return ret;
}

inline void SingleBuffer::save_value(const int v) {
    if (enable_values) {
        *current_value = v; 

        if (++current_value == values.end())
            current_value = values.begin(); 
    }
}

inline void SingleBuffer::show_content() {
    std::cout << "Buffer has " << num_elements() << " elements\n";
    for (int i = 0; i < num_current_elements; ++i) {
        int *p = at(i); 
        for (int j = 0; j < element_size; ++j) {
            std::cout << p[j] << " "; 
        }
        std::cout << " ==> " << values.at(i) << "\n";
    }
    std::cout << std::endl;
}
#endif