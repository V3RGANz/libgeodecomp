#ifndef LIBGEODECOMP_GEOMETRY_PARTITIONS_PARTITION_H
#define LIBGEODECOMP_GEOMETRY_PARTITIONS_PARTITION_H

#include <libgeodecomp/geometry/adjacency.h>
#include <libgeodecomp/geometry/region.h>
#include <libgeodecomp/misc/sharedptr.h>
#include <libgeodecomp/misc/stdcontaineroverloads.h>

namespace LibGeoDecomp {

#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4710 4711 )
#endif

/**
 * The base class for all domain decomposition schemes. By having all
 * these schemes implement a common interface, we can easily swap
 * those out and test multiple so each application can use that scheme
 * which matches the requirements of hardware, model and data best.
 */
template<int DIM>
class Partition
{
public:
    typedef typename SharedPtr<Adjacency>::Type AdjacencyPtr;

    /**
     * initializes the partition so that the domain will be split up
     * in chucks with sizes proportional to the weights specified in
     * _weights. For most applications offset should be set to 0.
     * Also, _weights.sum() should equal simulationArea.size() (where
     * simulationArea is stored in PartitionManager). This basically
     * means that each simulation cell corresponds to a weight of 1.
     * Each entry in the weight vector will usually correspond to an
     * MPI process, identified by its rank.
     */
    inline Partition(
        const std::size_t offset,
        const std::vector<std::size_t>& weights) :
        weights(weights)
    {
        startOffsets.resize(weights.size() + 1);
        startOffsets[0] = offset;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            startOffsets[i + 1] = startOffsets[i] + weights[i];
        }
    }

    virtual ~Partition()
    {}

    inline const std::vector<std::size_t>& getWeights() const
    {
        return weights;
    }

    virtual Region<DIM> getRegion(const std::size_t node) const = 0;

protected:
    std::vector<std::size_t> weights;
    std::vector<std::size_t> startOffsets;
};

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

}

#endif
