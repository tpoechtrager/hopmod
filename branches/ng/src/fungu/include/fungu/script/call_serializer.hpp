/*   
 *   The Fungu Scripting Engine Library
 *   
 *   Copyright (c) 2008-2009 Graham Daws.
 *
 *   Distributed under a BSD style license (see accompanying file LICENSE.txt)
 */
#ifndef FUNGU_SCRIPT_CALL_SERIALIZER_HPP
#define FUNGU_SCRIPT_CALL_SERIALIZER_HPP

#include "env.hpp"
#include "error.hpp"
#include "lexical_cast.hpp"
#include "parse_array.hpp"
#include "../dynamic_caller.hpp"
#include "code_block.hpp"

#include <list>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace fungu{
namespace script{

class call_serializer
{
public:
    typedef result_type return_type;
    typedef env::object::apply_arguments::value_type serialized_argument_type;
    
    call_serializer(env::object::apply_arguments & argv,env::frame * frame)
     :m_argv(argv),m_frame(frame)
    {
        
    }
    
    template<typename T>
    return_type serialize(const T & value)
    {
        return value;
    }
    
    result_type serialize(result_type value)
    {
        return value;
    }
    
    template<typename T>
    T deserialize(const serialized_argument_type &, target_tag<T>)
    {
        return m_argv.casted_front<T>();
    }
    
    serialized_argument_type & deserialize(serialized_argument_type & value, target_tag<serialized_argument_type>)
    {
        return value;
    }
    
    const char * deserialize(const serialized_argument_type & value, target_tag<const char *>)
    {
        m_string_list.push_back(value.to_string().copy());
        return m_string_list.back().begin();
    }
    
    template<typename T>
    std::vector<T> deserialize(const serialized_argument_type & value, target_tag<std::vector<T> >)
    {
        std::vector<T> a;
        parse_array<std::vector<T>,true>(value.to_string(), m_frame, a);
        return a;
    }
    
    template<typename T>
    boost::shared_ptr<T> deserialize(const serialized_argument_type & value, target_tag<boost::shared_ptr<T> >)
    {
        return any_cast<boost::shared_ptr<T> >(value);
    }
    
    template<typename T>
    boost::intrusive_ptr<T> deserialize(const serialized_argument_type & value, target_tag<boost::intrusive_ptr<T> >)
    {
        return any_cast<boost::intrusive_ptr<T> >(value);
    }
    
    code_block deserialize(const serialized_argument_type &, target_tag<code_block>)
    {
        any & arg = m_argv.front_reference();
        if(arg.get_type() != typeid(code_block))
        {
            arg = code_block(arg.to_string(), m_frame->get_env()->get_source_context());
            any_cast<code_block>(&arg)->compile(m_frame);
        }
        return any_cast<code_block>(arg);
    }
    
    return_type get_void_value()
    {
        return any::null_value();
    }
private:
    env::object::apply_arguments & m_argv; 
    env::frame * m_frame;
    std::list<const_string> m_string_list;
};

} //namespace script
} //namespace fungu

#endif
