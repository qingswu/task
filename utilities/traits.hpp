//
// dsa is a utility library of data structures and algorithms built with C++11.
// This file (invoke.hpp) is part of the dsa project.
//
// utilities; general program utilities for the dsa project. 
//
// author: Dalton Woodard
// contact: daltonmwoodard@gmail.com
// repository: https://github.com/daltonwoodard/utility.git
// license:
//
// Copyright (c) 2016 DaltonWoodard. See the COPYRIGHT.md file at the top-level
// directory or at the listed source repository for details.
//
//      Licensed under the Apache License. Version 2.0:
//          https://www.apache.org/licenses/LICENSE-2.0
//      or the MIT License:
//          https://opensource.org/licenses/MIT
//      at the licensee's option. This file may not be copied, modified, or
//      distributed except according to those terms.
//

#ifndef DSA_UTILITY_TRAITS_HPP
#define DSA_UTILITY_TRAITS_HPP

#include <functional>
#include <future>
#include <type_traits>


namespace dsa
{
namespace utility
{
    template <class>
    struct is_reference_wrapper : std::false_type {};

    template <class T>
    struct is_reference_wrapper <std::reference_wrapper <T>>
        : std::true_type {};

    template <class T>
    struct decay_reference_wrapper
    {
        using type = T;
    };

    template <class T>
    struct decay_reference_wrapper <std::reference_wrapper <T>>
    {
        using type = T;
    };

    template <class>
    struct is_future : std::false_type {};

    template <class T>
    struct is_future <std::future <T>> : std::true_type {};

    template <class T>
    struct decay_future 
    {
        using type = T;
    };

    template <class T>
    struct decay_future <std::future <T>>
    {
        using type = T;
    };
}   // namespace utility
}   // namespace dsa

#endif  // #ifndef DSA_UTILITY_TRAITS_HPP
