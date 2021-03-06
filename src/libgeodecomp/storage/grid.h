#ifndef LIBGEODECOMP_STORAGE_GRID_H
#define LIBGEODECOMP_STORAGE_GRID_H

#include <libflatarray/aligned_allocator.hpp>
#include <libgeodecomp/geometry/coord.h>
#include <libgeodecomp/geometry/coordbox.h>
#include <libgeodecomp/geometry/region.h>
#include <libgeodecomp/geometry/topologies.h>
#include <libgeodecomp/io/logger.h>
#include <libgeodecomp/storage/coordmap.h>
#include <libgeodecomp/storage/gridbase.h>
#include <libgeodecomp/storage/selector.h>

#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_WITH_BOOST_SERIALIZATION
#include <libgeodecomp/misc/cudaboostworkaround.h>
#include <libgeodecomp/communication/boostserialization.h>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#endif

#ifdef LIBGEODECOMP_WITH_HPX
#include <libgeodecomp/misc/cudaboostworkaround.h>
#include <libgeodecomp/communication/hpxserialization.h>
#include <hpx/runtime/serialization/input_archive.hpp>
#include <hpx/runtime/serialization/output_archive.hpp>
#endif

namespace LibGeoDecomp {

template<typename CELL_TYPE, typename GRID_TYPE>
class CoordMap;

#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4820 )
#endif

/**
 * A multi-dimensional regular grid
 */
template<typename CELL_TYPE, typename TOPOLOGY=Topologies::Cube<2>::Topology>
class Grid : public GridBase<CELL_TYPE, TOPOLOGY::DIM>
{
public:
    friend class GridTest;
    friend class TopologiesTest;
    friend class ParallelStripingSimulatorTest;
    const static int DIM = TOPOLOGY::DIM;

    using GridBase<CELL_TYPE, TOPOLOGY::DIM>::loadRegion;
    using GridBase<CELL_TYPE, TOPOLOGY::DIM>::saveRegion;

    // always align on cache line boundaries
    typedef typename std::vector<CELL_TYPE, LibFlatArray::aligned_allocator<CELL_TYPE, 64> > CellVector;
    typedef TOPOLOGY Topology;
    typedef CELL_TYPE Cell;
    typedef CoordMap<CELL_TYPE, Grid<CELL_TYPE, TOPOLOGY> > CoordMapType;

    explicit Grid(
        const Coord<DIM>& dim = Coord<DIM>(),
        const CELL_TYPE& defaultCell = CELL_TYPE(),
        const CELL_TYPE& edgeCell = CELL_TYPE()) :
        dimensions(dim),
        cellVector(std::size_t(dim.prod()), defaultCell),
        edgeCell(edgeCell)
    {}

    explicit Grid(const GridBase<CELL_TYPE, DIM>& base) :
        dimensions(base.dimensions()),
        cellVector(base.dimensions().prod()),
        edgeCell(base.getEdge())
    {
        CoordBox<DIM> box = base.boundingBox();
        for (typename CoordBox<DIM>::Iterator i = box.begin(); i != box.end(); ++i) {
            set(*i - box.origin, base.get(*i));
        }
    }

    Grid& operator=(const Grid& other)
    {
        resize(other.getDimensions());
        std::copy(other.cellVector.begin(), other.cellVector.end(), cellVector.begin());
        edgeCell = other.edgeCell;

        return *this;
    }

    inline void resize(const CoordBox<DIM>& newBox)
    {
        if (newBox.origin != Coord<DIM>()) {
            throw std::logic_error("Grid can't handle origin in resize(CoordBox)");
        }

        resize(newBox.dimensions);
    }

    inline void resize(const Coord<DIM>& newDim)
    {
        dimensions = newDim;
        cellVector.resize(std::size_t(newDim.prod()));
    }

    /**
     * returns a map that is referenced by relative coordinates from the
     * originating coordinate coord.
     */
    inline CoordMapType getNeighborhood(const Coord<DIM>& center) const
    {
        return CoordMapType(center, this);
    }

    inline CELL_TYPE& getEdgeCell()
    {
        return edgeCell;
    }

    inline const CELL_TYPE& getEdgeCell() const
    {
        return edgeCell;
    }

    /**
     * Return a pointer to the underlying data storage. Use with care!
     */
    inline
    CELL_TYPE *data()
    {
        return cellVector.data();
    }

    /**
     * Return a const pointer to the underlying data storage. Use with
     * care!
     */
    inline
    const CELL_TYPE *data() const
    {
        return cellVector.data();
    }

    inline CELL_TYPE& operator[](const Coord<DIM>& coord)
    {
        return Topology::locate(cellVector, coord, dimensions, edgeCell);
    }

    inline const CELL_TYPE& operator[](const Coord<DIM>& coord) const
    {
        return Topology::locate(cellVector, coord, dimensions, edgeCell);
    }

    inline bool operator==(const Grid& other) const
    {
        if (boundingBox() == CoordBox<DIM>() &&
            other.boundingBox() == CoordBox<DIM>()) {
            return true;
        }

        return
            (edgeCell   == other.edgeCell) &&
            (cellVector == other.cellVector);
    }

    inline bool operator==(const GridBase<CELL_TYPE, TOPOLOGY::DIM>& other) const
    {
        if (boundingBox() != other.boundingBox()) {
            return false;
        }

        if (edgeCell != other.getEdge()) {
            return false;
        }

        CoordBox<DIM> box = boundingBox();
        for (typename CoordBox<DIM>::Iterator i = box.begin(); i != box.end(); ++i) {
            if ((*this)[*i] != other.get(*i)) {
                return false;
            }
        }

        return true;
    }

    inline bool operator!=(const Grid& other) const
    {
        return !(*this == other);
    }

    inline bool operator!=(const GridBase<CELL_TYPE, TOPOLOGY::DIM>& other) const
    {
        return !(*this == other);
    }

    virtual void set(const Coord<DIM>& coord, const CELL_TYPE& cell)
    {
        (*this)[coord] = cell;
    }

    virtual void set(const Streak<DIM>& streak, const CELL_TYPE *cells)
    {
        Coord<DIM> cursor = streak.origin;


        if (boundingBox().inBounds(streak)) {
            CELL_TYPE *target = &(*this)[cursor];
            std::copy(cells, cells + streak.length(), target);
            return;
        }

        for (; cursor.x() < streak.endX; ++cursor.x()) {
            (*this)[cursor] = *cells;
            ++cells;
        }
    }

    virtual CELL_TYPE get(const Coord<DIM>& coord) const
    {
        return (*this)[coord];
    }

    virtual void get(const Streak<DIM>& streak, CELL_TYPE *cells) const
    {
        Coord<DIM> cursor = streak.origin;

        if (boundingBox().inBounds(streak)) {
            const CELL_TYPE *source = &(*this)[cursor];
            std::copy(source, source + streak.length(), cells);
            return;
        }

        for (; cursor.x() < streak.endX; ++cursor.x()) {
            *cells = (*this)[cursor];
            ++cells;
        }
    }

    virtual void setEdge(const CELL_TYPE& cell)
    {
        getEdgeCell() = cell;
    }

    virtual const CELL_TYPE& getEdge() const
    {
        return getEdgeCell();
    }

    inline const Coord<DIM>& getDimensions() const
    {
        return dimensions;
    }

    inline std::string toString() const
    {
        std::ostringstream message;
        message << "Grid<" << DIM << ">(\n"
                << "boundingBox: " << boundingBox()  << "\n"
                << "edgeCell:\n"
                << edgeCell << "\n";

        CoordBox<DIM> box = boundingBox();
        for (typename CoordBox<DIM>::Iterator i = box.begin(); i != box.end(); ++i) {
            message << "Coord" << *i << ":\n" << (*this)[*i] << "\n";
        }

        message << ")";
        return message.str();
    }

    virtual CoordBox<DIM> boundingBox() const
    {
        return CoordBox<DIM>(Coord<DIM>(), dimensions);
    }

    void saveRegion(
        std::vector<CELL_TYPE> *buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset = Coord<DIM>()) const
    {
        CELL_TYPE *target = buffer->data();

        typename Region<DIM>::StreakIterator end = region.endStreak(offset);
        for (typename Region<DIM>::StreakIterator i = region.beginStreak(offset); i != end; ++i) {
            get(*i, target);
            target += i->length();
        }
    }

#ifdef LIBGEODECOMP_WITH_BOOST_SERIALIZATION
    void saveRegion(
        std::vector<char> *buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset = Coord<DIM>()) const
    {
        typedef typename APITraits::SelectBoostSerialization<CELL_TYPE>::Value Trait;
        saveRegionImplementation(buffer, region, offset, Trait());
    }
#endif

    void loadRegion(
        const std::vector<CELL_TYPE>& buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset = Coord<DIM>())
    {
        const CELL_TYPE *source = buffer.data();

        typename Region<DIM>::StreakIterator end = region.endStreak(offset);
        for (typename Region<DIM>::StreakIterator i = region.beginStreak(offset); i != end; ++i) {
            set(*i, source);
            source += i->length();
        }
    }

#ifdef LIBGEODECOMP_WITH_BOOST_SERIALIZATION
    void loadRegion(
        const std::vector<char>& buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset = Coord<DIM>())
    {
        typedef typename APITraits::SelectBoostSerialization<CELL_TYPE>::Value Trait;
        loadRegionImplementation(buffer, region, offset, Trait());
    }
#endif

protected:

#ifdef LIBGEODECOMP_WITH_BOOST_SERIALIZATION
    void saveRegionImplementation(
        std::vector<char> *buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset,
        const APITraits::FalseType&) const
    {
        // fixme: throw exception, should not be called
    }

    void saveRegionImplementation(
        std::vector<char> *buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset,
        const APITraits::TrueType&) const
    {
        // fixme:
        // #ifdef LIBGEODECOMP_WITH_HPX
        //          int archive_flags = boost::archive::no_header;
        //          archive_flags |= hpx::util::disable_data_chunking;
        //          hpx::util::binary_filter *f = 0;
        //          hpx::serialization::output_archive archive(*vec, f, archive_flags);
        // #else
        typedef boost::iostreams::back_insert_device<std::vector<char> > Device;
        Device sink(*buffer);
        boost::iostreams::stream<Device> stream(sink);
        boost::archive::binary_oarchive archive(stream);
        // #endif

        for (typename Region<DIM>::Iterator i = region.begin(); i != region.end(); ++i) {
            archive & (*this)[*i];
        }
    }

    void loadRegionImplementation(
        const std::vector<char>& buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset,
        const APITraits::FalseType&)
    {
        // fixme: throw exception here, should not be called
    }

    void loadRegionImplementation(
        const std::vector<char>& buffer,
        const Region<DIM>& region,
        const Coord<DIM>& offset,
        const APITraits::TrueType&)
    {
        // fixme:
        //        #ifdef LIBGEODECOMP_WITH_HPX
        //         int archive_flags = boost::archive::no_header;
        //         archive_flags |= hpx::util::disable_data_chunking;
        //         hpx::serialization::input_archive archive(vec, vec.size(), archive_flags);
        // #else
        typedef boost::iostreams::basic_array_source<char> Device;
        Device source(&buffer.front(), buffer.size());
        boost::iostreams::stream<Device> stream(source);
        boost::archive::binary_iarchive archive(stream);
        // #endif

        for (typename Region<DIM>::Iterator i = region.begin(); i != region.end(); ++i) {
            archive & (*this)[*i];
        }
    }
#endif

    void saveMemberImplementation(
        char *target,
        MemoryLocation::Location targetLocation,
        const Selector<CELL_TYPE>& selector,
        const Region<DIM>& region) const
    {
        for (typename Region<DIM>::StreakIterator i = region.beginStreak(); i != region.endStreak(); ++i) {
            selector.copyMemberOut(
                &(*this)[i->origin],
                MemoryLocation::HOST,
                target,
                targetLocation,
                std::size_t(i->length()));
            target += selector.sizeOfExternal() * i->length();
        }
    }

    void loadMemberImplementation(
        const char *source,
        MemoryLocation::Location sourceLocation,
        const Selector<CELL_TYPE>& selector,
        const Region<DIM>& region)
    {
        for (typename Region<DIM>::StreakIterator i = region.beginStreak(); i != region.endStreak(); ++i) {
            selector.copyMemberIn(
                source,
                sourceLocation,
                &(*this)[i->origin],
                MemoryLocation::HOST,
                std::size_t(i->length()));
            source += selector.sizeOfExternal() * i->length();
        }
    }

private:
    Coord<DIM> dimensions;
    CellVector cellVector;
    CELL_TYPE edgeCell;
};

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

template<typename _CharT, typename _Traits, typename _CellT, typename _TopologyT>
std::basic_ostream<_CharT, _Traits>&
operator<<(std::basic_ostream<_CharT, _Traits>& __os,
           const Grid<_CellT, _TopologyT>& grid)
{
    __os << grid.toString();
    return __os;
}

}

#endif
