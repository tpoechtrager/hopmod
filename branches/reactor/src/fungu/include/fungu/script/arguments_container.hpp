/*   
 *   The Fungu Scripting Engine Library
 *   
 *   Copyright (c) 2008-2009 Graham Daws.
 *
 *   Distributed under a BSD style license (see accompanying file LICENSE.txt)
 */
#ifndef FUNGU_SCRIPT_ARGUMENTS_CONTAINER_HPP
#define FUNGU_SCRIPT_ARGUMENTS_CONTAINER_HPP

#include "any.hpp"
#include "result_type.hpp"
#include <vector>

namespace fungu{
namespace script{

/**
    
*/
class arguments_container
{
public:
    typedef result_type value_type;
    typedef value_type & reference;
    typedef const value_type & const_reference;

    arguments_container(std::vector<value_type> &);
    
    void push_back(const value_type &);

    value_type & front();
    value_type & back();
    
    template<typename T>
    T casted_front()
    {
        if(m_front->get_type() != typeid(T))
            *m_front = lexical_cast<T>(*m_front);
        return *any_cast<T>(&(*m_front));
    }
    
    any & front_reference();
    value_type & safe_front();
    
    template<typename T>
    T safe_casted_front()
    {
        if(empty()) throw error(NOT_ENOUGH_ARGUMENTS);
        return casted_front<T>();
    }
    
    void pop_front();
    
    std::size_t size()const;
    bool empty()const;
private:
    std::vector<value_type> & m_arguments;
    std::vector<value_type>::iterator m_front;
};

} //namespace script
} //namespace fungu

#endif