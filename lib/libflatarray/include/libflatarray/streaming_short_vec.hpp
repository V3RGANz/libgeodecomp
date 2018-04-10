/**
 * Copyright 2016 Andreas Schäfer
 * Copyright 2018 Google
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef FLAT_ARRAY_STREAMING_SHORT_VEC_HPP
#define FLAT_ARRAY_STREAMING_SHORT_VEC_HPP

#include <libflatarray/short_vec.hpp>

namespace LibFlatArray {

#ifdef __ICC
// disabling this warning as implicit type conversion is exactly our goal here:
#pragma warning push
#pragma warning (disable: 2304)
#endif

template<typename CARGO, std::size_t ARITY>
class streaming_short_vec;

template<typename CARGO, std::size_t ARITY >
inline bool any(const streaming_short_vec<CARGO, ARITY>& vec)
{
    return vec.any();
}

// Don't warn about these functions being stripped from an executable
// as they're not being used, that's actually expected behavior.
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

/**
 * Wraps functionality of short_vec, but replaces all stores by
 * streaming (i.e. non-temporal) stores. Downside: all store addresses
 * must be aligned.
 */
template<typename CARGO, std::size_t ARITY>
class streaming_short_vec : public short_vec<CARGO, ARITY>
{
public:

    inline
    streaming_short_vec(const CARGO val = 0) : short_vec<CARGO, ARITY>(val)
    {}

    inline
    streaming_short_vec(const CARGO *data) : short_vec<CARGO, ARITY>(data)
    {}

    inline
    streaming_short_vec(short_vec<CARGO, ARITY>&& val) : short_vec<CARGO, ARITY>(std::move(val))
    {}

#ifdef LIBFLATARRAY_WITH_CPP14
    inline
    streaming_short_vec(const std::initializer_list<CARGO>& list)
    {
        const CARGO *ptr = static_cast<const CARGO *>(&(*list.begin()));
        load(ptr);
    }
#endif

    using short_vec<CARGO, ARITY>::load;

    inline
    void store(CARGO *data)
    {
        short_vec<CARGO, ARITY>::store_nt(data);
    }

    inline
    void store_aligned(CARGO *data)
    {
        short_vec<CARGO, ARITY>::store_nt(data);
    }
};

#ifdef __ICC
#pragma warning pop
#endif

template<typename CARGO, int ARITY>
inline
void operator<<(double *data, const streaming_short_vec<CARGO, ARITY>& vec)
{
    vec.store_nt(data);
}

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

}

#endif
