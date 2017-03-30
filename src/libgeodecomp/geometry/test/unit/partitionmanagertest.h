#include <libgeodecomp/geometry/partitionmanager.h>
#include <libgeodecomp/geometry/partitions/recursivebisectionpartition.h>
#include <libgeodecomp/geometry/partitions/stripingpartition.h>

using namespace LibGeoDecomp;

namespace LibGeoDecomp {

class PartitionManagerTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        dimensions = Coord<2>(20, 20);
        offset = 10 * 20 + 8;
        weights.clear();
        weights << std::size_t(15)
                << std::size_t(12)
                << std::size_t(1)
                << std::size_t(32)
                << std::size_t(25)
                << std::size_t(40)
                << std::size_t(67);
        /**
         * the grid should look like this: (line no. at beginning of
         * the rows, n means that the corresponding cell belongs to
         * node n)
         *
         * 08: --------------------
         * 09: --------------------
         * 10: --------000000000000
         * 11: 00011111111111123333
         * 12: 33333333333333333333
         * 13: 33333333444444444444
         * 14: 44444444444445555555
         * 15: 55555555555555555555
         * 16: 55555555555556666666
         * 17: 66666666666666666666
         * 18: 66666666666666666666
         * 19: 66666666666666666666
         *
         */
        rank = 4;
        ghostZoneWidth = 3;
        /**
         *
         * the outer ghost zone of node 4 and width 3. "." is its
         * inner set, n means the corresponding cell has distance n to
         * the original region.
         *
         * 08: --------------------
         * 09: --------------------
         * 10: -----333333333333333
         * 11: 33333322222222222222
         * 12: 22222221111111111111
         * 13: 11111111............
         * 14: .............1111111
         * 15: 11111111111111222222
         * 16: 22222222222222233333
         * 17: 3333333333333333----
         * 18: --------------------
         * 19: --------------------
         *
         */
        partition.reset(new StripingPartition<2>(Coord<2>(), dimensions, offset, weights));
        boundingBoxes =
            fakeBoundingBoxes(
                offset,
                weights.size(),
                ghostZoneWidth,
                weights,
                partition);
        expandedBoundingBoxes =
            fakeExpandedBoundingBoxes(
                offset,
                weights.size(),
                ghostZoneWidth,
                weights,
                partition);

        SharedPtr<AdjacencyManufacturer<2> >::Type dummyAdjacencyManufacturer(new DummyAdjacencyManufacturer<2>);

        partitionManager.resetRegions(
            dummyAdjacencyManufacturer,
            CoordBox<2>(Coord<2>(), dimensions),
            partition,
            rank,
            ghostZoneWidth);
        partitionManager.resetGhostZones(boundingBoxes, expandedBoundingBoxes);
    }

    void testResetRegionsAndGhostRegionFragments()
    {
        unsigned curOffset = offset;
        for (int i = 0; i < 7; ++i) {
            // 23 because you have to intersect node 6's region with
            // the outer ghost zone. this leaves a fragment of length 22.
            unsigned length = (i != 6)? weights[static_cast<std::size_t>(i)] : 23;
            // we're node 4 ourselves, so that non-existent halo can be skipped
            if (i != 4) {
                checkRegion(
                    partitionManager.getOuterGhostZoneFragments()[i].back(),
                    curOffset,
                    curOffset + length,
                    partition);
            }
            curOffset += weights[i];
        }
    }

    void testResetRegionsAndExtendedRegions()
    {
        for (unsigned i = 0; i <= ghostZoneWidth; ++i) {
            checkRegion(partitionManager.getRegion(3, i),
                        (11 - i) * 20 + 16 - i,
                        (13 + i) * 20 +  8 + i,
                        partition);
            checkRegion(partitionManager.getRegion(4, i),
                        (13 - i) * 20 +  8 - i,
                        (14 + i) * 20 + 13 + i,
                        partition);
            checkRegion(partitionManager.getRegion(5, i),
                        (14 - i) * 20 + 13 - i,
                        (16 + i) * 20 + 13 + i,
                        partition);
        }
    }

    void testResetRegionsAndOuterAndInnerRims()
    {
        checkRegion(
            partitionManager.rim(1),
            11 * 20 + 6,
            16 * 20 + 15,
            partition);
    }

    void testResetRegionsAndInnerSets()
    {
        TS_ASSERT(partitionManager.innerSet(0).empty());
        TS_ASSERT(partitionManager.innerSet(1).empty());
        TS_ASSERT(partitionManager.innerSet(2).empty());
        TS_ASSERT(partitionManager.innerSet(3).empty());
    }

    void testGetOuterRim()
    {
        Region<2> expected = partitionManager.ownRegion(ghostZoneWidth) -
            partitionManager.ownRegion();
        TS_ASSERT_EQUALS(expected, partitionManager.getOuterRim());
    }

    void test3DFirst()
    {
        unsigned ghostZoneWidth = 4;
        CoordBox<3> box(Coord<3>(), Coord<3>(55, 47, 31));

        std::vector<std::size_t> weights;
        weights << std::size_t(10000)
                << std::size_t(15000)
                << std::size_t(25000)
                << box.dimensions.prod() - sum(weights);
        SharedPtr<Partition<3> >::Type partition(
            new StripingPartition<3>(Coord<3>(), box.dimensions, 0, weights));

        SharedPtr<AdjacencyManufacturer<3> >::Type dummyAdjacencyManufacturer(new DummyAdjacencyManufacturer<3>);

        PartitionManager<Topologies::Torus<3>::Topology> partitionManager;
        partitionManager.resetRegions(
            dummyAdjacencyManufacturer,
            box,
            partition,
            0,
            ghostZoneWidth);

        std::vector<CoordBox<3> > boundingBoxes;
        std::vector<CoordBox<3> > expandedBoundingBoxes;
        for (int i = 0; i < 4; ++i) {
            boundingBoxes << partitionManager.getRegion(i, 0).boundingBox();
            expandedBoundingBoxes << partitionManager.getRegion(i, ghostZoneWidth).boundingBox();
        }

        partitionManager.resetGhostZones(boundingBoxes, expandedBoundingBoxes);

        Region<3> expected;
        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(0));
        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(1));
    }

    void test3DSecond()
    {
        unsigned ghostZoneWidth = 3;
        CoordBox<3> box(Coord<3>(), Coord<3>(55, 47, 55));

        std::vector<std::size_t> weights;
        weights << std::size_t(40000)
                << std::size_t(15000)
                << std::size_t(25000)
                << (box.dimensions.prod() - sum(weights));
        SharedPtr<Partition<3> >::Type partition(
            new StripingPartition<3>(Coord<3>(), box.dimensions, 0, weights));

        SharedPtr<AdjacencyManufacturer<3> >::Type dummyAdjacencyManufacturer(new DummyAdjacencyManufacturer<3>);

        PartitionManager<Topologies::Torus<3>::Topology> partitionManager;
        partitionManager.resetRegions(
            dummyAdjacencyManufacturer,
            box,
            partition,
            0,
            ghostZoneWidth);

        std::vector<CoordBox<3> > boundingBoxes;
        std::vector<CoordBox<3> > expandedBoundingBoxes;
        for (int i = 0; i < 4; ++i) {
            boundingBoxes << partitionManager.getRegion(i, 0).boundingBox();
            expandedBoundingBoxes << partitionManager.getRegion(i, ghostZoneWidth).boundingBox();
        }

        partitionManager.resetGhostZones(boundingBoxes, expandedBoundingBoxes);

        // index 0:
        Region<3> expected;
        for (int z = 0; z < 15; ++z) {
            for (int y = 0; y < 47; ++y) {
                expected << Streak<3>(Coord<3>(0, y, z), 55);
            }
        }
        for (int y = 0; y < 22; ++y) {
            expected << Streak<3>(Coord<3>(0, y, 15), 55);
        }
        expected << Streak<3>(Coord<3>(0, 22, 15), 15);

        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(0));

        // index 1:
        expected.clear();
        for (int z = 1; z < 14; ++z) {
            for (int y = 0; y < 47; ++y) {
                expected << Streak<3>(Coord<3>(0, y, z), 55);
            }
        }
        for (int y = 1; y < 21; ++y) {
            expected << Streak<3>(Coord<3>(0, y, 14), 55);
        }
        expected << Streak<3>(Coord<3>(1, 21, 14), 14);

        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(1));

        // index 2:
        expected.clear();
        for (int z = 2; z < 13; ++z) {
            for (int y = 0; y < 47; ++y) {
                expected << Streak<3>(Coord<3>(0, y, z), 55);
            }
        }
        for (int y = 2; y < 20; ++y) {
            expected << Streak<3>(Coord<3>(0, y, 13), 55);
        }
        expected << Streak<3>(Coord<3>(2, 20, 13), 13);

        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(2));

        // index 3:
        expected.clear();
        for (int z = 3; z < 12; ++z) {
            for (int y = 0; y < 47; ++y) {
                expected << Streak<3>(Coord<3>(0, y, z), 55);
            }
        }
        for (int y = 3; y < 19; ++y) {
            expected << Streak<3>(Coord<3>(0, y, 12), 55);
        }
        expected << Streak<3>(Coord<3>(3, 19, 12), 12);

        TS_ASSERT_EQUALS(expected, partitionManager.innerSet(3));

    }

private:
    Coord<2> dimensions;
    unsigned offset;
    SharedPtr<StripingPartition<2> >::Type partition;
    std::vector<std::size_t> weights;
    int rank;
    unsigned ghostZoneWidth;
    std::vector<CoordBox<2> > boundingBoxes;
    std::vector<CoordBox<2> > expandedBoundingBoxes;
    PartitionManager<Topologies::Cube<2>::Topology> partitionManager;

    std::vector<CoordBox<2> > fakeBoundingBoxes(
        unsigned offset,
        unsigned size,
        unsigned ghostZoneWidth,
        const std::vector<std::size_t>& weights,
        const SharedPtr<StripingPartition<2> >::Type& partition)
    {
        std::vector<CoordBox<2> > boundingBoxes(size);
        std::size_t startOffset = offset;
        std::size_t endOffset = offset;

        for (unsigned i = 0; i < size; ++i) {
            endOffset += weights[i];
            Region<2> s;

            for (StripingPartition<2>::Iterator coords = (*partition)[startOffset];
                 coords != (*partition)[endOffset];
                 ++coords) {
                s << *coords;
            }

            boundingBoxes[i] = s.boundingBox();
            startOffset = endOffset;
        }

        return boundingBoxes;
    }

    std::vector<CoordBox<2> > fakeExpandedBoundingBoxes(
        unsigned offset,
        unsigned size,
        unsigned ghostZoneWidth,
        const std::vector<std::size_t>& weights,
        const SharedPtr<StripingPartition<2> >::Type& partition)
    {
        std::vector<CoordBox<2> > boundingBoxes(size);
        std::size_t startOffset = offset;
        std::size_t endOffset = offset;

        for (unsigned i = 0; i < size; ++i) {
            endOffset += weights[i];
            Region<2> s;

            for (StripingPartition<2>::Iterator coords = (*partition)[startOffset];
                 coords != (*partition)[endOffset];
                 ++coords) {
                s << *coords;
            }

            s = s.expand(ghostZoneWidth);
            boundingBoxes[i] = s.boundingBox();
            startOffset = endOffset;
        }

        return boundingBoxes;
    }

    void checkRegion(
        const Region<2>& region,
        unsigned start,
        unsigned end,
        const SharedPtr<StripingPartition<2> >::Type& partition)
    {
        std::vector<Coord<2> > expected;
        std::vector<Coord<2> > actual;
        for (StripingPartition<2>::Iterator i = (*partition)[start];
             i != (*partition)[end];
             ++i) {
            expected << *i;
        }

        for (Region<2>::Iterator i = region.begin(); i != region.end(); ++i) {
            actual << *i;
        }

        TS_ASSERT_EQUALS(expected, actual);
    }
};

}
