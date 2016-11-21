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

#ifndef DSA_UTILITY_SEQUENCE_HPP
#define DSA_UTILITY_SEQUENCE_HPP

#include <cstddef>


namespace dsa
{
namespace utility
{
#if __cplusplus >= 201402L
    template <std::size_t ... I>
    using index_sequence = std::index_sequence <I...>;

    template <std::size_t N>
    using make_index_sequence = std::make_index_sequence <N>;
#else
    template <std::size_t ... I>
    struct index_sequence
    {
        using type = index_sequence;
        using value_type = std::size_t;

        static constexpr std::size_t size (void) noexcept
        {
            return sizeof... (I);
        }
    };

    template <typename, typename>
    struct merge;

    template <std::size_t ... I1, std::size_t ... I2>
    struct merge <index_sequence <I1...>, index_sequence <I2...>>
        : index_sequence <I1..., (sizeof... (I1) + I2)...>
    {};

    template <std::size_t N>
    struct seq_gen : merge <
        typename seq_gen <N/2>::type,
        typename seq_gen <N - N/2>::type
    >
    {};

    template <>
    struct seq_gen <0> : index_sequence <> {};

    template <>
    struct seq_gen <1> : index_sequence <0> {};

    template <std::size_t N>
    using make_index_sequence = typename seq_gen <N>::type;
#endif
}   // namespace utility
}   // namespace dsa

#endif  // #ifndef DSA_UTILITY_SEQUENCE_HPP
