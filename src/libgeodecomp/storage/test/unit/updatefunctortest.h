// Kill some warnings in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 4710 4711 )
#endif

#include <cxxtest/TestSuite.h>
#include <sstream>
#include <vector>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

#include <libgeodecomp/misc/apitraits.h>
#include <libgeodecomp/misc/testhelper.h>
#include <libgeodecomp/storage/grid.h>
#include <libgeodecomp/storage/soagrid.h>
#include <libgeodecomp/storage/updatefunctor.h>
#include <libgeodecomp/storage/updatefunctortestbase.h>

using namespace LibGeoDecomp;

std::stringstream myLog;

class BasicCell
{
public:
    class API :
        public APITraits::HasTorusTopology<2>
    {};

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
        myLog << "BasicCell::update(nanoStep = " << nanoStep << ")\n";
    }
};

class LineUpdateCell
{
public:
    class API :
        public APITraits::HasUpdateLineX,
        public APITraits::HasTorusTopology<2>
    {};

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
        myLog << "LineUpdateCell::update(nanoStep = " << nanoStep << ")\n";
    }

    // won't ever be called as all current update functors support
    // updateLineX only with fixed neighborhoods
    template<typename NEIGHBORHOOD>
    static void updateLineX(LineUpdateCell *target, long *x, long endX, const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
        myLog << "LineUpdateCell::updateLineX(x = " << *x << ", endX = " << endX << ", nanoStep = " << nanoStep << ")\n";
        *x = endX;
    }
};

class FixedCell
{
public:
    class API :
        public APITraits::HasFixedCoordsOnlyUpdate,
        public APITraits::HasTorusTopology<2>
    {};

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
        myLog << "FixedCell::update(nanoStep = " << nanoStep << ")\n";
    }

};

class FixedLineUpdateCell
{
public:
    class API :
        public APITraits::HasFixedCoordsOnlyUpdate,
        public APITraits::HasUpdateLineX,
        public APITraits::HasTorusTopology<2>
    {};

    template<typename NEIGHBORHOOD>
    static void updateLineX(FixedLineUpdateCell *target, long *x, long endX, const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
        myLog << "FixedLineUpdateCell::updateLine(x = " << *x << ", endX = " << endX << ", nanoStep = " << nanoStep << ")\n";
        *x = endX;
    }
};

class MySoATestCellWithDoubleAndBool
{
public:
    class API :
        public APITraits::HasFixedCoordsOnlyUpdate,
        public APITraits::HasSoA,
        public APITraits::HasUpdateLineX,
        public APITraits::HasStencil<Stencils::Moore<3, 1> >,
        public APITraits::HasTorusTopology<3>,
        public LibFlatArray::api_traits::has_asymmetric_dual_callback,
        public LibFlatArray::api_traits::has_default_3d_sizes_uniform
    {};

    explicit MySoATestCellWithDoubleAndBool(
        double temp = 0.0,
        bool alive = false) :
        temp(temp),
        alive(alive)
    {}

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, unsigned nanoStep)
    {
    }

    template<typename ACCESSOR1, typename ACCESSOR2>
    static void updateLineX(
        ACCESSOR1& hoodOld, int indexEnd,
        ACCESSOR2& hoodNew, unsigned nanoStep)
    {
        for (; hoodOld.index() < indexEnd; ++hoodOld.index(), ++hoodNew.index()) {
            hoodNew.temp()  = hoodOld[FixedCoord<0, 0, 0>()].temp();
            hoodNew.alive() = hoodOld[FixedCoord<0, 0, 0>()].alive();
        }
    }

    double temp;
    bool alive;
};

LIBFLATARRAY_REGISTER_SOA(MySoATestCellWithDoubleAndBool, ((double)(temp))((bool)(alive)))

namespace LibGeoDecomp {

template<class STENCIL>
class UpdateFunctorTestHelper : public UpdateFunctorTestBase<STENCIL>
{
public:
    using UpdateFunctorTestBase<STENCIL>::DIM;
    typedef typename UpdateFunctorTestBase<STENCIL>::TestCellType TestCellType;
    typedef typename UpdateFunctorTestBase<STENCIL>::GridType GridType;
    typedef STENCIL Stencil;

    virtual void callFunctor(
        const Region<DIM>& region,
        const GridType& gridOld,
        GridType *gridNew,
        unsigned nanoStep)
    {
        UpdateFunctor<TestCellType>()(
            region, Coord<DIM>(), Coord<DIM>(), gridOld, gridNew, nanoStep);
    }
};


class UpdateFunctorTest : public CxxTest::TestSuite
{
public:
    void testSelector()
    {
        checkSelector<BasicCell>(
            "BasicCell::update(nanoStep = 0)\n", 8);
        checkSelector<LineUpdateCell>(
            "LineUpdateCell::update(nanoStep = 0)\n", 8);
        checkSelector<FixedCell>(
            "FixedCell::update(nanoStep = 0)\n", 8);
        checkSelector<FixedLineUpdateCell>(
            "FixedLineUpdateCell::updateLine(x = 0, endX = 1, nanoStep = 0)\nFixedLineUpdateCell::updateLine(x = 1, endX = 7, nanoStep = 0)\nFixedLineUpdateCell::updateLine(x = 7, endX = 8, nanoStep = 0)\n", 1);
    }

    void testMoore2D()
    {
        UpdateFunctorTestHelper<Stencils::Moore<2, 1> >().testSimple(3);
        UpdateFunctorTestHelper<Stencils::Moore<2, 1> >().testSplittedTraversal(3);
    }

    void testMoore3D()
    {
        UpdateFunctorTestHelper<Stencils::Moore<3, 1> >().testSimple(3);
        UpdateFunctorTestHelper<Stencils::Moore<3, 1> >().testSplittedTraversal(3);
    }

    void testVonNeumann2D()
    {
        UpdateFunctorTestHelper<Stencils::VonNeumann<2, 1> >().testSimple(3);
        UpdateFunctorTestHelper<Stencils::VonNeumann<2, 1> >().testSplittedTraversal(3);
    }

    void testVonNeumann3D()
    {
        UpdateFunctorTestHelper<Stencils::VonNeumann<3, 1> >().testSimple(3);
        UpdateFunctorTestHelper<Stencils::VonNeumann<3, 1> >().testSplittedTraversal(3);
    }

    void testStructOfArraysBasic()
    {
        CoordBox<3> box1(Coord<3>(0,  0,  0),  Coord<3>(30, 20, 10));
        CoordBox<3> box2(Coord<3>(50, 20, 50), Coord<3>(50, 10, 10));
        typedef APITraits::SelectTopology<MySoATestCellWithDoubleAndBool>::Value Topology;

        SoAGrid<MySoATestCellWithDoubleAndBool, Topology> gridOld(box1, MySoATestCellWithDoubleAndBool(47), MySoATestCellWithDoubleAndBool(1));
        SoAGrid<MySoATestCellWithDoubleAndBool, Topology> gridNew(box2, MySoATestCellWithDoubleAndBool(11), MySoATestCellWithDoubleAndBool(0));

        for (int i = 5; i < 27; ++i) {
            Coord<3> c(i, 2, 1);
            gridOld.set(c, MySoATestCellWithDoubleAndBool(i - 5));
            TS_ASSERT_EQUALS(gridOld.get(c).temp,  i - 5);
        }
        for (int i = 57; i < 79; ++i) {
            Coord<3> c(i, 27, 56);
            TS_ASSERT_EQUALS(gridNew.get(c).temp,  11.0);
        }

        Streak<3> streak(Coord<3>(4, 1, 0), 26);
        Coord<3> sourceOffset(1, 1, 1);
        Coord<3> targetOffset(53, 26, 56);
        Region<3> region;
        region << streak;
        UpdateFunctor<MySoATestCellWithDoubleAndBool>()(region, sourceOffset, targetOffset, gridOld, &gridNew, 0);

        for (int i = 57; i < 79; ++i) {
            Coord<3> c(i, 27, 56);
            TS_ASSERT_EQUALS(gridNew.get(c).temp,  i - 57);
        }
    }

    void testStructOfArraysTestCell()
    {
        using std::swap;
        typedef TestCellSoA TestCellType;
        typedef SoAGrid<TestCellType, Topologies::Cube<3>::Topology> GridType;

        int timeSteps = 5;
        unsigned nanoSteps = TestCellType::NANO_STEPS;
        Coord<3> dim(10, 15, 5);
        CoordBox<3> box(Coord<3>(), dim);

        TestInitializer<TestCellType> init(dim);
        GridType gridA(box);
        init.grid(&gridA);
        GridType gridB = gridA;

        Region<3> region;
        region << gridA.boundingBox();

        GridType *gridOld = &gridA;
        GridType *gridNew = &gridB;


        for (int t = 0; t < timeSteps; ++t) {
            for (unsigned s = 0; s < nanoSteps; ++s) {
                UpdateFunctor<TestCellType>()(region, Coord<3>(), Coord<3>(), *gridOld, gridNew, s);
                unsigned cycle = (init.startStep() + t) * TestCellType::NANO_STEPS + s;

                TS_ASSERT_TEST_GRID2(GridType, *gridOld, cycle, );
                cycle += 1;
                TS_ASSERT_TEST_GRID2(GridType, *gridNew, cycle, );

                swap(gridOld, gridNew);
            }
        }
    }

private:
    template<typename CELL>
    void checkSelector(const std::string& line, int repeats)
    {
        Streak<2> streak(Coord<2>(2, 1), 10);
        Coord<2> dim(20, 10);
        unsigned nanoStep = 0;

        Grid<CELL> gridOld(dim);
        Grid<CELL> gridNew(dim);
        Region<2> region;
        region << streak;

        UpdateFunctor<CELL>()(region, Coord<2>(), Coord<2>(), gridOld, &gridNew, nanoStep);

        std::vector<char> message(1024 * 16, 0);
        myLog.read(&message[0], 1024 * 16);
        std::string expected = "";
        for (int i = 0; i < repeats; ++i) {
            expected += line;
        }

        TS_ASSERT_EQUALS(expected, std::string(&message[0]));
        myLog.clear();
    }
};

}
