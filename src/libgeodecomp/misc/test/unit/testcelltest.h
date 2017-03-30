#include <cxxtest/TestSuite.h>
#include <sstream>
#include <libgeodecomp/misc/testcell.h>
#include <libgeodecomp/misc/testhelper.h>
#include <libgeodecomp/storage/grid.h>
#include <libgeodecomp/storage/updatefunctor.h>

using namespace LibGeoDecomp;

namespace LibGeoDecomp {

class TestCellTest : public CxxTest::TestSuite
{
private:
    typedef TestCell<2,
                     Stencils::Moore<2, 1>,
                     Topologies::Cube<2>::Topology,
                     TestCellHelpers::EmptyAPI,
                     TestCellHelpers::NoOutput> TestCellType;
    static const unsigned NANO_STEPS = APITraits::SelectNanoSteps<TestCellType>::VALUE;
    Grid<TestCellType> grid;
    int width;
    int height;

public:
    void setUp()
    {
        width = 4;
        height = 3;
        grid = Grid<TestCellType >(Coord<2>(width, height));
        grid[Coord<2>(-1, -1)] = TestCellType(Coord<2>(-1, -1),
                                          Coord<2>(width, height));
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                grid[Coord<2>(x, y)] =
                    TestCellType(Coord<2>(x, y), Coord<2>(width, height), 0);
            }
        }
    }

    void tearDown()
    {
    }

    void testDefaultConstructor()
    {
        TS_ASSERT(!TestCellType().valid());
    }

    void testSetUp()
    {
        TS_ASSERT_TEST_GRID(Grid<TestCellType>, grid, 0);
    }

    void testUpdate()
    {
        TS_ASSERT_TEST_GRID(Grid<TestCellType>, grid, 0);
        update();
        TS_ASSERT_TEST_GRID(Grid<TestCellType>, grid, 1);
    }

    void testMultipleUpdate()
    {
        for (unsigned i = 0; i < 100; i++) {
            update(i % NANO_STEPS);
            TS_ASSERT_TEST_GRID(Grid<TestCellType >, grid, i + 1);
        }
    }

    void testUpdateUnsyncedNanoStep()
    {
        update(3);
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateOutOfBoundsNanoStep()
    {
        update(100);
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateBadEdgeCellInner()
    {
        grid[Coord<2>(1, 1)] = TestCellType(Coord<2>(-1, -1), Coord<2>(width, height));
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateBadEdgeCellOuter()
    {
        grid[Coord<2>(-1, -1)] =
            TestCellType(Coord<2>(1, 1), Coord<2>(width, height));
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateInvalidNeighbor()
    {
        grid[Coord<2>(1, 0)] = TestCellType();
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
        TS_ASSERT(!grid[Coord<2>(1, 0)].isValid);
        TS_ASSERT(!grid[Coord<2>(2, 0)].isValid);
        TS_ASSERT(!grid[Coord<2>(0, 1)].isValid);
        TS_ASSERT(!grid[Coord<2>(1, 1)].isValid);
        TS_ASSERT(!grid[Coord<2>(2, 1)].isValid);
    }

    void testUpdateBadCycle()
    {
        grid[Coord<2>()].cycleCounter = 1;
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateBadRect()
    {
        grid[Coord<2>()] = TestCellType(Coord<2>(0, 0), Coord<2>(123, 456));
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    void testUpdateBadPos()
    {
        grid[Coord<2>()].pos = Coord<2>(1, 0);
        update();
        TS_ASSERT(!grid[Coord<2>()].isValid);
    }

    typedef TestCell<
        3,
        Stencils::Moore<3, 1>,
        Topologies::Torus<3>::Topology,
        TestCellHelpers::EmptyAPI,
        TestCellHelpers::NoOutput> TestCell3D;
    typedef Grid<TestCell3D, APITraits::SelectTopology<TestCell3D>::Value> Grid3D;

    void test3D1()
    {
        int width = 4;
        int height = 7;
        int depth = 5;
        Coord<3> dim(width, height, depth);

        Grid3D gridA(dim);
        Grid3D gridB(dim);
        gridA.getEdgeCell() =
            TestCell3D(Coord<3>::diagonal(-1), dim);
        gridA.getEdgeCell().isEdgeCell = true;

        gridB.getEdgeCell() =
            gridA.getEdgeCell();

        for (int z = 0; z < dim.z(); ++z) {
            for (int y = 0; y < dim.y(); y++) {
                for (int x = 0; x < dim.x(); x++) {
                    Coord<3> pos(x, y, z);
                    gridA[pos] = TestCell3D(pos, dim, 0);
                }
            }
        }
        TS_ASSERT_TEST_GRID(Grid3D, gridA, 0);

        for (int z = 0; z < dim.z(); ++z) {
            for (int y = 0; y < dim.y(); y++) {
                for (int x = 0; x < dim.x(); x++) {
                    Coord<3> pos(x, y, z);
                    gridB[pos].update(gridA.getNeighborhood(pos), 0);
                }
            }
        }
        TS_ASSERT_TEST_GRID(Grid3D, gridB, 1);

        for (int z = 0; z < dim.z(); ++z) {
            for (int y = 0; y < dim.y(); y++) {
                for (int x = 0; x < dim.x(); x++) {
                    Coord<3> pos(x, y, z);
                    gridA[pos].update(gridB.getNeighborhood(pos), 1);
                }
            }
        }
        TS_ASSERT_TEST_GRID(Grid3D, gridA, 2);
    }

    void test3D2()
    {
        int width = 4;
        int height = 7;
        int depth = 9;
        Coord<3> dim(width, height, depth);

        Grid3D gridA(dim);
        Grid3D gridB(dim);
        gridA.getEdgeCell() =
            TestCell3D(Coord<3>::diagonal(-1), dim);
        gridA.getEdgeCell().isEdgeCell = true;
        gridB.getEdgeCell() =
            gridA.getEdgeCell();

        for (int z = 0; z < dim.z(); ++z) {
            for (int y = 0; y < dim.y(); y++) {
                for (int x = 0; x < dim.x(); x++) {
                    Coord<3> pos(x, y, z);
                    gridA[pos] = TestCell3D(pos, dim, 0);
                }
            }
        }

        TS_ASSERT_TEST_GRID(Grid3D, gridA, 0);

        // use a wrong z-value to freak out the update step
        int badZ = 2;
        int actualZ = 5;
        for (int y = 0; y < dim.y(); y++)
            for (int x = 0; x < dim.x(); x++)
                gridA[Coord<3>(x, y, actualZ)] =
                    TestCell3D(Coord<3>(x, y, badZ), dim, 0);

        for (int z = 0; z < dim.z(); ++z) {
            for (int y = 0; y < dim.y(); y++) {
                for (int x = 0; x < dim.x(); x++) {
                    Coord<3> pos(x, y, z);
                    gridB[pos].update(gridA.getNeighborhood(pos), 0);
                }
            }
        }

        // verify that cells surrounding the rougue cells have been
        // invalidated:
        CoordBox<3> box(Coord<3>(0, 0, actualZ - 1),
                        Coord<3>(dim.x(), dim.y(), 3));
        for (CoordBox<3>::Iterator i = box.begin(); i != box.end(); ++i) {
            TS_ASSERT(!gridB[*i].valid());
        }
    }

    class AdditionalAPI :
        public APITraits::HasUpdateLineX,
        public APITraits::HasFixedCoordsOnlyUpdate
    {};

    void test3DwithUpdateLineXandWithoutSoA()
    {
        typedef TestCell<
            3,
            Stencils::Moore<3, 1>,
            Topologies::Torus<3>::Topology,
            AdditionalAPI> TestCellType;
        typedef Grid<TestCellType, Topologies::Torus<3>::Topology> GridType;

        Coord<3> dim(20, 10, 5);
        CoordBox<3> box(Coord<3>(), dim);
        Region<3> region;
        region << box;

        GridType gridA(dim);
        gridA.getEdgeCell() =
            TestCellType(Coord<3>::diagonal(-1), dim);
        gridA.getEdgeCell().isEdgeCell = true;
        for (CoordBox<3>::Iterator i =  box.begin(); i != box.end(); ++i) {
            gridA[*i] = TestCellType(*i, dim, 0);
        }

        GridType gridB(dim);
        gridB.getEdgeCell() = gridA.getEdgeCell();

        TS_ASSERT_TEST_GRID(GridType, gridA, 0);

        UpdateFunctor<TestCellType>()(region, Coord<3>(), Coord<3>(), gridA, &gridB, 0);

        TS_ASSERT_TEST_GRID(GridType, gridA, 0);
        TS_ASSERT_TEST_GRID(GridType, gridB, 1);
    }

    void update(unsigned nanoStep = 0)
    {
        Grid<TestCellType > newGrid(Coord<2>(width, height));
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                CoordMap<TestCellType > map(Coord<2>(x, y), &grid);
                newGrid[Coord<2>(x, y)].update(map, nanoStep);
            }
        }
        newGrid[Coord<2>(-1, -1)] = grid[Coord<2>(-1, -1)];
        grid = newGrid;
    }

};

}
