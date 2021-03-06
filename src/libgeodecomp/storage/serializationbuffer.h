#ifndef LIBGEODECOMP_STORAGE_SERIALIZATIONBUFFER_H
#define LIBGEODECOMP_STORAGE_SERIALIZATIONBUFFER_H

#include <libflatarray/flat_array.hpp>
#include <libgeodecomp/misc/apitraits.h>

namespace LibGeoDecomp {

namespace SerializationBufferHelpers {

/**
 * This is an n-way switch to allow other classes to select the
 * appropriate type to buffer regions of a grid; for use with
 * GridBase::loadRegion() and saveRegion().
 */
template<typename CELL, typename SUPPORTS_SOA = void, typename SUPPORTS_BOOST_SERIALIZATION = void>
class Implementation
{
public:
    typedef std::vector<CELL> BufferType;
    typedef CELL ElementType;
    typedef typename APITraits::TrueType FixedSize;

    template<typename REGION>
    static BufferType create(const REGION& region)
    {
        return BufferType(minimumStorageSize(region));
    }

    template<typename REGION>
    static std::size_t minimumStorageSize(const REGION& region)
    {
        return region.size();
    }

    template<typename REGION>
    static void resize(BufferType *buffer, const REGION& region)
    {
        return buffer->resize(minimumStorageSize(region));
    }

    static ElementType *getData(BufferType& buffer)
    {
        return &buffer.front();
    }

    static inline ElementType *getInsertIterator(BufferType *buffer)
    {
        return buffer->data();
    }

#ifdef LIBGEODECOMP_WITH_MPI
    static inline MPI_Datatype cellMPIDataType()
    {
        return APITraits::SelectMPIDataType<ElementType>::value();
    }
#endif
};

/**
 * see above
 */
template<typename CELL>
class Implementation<CELL, typename CELL::API::SupportsSoA, void>
{
public:
    typedef std::vector<char> BufferType;
    typedef char ElementType;
    typedef typename APITraits::TrueType FixedSize;

    template<typename REGION>
    static BufferType create(const REGION& region)
    {
        return BufferType(minimumStorageSize(region));
    }

    template<typename REGION>
    static std::size_t minimumStorageSize(const REGION& region)
    {
        return LibFlatArray::aggregated_member_size<CELL>::VALUE * region.size();
    }

    template<typename REGION>
    static void resize(BufferType *buffer, const REGION& region)
    {
        return buffer->resize(minimumStorageSize(region));
    }

    static ElementType *getData(BufferType& buffer)
    {
        return &buffer.front();
    }

    static inline ElementType *getInsertIterator(BufferType *buffer)
    {
        return buffer->data();
    }

#ifdef LIBGEODECOMP_WITH_MPI
    static inline MPI_Datatype cellMPIDataType()
    {
        return MPI_CHAR;
    }
#endif
};

/**
 * see above
 */
template<typename CELL>
class Implementation<CELL, void, typename CELL::API::SupportsBoostSerialization>
{
public:
    typedef std::vector<char> BufferType;
    typedef char ElementType;
    typedef typename APITraits::FalseType FixedSize;

    template<typename REGION>
    static BufferType create(const REGION& region)
    {
        return BufferType();
    }

    template<typename REGION>
    static std::size_t minimumStorageSize(const REGION& region)
    {
        return region.size();
    }

    template<typename REGION>
    static void resize(BufferType *buffer, const REGION& region)
    {
        buffer->resize(0);
    }

    static ElementType *getData(BufferType& buffer)
    {
        return &buffer.front();
    }

//     static inline InsertIteratorType getInsertIterator(BufferType *buffer)
//     {
// typedef boost::iostreams::back_insert_device<std::vector<char> > DeviceType;
// DeviceType sink(*buffer);
// boost::iostreams::stream<Device> stream(sink);
// return boost::archive::binary_oarchive archive(stream);
//     }

#ifdef LIBGEODECOMP_WITH_MPI
    static inline MPI_Datatype cellMPIDataType()
    {
        return MPI_CHAR;
    }
#endif
};

}

/**
 * This class provides a uniform interface for the different ways that
 * cell classes can be serialized. It is a bridge between the grid
 * class which holds the data and other classes, e.g. a PatchLink for
 * synchronization of ghost zones.
 */
template<typename CELL>
class SerializationBuffer
{
public:
    typedef SerializationBufferHelpers::Implementation<CELL> Implementation;
    typedef typename Implementation::BufferType BufferType;
    typedef typename Implementation::ElementType ElementType;
    typedef typename Implementation::FixedSize FixedSize;
    // typedef typename Implementation::InsertIteratorType InsertIteratorType;

    /**
     * Allocates a buffer that can hold an excerpt of a grid as
     * described by the Region.
     */
    template<typename REGION>
    static inline BufferType create(const REGION& region)
    {
        return Implementation::create(region);
    }

    /**
     * Returns a pointer to the serialized data. The data there is
     * expected to be serializable via MPI (i.e. is either bytewise
     * serializable or an MPI datatype exists).
     */
    static inline ElementType *getData(BufferType& buffer)
    {
        return Implementation::getData(buffer);
    }

    /**
     * Returns the minimum number of bytes that are required to store
     * the number of cells as outlined by region.
     *
     * For standard and SoA models this is generally identical with
     * the amount of actually allocated memory. For dynamic
     * serialization such as Boost Serialization the actual ammount is
     * often larger.
     */
    template<typename REGION>
    static std::size_t minimumStorageSize(const REGION& region)
    {
        return Implementation::minimumStorageSize(region);
    }

    /**
     * Adapts the buffer size so that it can hold a set of cells as
     * described by region.
     */
    template<typename REGION>
    static inline void resize(BufferType *buffer, const REGION& region)
    {
        Implementation::resize(buffer, region);
    }

    // static inline InsertIteratorType getInsertIterator(BufferType *buffer)
    // {
    //     Implementation::getInsertIterator(buffer);
    // }

#ifdef LIBGEODECOMP_WITH_MPI
    /**
     * Returns the MPI datatype which should be used to transmission.
     */
    static inline  MPI_Datatype cellMPIDataType()
    {
        return Implementation::cellMPIDataType();
    }
#endif
};

}

#endif
