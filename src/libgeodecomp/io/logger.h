#ifndef LIBGEODECOMP_IO_LOGGER_H
#define LIBGEODECOMP_IO_LOGGER_H

#include <libgeodecomp/config.h>
#include <libgeodecomp/io/timestringconversion.h>
#include <libgeodecomp/misc/scopedtimer.h>

// Kill warning 4514 in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

#ifdef LIBGEODECOMP_WITH_HPX
#include <hpx/config.hpp>
#endif

#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4820 )
#endif
#include <iomanip>
#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

namespace LibGeoDecomp {

/**
 * Logger provides a set of functions and macros to selectively print
 * different amounts/levels of debugging information. Filtering
 * happens at compile time, thus no runtime overhead is incurred for
 * filtered messages.
 */
class Logger {
public:
    static const int FATAL = 0;
    static const int FAULT = 1;
    static const int WARN  = 2;
    static const int INFO  = 3;
    static const int DBG   = 4;
};

}

#if LIBGEODECOMP_DEBUG_LEVEL < 0
#define LOG(LEVEL, MESSAGE)
#endif

#if LIBGEODECOMP_DEBUG_LEVEL >= 0
#define LOG_MAIN(LEVEL, MESSAGE)                                        \
    if ((LibGeoDecomp::Logger::LEVEL) <= LIBGEODECOMP_DEBUG_LEVEL) {    \
        std::cout << #LEVEL[0] << ", ["                                 \
                  << TimeStringConversion::renderISO(ScopedTimer::time()) \
                  << "] " << std::right                                 \
                  << std::setw(5) << #LEVEL                             \
                  << " -- " << MESSAGE << "\n";                         \
    }

// on MSVC builds disable warning about static conditionals:
#ifdef _MSC_BUILD

#define LOG(LEVEL, MESSAGE)                                     \
    __pragma( warning( push ) )                                 \
    __pragma( warning( disable : 4127 ) )                       \
    LOG_MAIN(LEVEL, MESSAGE)                                    \
    __pragma( warning( pop ) )                                  \

#else

#define LOG(LEVEL, MESSAGE)                                     \
    LOG_MAIN(LEVEL, MESSAGE)                                    \

#endif

#endif

#endif
