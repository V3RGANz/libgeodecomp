#ifndef LIBGEODECOMP_IO_UNSTRUCTUREDTESTINITIALIZER_H
#define LIBGEODECOMP_IO_UNSTRUCTUREDTESTINITIALIZER_H

#include <libgeodecomp/io/initializer.h>
#include <libgeodecomp/misc/sharedptr.h>
#include <libgeodecomp/misc/unstructuredtestcell.h>

namespace LibGeoDecomp {

/**
 * Helper class which sets up grids for use with various variants of
 * TestCell.
 */
template<class TEST_CELL = UnstructuredTestCell<> >
class UnstructuredTestInitializer : public Initializer<TEST_CELL>
{
public:
    using Initializer<TEST_CELL>::NANO_STEPS;

    UnstructuredTestInitializer(
        int dim,
        unsigned maxSteps,
        unsigned startStep = 0,
        int maxNeighbors = 20) :
        dim(dim),
        lastStep(maxSteps),
        firstStep(startStep),
        maxNeighbors(maxNeighbors)
    {}

    virtual void grid(GridBase<TEST_CELL, 1> *ret)
    {
        int cycle = NANO_STEPS * firstStep;
        Region<1> boundingRegion = ret->boundingRegion();
        typename GridBase<TEST_CELL, 1>::SparseMatrix weights;

        for (Region<1>::Iterator i = boundingRegion.begin(); i != boundingRegion.end(); ++i) {
            TEST_CELL cell(i->x(), cycle, true);

            int startNeighbors = i->x() + 1;
            int numNeighbors   = i->x() % maxNeighbors + 1;
            int endNeighbors   = startNeighbors + numNeighbors;

            // we need to insert ID/weight pairs here so can retrieve them sorted by ID below:
            std::map<int, double> weightsReorderBuffer;

            for (int j = startNeighbors; j != endNeighbors; ++j) {
                int actualNeighbor = j % dim;
                double edgeWeight = actualNeighbor + 0.1;

                weightsReorderBuffer[actualNeighbor] = edgeWeight;
                weights << std::make_pair(Coord<2>(i->x(), actualNeighbor), edgeWeight);
            }

            for (std::map<int, double>::iterator j = weightsReorderBuffer.begin(); j != weightsReorderBuffer.end(); ++j) {
                cell.expectedNeighborWeights << j->second;
            }

            ret->set(*i, cell);
        }

        ret->setWeights(0, weights);

        ret->setEdge(TEST_CELL(-1, 0, true, true));
    }

    Coord<1> gridDimensions() const
    {
        return Coord<1>(dim);
    }

    unsigned maxSteps() const
    {
        return lastStep;
    }

    unsigned startStep() const
    {
        return firstStep;
    }

    SharedPtr<Adjacency>::Type getAdjacency(const Region<1>& region) const
    {
        SharedPtr<Adjacency>::Type ret(new RegionBasedAdjacency());

        for (Region<1>::Iterator i = region.begin(); i != region.end(); ++i) {
            int startNeighbors = i->x() + 1;
            int numNeighbors   = i->x() % maxNeighbors + 1;
            int endNeighbors   = startNeighbors + numNeighbors;

            for (int j = startNeighbors; j != endNeighbors; ++j) {
                int actualNeighbor = j % dim;
                ret->insert(i->x(), actualNeighbor);
            }
        }

        return ret;
    }

    SharedPtr<Adjacency>::Type getReverseAdjacency(const Region<1>& region) const
    {
        SharedPtr<Adjacency>::Type ret(new RegionBasedAdjacency());

        for (Region<1>::Iterator i = region.begin(); i != region.end(); ++i) {
            int startNeighbors = i->x() - maxNeighbors;
            int endNeighbors   = i->x();

            for (int j = startNeighbors; j != endNeighbors; ++j) {
                int actualNeighbor = (j + dim) % dim;
                int numNeighbors = actualNeighbor % maxNeighbors + 1;

                if (i->x() <= (actualNeighbor + numNeighbors)) {
                    ret->insert(i->x(), actualNeighbor);
                }
            }
        }

        return ret;
    }

private:
    int dim;
    unsigned lastStep;
    unsigned firstStep;
    int maxNeighbors;
};

}

#endif
