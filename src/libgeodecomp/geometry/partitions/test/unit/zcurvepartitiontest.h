#include <libgeodecomp/geometry/partitions/zcurvepartition.h>

#include <cxxtest/TestSuite.h>

using namespace LibGeoDecomp;

namespace LibGeoDecomp {

class ZCurvePartitionTest : public CxxTest::TestSuite
{
public:
    typedef std::vector<Coord<2> > CoordVector;

    void setUp()
    {
        partition = ZCurvePartition<2>(Coord<2>(10, 20), Coord<2>(4, 4));
        expected.clear();
        expected << Coord<2>(10, 20) << Coord<2>(11, 20) << Coord<2>(10, 21) << Coord<2>(11, 21)
                 << Coord<2>(12, 20) << Coord<2>(13, 20) << Coord<2>(12, 21) << Coord<2>(13, 21)
                 << Coord<2>(10, 22) << Coord<2>(11, 22) << Coord<2>(10, 23) << Coord<2>(11, 23)
                 << Coord<2>(12, 22) << Coord<2>(13, 22) << Coord<2>(12, 23) << Coord<2>(13, 23);
        actual.clear();
    }

    void testFillRectangles()
    {
        CoordVector actual;
        for (std::size_t i = 0; i < 16; ++i) {
            actual << *ZCurvePartition<2>::Iterator(Coord<2>(10, 20), Coord<2>(4, 4), i);
        }

        TS_ASSERT_EQUALS(actual, expected);
    }

    void testOperatorInc()
    {
        ZCurvePartition<2>::Iterator i(Coord<2>(10, 10), Coord<2>(4, 4), 10);
        TS_ASSERT_EQUALS(Coord<2>(10, 13), *i);
        ++i;
        TS_ASSERT_EQUALS(Coord<2>(11, 13), *i);
        ++i;
        TS_ASSERT_EQUALS(Coord<2>(12, 12), *i);
        ++i;
        TS_ASSERT_EQUALS(Coord<2>(13, 12), *i);
        ++i;
        TS_ASSERT_EQUALS(Coord<2>(12, 13), *i);
        ++i;
        TS_ASSERT_EQUALS(Coord<2>(13, 13), *i);
    }

    void testLoop()
    {
        CoordVector actual;
        for (ZCurvePartition<2>::Iterator i = partition.begin(); i != partition.end(); ++i)
            actual.push_back(*i);
        TS_ASSERT_EQUALS(actual, expected);
    }

    void testAsymmetric()
    {
        partition = ZCurvePartition<2>(Coord<2>(10, 20), Coord<2>(5, 3));
        // 01234
        // 569ab
        // 78cde
        expected.clear();
        expected << Coord<2>(10, 20) << Coord<2>(11, 20) << Coord<2>(12, 20) << Coord<2>(13, 20) << Coord<2>(14, 20)
                 << Coord<2>(10, 21) << Coord<2>(11, 21)
                 << Coord<2>(10, 22) << Coord<2>(11, 22)
                 << Coord<2>(12, 21) << Coord<2>(13, 21) << Coord<2>(14, 21)
                 << Coord<2>(12, 22) << Coord<2>(13, 22) << Coord<2>(14, 22);

        for (ZCurvePartition<2>::Iterator i = partition.begin(); i != partition.end(); ++i) {
            actual.push_back(*i);
        }

        TS_ASSERT_EQUALS(expected, actual);
    }

    void testSquareBracketsOperatorVersusIteration()
    {
        Coord<2> offset(10, 20);
        Coord<2> dimensions(6, 35);
        partition = ZCurvePartition<2>(offset, dimensions);
        CoordVector expected;

        for (std::size_t i = 0; i < static_cast<std::size_t>(dimensions.prod()); ++i) {
            expected << *partition[i];
        }

        CoordVector actual;

        for (ZCurvePartition<2>::Iterator i = partition.begin(); i != partition.end(); ++i) {
            actual << *i;
        }

        TS_ASSERT_EQUALS(expected, actual);
    }

    void testLarge()
    {
        partition = ZCurvePartition<2>(Coord<2>(10, 20), Coord<2>(600, 3500));
        CoordVector expectedSorted;

        for (int x = 10; x < 610; ++x) {
            for (int y = 20; y < 3520; ++y) {
                expectedSorted << Coord<2>(x, y);
            }
        }

        sort(expectedSorted);
        CoordVector actual;
        for (ZCurvePartition<2>::Iterator i = partition.begin(); i != partition.end(); ++i) {
            actual.push_back(*i);
        }

        sort(actual);
        TS_ASSERT_EQUALS(expectedSorted, actual);
    }

    void test3dSimple()
    {
        ZCurvePartition<3> partition(Coord<3>(1, 2, 3), Coord<3>(2, 2, 2));

        std::vector<Coord<3> > actual1;
        for (std::size_t i = 0; i < 8; ++i) {
            actual1 << *partition[i];
        }

        std::vector<Coord<3> > actual2;
        for (ZCurvePartition<3>::Iterator i = partition.begin();
             i != partition.end();
             ++i) {
            actual2 << *i;
        }

        std::vector<Coord<3> > expected;
        expected << Coord<3>(1, 2, 3)
                 << Coord<3>(2, 2, 3)
                 << Coord<3>(1, 3, 3)
                 << Coord<3>(2, 3, 3)
                 << Coord<3>(1, 2, 4)
                 << Coord<3>(2, 2, 4)
                 << Coord<3>(1, 3, 4)
                 << Coord<3>(2, 3, 4);

        TS_ASSERT_EQUALS(actual1, expected);
        TS_ASSERT_EQUALS(actual2, expected);
    }

    void largeTest(Coord<3> dimensions)
    {
        Coord<3> offset(10, 20, 30);
        ZCurvePartition<3> partition(offset, dimensions);

        std::vector<Coord<3> > actual1;
        std::vector<Coord<3> > actual2;
        std::vector<Coord<3> > expected;

        CoordBox<3> box(offset, dimensions);
        for (CoordBox<3>::Iterator i = box.begin(); i != box.end(); ++i) {
            expected << *i;
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(dimensions.prod()); ++i) {
            actual1 << *partition[i];
        }

        for (ZCurvePartition<3>::Iterator i = partition.begin();
             i != partition.end();
             ++i) {
            actual2 << *i;
        }

        sort(actual1);
        sort(actual2);
        sort(expected);

        TS_ASSERT_EQUALS(actual1, expected);
        TS_ASSERT_EQUALS(actual2, expected);
    }

    void test3dLarge2()
    {
        largeTest(Coord<3>(5, 7, 20));
        largeTest(Coord<3>(5, 40, 4));
        largeTest(Coord<3>(50, 8, 8));
    }


private:
    ZCurvePartition<2> partition;
    CoordVector expected, actual;
};

}
