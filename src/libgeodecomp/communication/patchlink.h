#ifndef LIBGEODECOMP_COMMUNICATION_PATCHLINK_H
#define LIBGEODECOMP_COMMUNICATION_PATCHLINK_H

#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_WITH_MPI

#include <libgeodecomp/communication/mpilayer.h>
#include <libgeodecomp/misc/limits.h>
#include <libgeodecomp/storage/patchaccepter.h>
#include <libgeodecomp/storage/patchprovider.h>
#include <libgeodecomp/storage/serializationbuffer.h>

// Kill warning 4514 in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

#include <deque>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

namespace LibGeoDecomp {

/**
 * PatchLink encapsulates the transmission of patches to and from
 * remote processes. PatchLink::Accepter takes the patches from a
 * Stepper hands them on to MPI, while PatchLink::Provider will receive
 * the patches from the net and provide then to a Stepper.
 */
template<class GRID_TYPE>
class PatchLink
{
public:
    friend class PatchLinkTest;

    typedef typename GRID_TYPE::CellType CellType;
    typedef typename SerializationBuffer<CellType>::BufferType BufferType;
    typedef typename SerializationBuffer<CellType>::FixedSize FixedSize;

    const static int DIM = GRID_TYPE::DIM;

    class Link
    {
    public:
        /**
         * MPI matches messages by communicator, rank and tag. To
         * avoid collisions if more than two patchlinks per node-pair
         * are present, the tag parameter needs to be unique (for this
         * pair).
         */
        inline Link(
            const Region<DIM>& region,
            int tag,
            MPI_Comm communicator = MPI_COMM_WORLD) :
            lastNanoStep(0),
            stride(1),
            mpiLayer(communicator),
            region(region),
            buffer(SerializationBuffer<CellType>::create(region)),
            tag(tag)
        {}

        virtual ~Link()
        {
            wait();
        }

        /**
         * Should be called prior to destruction to allow
         * implementations to perform any cleanup actions (e.g. to
         * post any receives to pending transmissions).
         */
        virtual void cleanup()
        {}

        virtual void charge(std::size_t next, std::size_t last, std::size_t newStride)
        {
            lastNanoStep = last;
            stride = newStride;
        }

        inline void wait()
        {
            mpiLayer.wait(tag);
        }

        inline void cancel()
        {
            mpiLayer.cancelAll();
        }

    protected:
        std::size_t lastNanoStep;
        long stride;
        MPILayer mpiLayer;
        Region<DIM> region;
        BufferType buffer;
        int tag;
    };

    class Accepter :
        public Link,
        public PatchAccepter<GRID_TYPE>
    {
    public:
        using Link::buffer;
        using Link::lastNanoStep;
        using Link::mpiLayer;
        using Link::region;
        using Link::stride;
        using Link::tag;
        using Link::wait;
        using PatchAccepter<GRID_TYPE>::checkNanoStepPut;
        using PatchAccepter<GRID_TYPE>::infinity;
        using PatchAccepter<GRID_TYPE>::pushRequest;
        using PatchAccepter<GRID_TYPE>::requestedNanoSteps;

        inline Accepter(
            const Region<DIM>& region,
            const int dest,
            const int tag,
            const MPI_Datatype& cellMPIDatatype,
            MPI_Comm communicator = MPI_COMM_WORLD) :
            Link(region, tag, communicator),
            dest(dest),
            cellMPIDatatype(cellMPIDatatype)
        {}

        virtual void charge(std::size_t next, std::size_t last, std::size_t newStride)
        {
            Link::charge(next, last, newStride);
            pushRequest(next);
        }

        virtual void put(
            const GRID_TYPE& grid,
            const Region<DIM>& /*validRegion*/,
            const Coord<DIM>& globalGridDimensions,
            const std::size_t nanoStep,
            const std::size_t rank)
        {
            if (!checkNanoStepPut(nanoStep)) {
                return;
            }

            wait();
            grid.saveRegion(&buffer, region);
            sendHeader(FixedSize());
            mpiLayer.send(&buffer[0], dest, buffer.size(), tag, cellMPIDatatype);

            std::size_t nextNanoStep = (min)(requestedNanoSteps) + stride;
            if ((lastNanoStep == infinity()) ||
                (nextNanoStep < lastNanoStep)) {
                requestedNanoSteps << nextNanoStep;
            }

            erase_min(requestedNanoSteps);
        }

    private:
        int dest;
        int dataSize;
        MPI_Datatype cellMPIDatatype;

        void sendHeader(APITraits::TrueType)
        {
            // we don't need any header for fixed size buffers
        }

        void sendHeader(APITraits::FalseType)
        {
            if (buffer.size() > std::size_t(Limits<int>::getMax())) {
                throw std::invalid_argument("buffer size exceeds std::numeric_limits<int>::max()");
            }

            dataSize = buffer.size();
            mpiLayer.send(&dataSize, dest, 1, tag, MPI_INT);
        }
    };

    class Provider :
        public Link,
        public PatchProvider<GRID_TYPE>
    {
    public:
        using Link::buffer;
        using Link::lastNanoStep;
        using Link::mpiLayer;
        using Link::region;
        using Link::stride;
        using Link::tag;
        using Link::wait;
        using PatchProvider<GRID_TYPE>::checkNanoStepGet;
        using PatchProvider<GRID_TYPE>::infinity;
        using PatchProvider<GRID_TYPE>::storedNanoSteps;
        using PatchProvider<GRID_TYPE>::get;

        inline
        Provider(
            const Region<DIM>& region,
            int source,
            int tag,
            const MPI_Datatype& cellMPIDatatype,
            MPI_Comm communicator = MPI_COMM_WORLD) :
            Link(region, tag, communicator),
            source(source),
            dataSize(0),
            cellMPIDatatype(cellMPIDatatype),
            transmissionInFlight(false)
        {}

        virtual void cleanup()
        {
            if (transmissionInFlight) {
                recvSecondPart(FixedSize());
            }
        }

        virtual void charge(const std::size_t next, const std::size_t last, const std::size_t newStride)
        {
            Link::charge(next, last, newStride);
            recv(next);
        }

        virtual void get(
            GRID_TYPE *grid,
            const Region<DIM>& patchableRegion,
            const Coord<DIM>& globalGridDimensions,
            const std::size_t nanoStep,
            const std::size_t rank,
            const bool remove = true)
        {
            if (storedNanoSteps.empty() || (nanoStep < (min)(storedNanoSteps))) {
                return;
            }

            checkNanoStepGet(nanoStep);
            wait();
            recvSecondPart(FixedSize());
            transmissionInFlight = false;

            grid->loadRegion(buffer, region);

            std::size_t nextNanoStep = (min)(storedNanoSteps) + stride;
            if ((lastNanoStep == infinity()) ||
                (nextNanoStep < lastNanoStep)) {
                recv(nextNanoStep);
            }

            erase_min(storedNanoSteps);
        }

        void recv(const std::size_t nanoStep)
        {
            storedNanoSteps << nanoStep;
            recvFirstPart(FixedSize());
            transmissionInFlight = true;
        }

    private:
        int source;
        int dataSize;
        MPI_Datatype cellMPIDatatype;
        bool transmissionInFlight;

        void recvFirstPart(APITraits::TrueType)
        {
            mpiLayer.recv(&buffer[0], source, buffer.size(), tag, cellMPIDatatype);
        }

        void recvFirstPart(APITraits::FalseType)
        {
            mpiLayer.recv(&dataSize, source, 1, tag, MPI_INT);
        }

        void recvSecondPart(APITraits::TrueType)
        {
            // no second receive neccessary for fixed size payloads
        }

        void recvSecondPart(APITraits::FalseType)
        {
            buffer.resize(dataSize);
            mpiLayer.recv(&buffer[0], source, dataSize, tag, cellMPIDatatype);
            wait();
        }
    };

};

}

#endif
#endif
