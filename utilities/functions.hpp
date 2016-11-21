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

#ifndef DSA_UTILITY_FUNCTIONS_HPP
#define DSA_UTILITY_FUNCTIONS_HPP

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include "sequence.hpp"
#include "traits.hpp"


namespace dsa
{
namespace utility
{
namespace detail
{
    /*
     * invoke implemented as per the C++17 standard specification.
     */
    template <class B, class T, class D, class ... Args>
    constexpr inline auto invoke_ (T B::*f, D && d, Args && ... args)
        noexcept (noexcept (
            (std::forward <D> (d).*f) (std::forward <Args> (args)...)
        ))
        -> typename std::enable_if <
            std::is_function <T>::value &&
                std::is_base_of <B, typename std::decay <D>::type>::value,
            decltype ((std::forward <D> (d).*f) (std::forward <Args> (args)...))
        >::type
    {
        return (std::forward <D> (d).*f) (std::forward <Args> (args)...);
    }

    template <class B, class T, class R, class ... Args>
    constexpr inline auto invoke_ (T B::*f, R && r, Args && ... args)
        noexcept (noexcept (
            (r.get ().*f) (std::forward <Args> (args)...)
        ))
        -> typename std::enable_if <
            std::is_function <T>::value &&
                is_reference_wrapper <typename std::decay <R>::type>::value,
            decltype ((r.get ().*f) (std::forward <Args> (args)...))
        >::type
    {
        return (r.get ().*f) (std::forward <Args> (args)...);
    }

    template <class B, class T, class P, class ... Args>
    constexpr inline auto invoke_ (T B::*f, P && p, Args && ... args)
        noexcept (noexcept (
            ((*std::forward <P> (p)).*f) (std::forward <Args> (args)...)
        ))
        -> typename std::enable_if <
            std::is_function <T>::value &&
                !is_reference_wrapper <typename std::decay <P>::type>::value &&
                !std::is_base_of <B, typename std::decay <P>::type>::value,
            decltype (((*std::forward <P> (p)).*f) (
                    std::forward <Args> (args)...
            ))
        >::type
    {
        return ((*std::forward <P> (p)).*f) (std::forward <Args> (args)...);
    }

    template <class B, class T, class D>
    constexpr inline auto invoke_ (T B::*m, D && d)
        noexcept (noexcept (std::forward <D> (d).*m))
        -> typename std::enable_if <
            !std::is_function <T>::value &&
                std::is_base_of <B, typename std::decay <D>::type>::value,
            decltype (std::forward <D> (d).*m)
        >::type
    {
        return std::forward <D> (d).*m;
    }

    template <class B, class T, class R>
    constexpr inline auto invoke_ (T B::*m, R && r)
        noexcept (noexcept (r.get ().*m))
        -> typename std::enable_if <
            !std::is_function <T>::value &&
                is_reference_wrapper <typename std::decay <R>::type>::value,
            decltype (r.get ().*m)
        >::type
    {
        return r.get ().*m;
    }

    template <class B, class T, class P>
    constexpr inline auto invoke_ (T B::*m, P && p)
        noexcept (noexcept ((*std::forward <P> (p)).*m))
        -> typename std::enable_if <
            !std::is_function <T>::value &&
                !is_reference_wrapper <typename std::decay <P>::type>::value &&
                !std::is_base_of <B, typename std::decay <P>::type>::value,
            decltype ((*std::forward <P> (p)).*m)
        >::type
    {
        return (*std::forward <P> (p)).*m;
    }

    template <class Callable, class ... Args>
    constexpr inline auto invoke_ (Callable && c, Args && ... args)
        noexcept (noexcept (
            std::forward <Callable> (c) (std::forward <Args> (args)...)
        ))
        -> decltype (
            std::forward <Callable> (c) (std::forward <Args> (args)...)
        )
    {
        return std::forward <Callable> (c) (std::forward <Args> (args)...);
    }

    /*
     * apply implemented as per the C++17 standard specification.
     */
    template <class F, class T, std::size_t ... I>
    constexpr inline auto apply_ (F && f, T && t, index_sequence <I...>)
        noexcept (noexcept (
            invoke_ (
                std::forward <F> (f), std::get <I> (std::forward <T> (t))...
            )
        ))
        -> decltype (
            invoke_ (
                std::forward <F> (f), std::get <I> (std::forward <T> (t))...
            )
        )
    {
        return invoke_ (
            std::forward <F> (f), std::get <I> (std::forward <T> (t))...
        );
    }
}   // namespace detail

    template <class F, class ... Args>
    constexpr inline auto invoke (F && f, Args && ... args)
        noexcept (noexcept (
            detail::invoke_ (std::forward <F> (f),
                             std::forward <Args> (args)...)
        ))
        -> decltype (
            detail::invoke_ (std::forward <F> (f),
                             std::forward <Args> (args)...)
        )
    {
        return detail::invoke_ (
            std::forward <F> (f), std::forward <Args> (args)...
        );
    }

    template <class F, class T>
    constexpr inline auto apply (F && f, T && t)
        noexcept (noexcept (
            detail::apply_ (
                std::forward <F> (f), std::forward <T> (t),
                make_index_sequence <std::tuple_size <T>::value> {}
            )
        ))
        -> decltype (
            detail::apply_ (
                std::forward <F> (f), std::forward <T> (t),
                make_index_sequence <std::tuple_size <T>::value> {}
            )
        )
    {
        return detail::apply_ (
            std::forward <F> (f), std::forward <T> (t),
            make_index_sequence <std::tuple_size <T>::value> {}
        );
    }
}   // namespace utility
}   //  namespace dsa

#endif  // #ifndef DSA_UTILITY_INVOKE_HPP
