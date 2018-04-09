#include <libgeodecomp.h>
using namespace LibGeoDecomp;

/**
 * A simple Adaptive Mesh Refinement (AMR) demo with automatic grid
 * adaptation. It computes a simple heat dissipation with fixed
 * heating/cooling points.
 *
 * We're using in-cell refinement, which means that all
 * finer grids (patches) are stored within cells of the coarsest
 * level. This implies that these cells do not all have the same
 * memory footprint. The code has been optimized for brevity and
 * legibility, not performance.
 */
template<
    int SUBLEVEL_DIM_X = 4,
    int SUBLEVEL_DIM_Y = SUBLEVEL_DIM_X,
    int SUBLEVEL_TILE_SIZE = (SUBLEVEL_DIM_X * SUBLEVEL_DIM_Y),
    int DELTA_MAX = 10>
class AMRDiffusionCell
{
public:
    typedef AMRDiffusionCell value_type;

    inline static int sublevelDimX()
    {
        return SUBLEVEL_DIM_X;
    }

    inline static int sublevelDimY()
    {
        return SUBLEVEL_DIM_Y;
    }

    // const static int PATCH_DIM_BITS = 3;
    // // sublevel patches (refined grids) are PATCH_DIM x PATCH_DIM in size.
    // const static int PATCH_DIM = 1 >< PATCH_DIM_BITS;
    // const static int PATCH_DIM_BITMASK = PATCH_DIM - 1;

    // const static int BASE_DIM_BITS = 20;
    // const static int BASE_DIM = 1 << BASE_DIM_BITS;
    // const static int BASE_DIM_BITMASK = BASE_DIM - 1;

    // fixme: move this to dedicated class/file
    template<typename NEIGHBORHOOD>
    class HoodAdapter
    {
    public:
        inline
        explicit HoodAdapter(const NEIGHBORHOOD& hood) :
            hood(hood)
        {}

        template<int X, int Y>
        const AMRDiffusionCell *operator()(FixedCoord<X, Y, 0>, const AMRDiffusionCell& origin) const
        {
            Coord<2> searchPoint = origin.logicalCoord +
                Coord<2>(X * origin.logicalOffset.x(),
                         Y * origin.logicalOffset.y());

            // std::cout << "looking for " << searchPoint << "\n";
            const AMRDiffusionCell *res = 0;

            if (origin.logicalCoord == Coord<2>(16, 0)) {
                // std::cout << "  looking for " << searchPoint << "\n"
                //           << "  FixedCoord<" << X << ", " << Y << ">\n";
            }

            // most accesses go to (0, 0)
            res = hood[FixedCoord<0, 0>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at A\n";
                return res;
            }

            res = hood[FixedCoord<-1, -1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at B\n";
                return res;
            }

            res = hood[FixedCoord< 0, -1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at C\n";
                return res;
            }

            res = hood[FixedCoord< 1, -1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at D\n";
                return res;
            }

            // same row:
            res = hood[FixedCoord<-1,  0>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at E\n";
                return res;
            }

            res = hood[FixedCoord< 1,  0>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at F\n";
                return res;
            }

            // lower row:
            res = hood[FixedCoord<-1,  1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at G\n";
                return res;
            }

            res = hood[FixedCoord< 0,  1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at H\n";
                return res;
            }

            res = hood[FixedCoord< 1,  1>()].lookup(searchPoint);
            if (res) {
                // if (origin.logicalCoord == Coord<2>(16, 0)) std::cout << "    found at I\n";
                return res;
            }

            // std::cout << "searchPoint: " << searchPoint << "\n"
            //           << "origin: " << origin.logicalCoord << ", " << origin.logicalOffset << "\n";
            throw std::logic_error("neighbor not found");
        }

    private:
        const NEIGHBORHOOD& hood;
    };

    // fixme: move this to dedicated class/file
    // fixme: ignore non-leaf nodes in size calculation and visitor pattern?
    class Iterator
    {
    public:
        explicit Iterator(const AMRDiffusionCell *cell) :
            cursors(1, cell)
        {}

        const AMRDiffusionCell& operator*() const
        {
            return *cursors.back();
        }

        void operator++()
        {
            // std::cout << "operator++1\n";
            if (cursors.back()->sublevel.size() != 0) {
                cursors.push_back(&cursors.back()->sublevel[0][0]);
                // std::cout << "operator++2b\n";
                return;
            }

            cursors.back()++;
            // we'll need to pop all cursors which have reached the
            // end of their superordinate level (but preserve the
            // top-most cursor as it may be part of the topmost layer)
            while ((cursors.size() > 1) &&
                   ((*(cursors.end() - 2))->sublevel[0].end() == cursors.back())) {
                cursors.pop_back();
                cursors.back()++;
            }
            // std::cout << "operator++2a\n";
        }

        inline bool operator==(const Iterator& other) const
        {
            return cursors.back() == other.cursors.back();
        }

        inline bool operator!=(const Iterator& other) const
        {
            return cursors.back() != other.cursors.back();
        }

        inline const AMRDiffusionCell* operator->() const
        {
            return cursors.back();
        }

    private:
        std::vector<const AMRDiffusionCell*> cursors;
    };

    typedef Iterator iterator;
    typedef Iterator const_iterator;

    // template<typename NEIGHBORHOOD>
    // class LookupHelper
    // {
    // public:
    //     LookupHelper(const NEIGHBORHOOD *hood) :
    //         hood(hood)
    //     {}

    //     inline
    //     const AMRDiffusionCell& operator()(const unsigned long x, const unsigned long y) const
    //     {
    //         Coord<2> relativeCoord;
    //         unsigned long baseCoordX = x & BASE_DIM_BITMASK;
    //         unsigned long baseCoordY = y & BASE_DIM_BITMASK;

    //         if (baseCoordX < centerX) {
    //             relativeCoord.x() = -1;
    //         } else {
    //             if (baseCoordX > centerX) {
    //                 relativeCoord.x() = 1;
    //             }
    //         }

    //         if (baseCoordY < centerY) {
    //             relativeCoord.y() = -1;
    //         } else {
    //             if (baseCoordY > centerY) {
    //                 relativeCoord.y() = 1;
    //             }
    //         }

    //         return (*this)((*hood)[relativeCoord], x >> BASE_DIM_BITS, y >> BASE_DIM_BITS);
    //     }

    //     inline
    //     const AMRDiffusionCell& operator()(const AMRDiffusionCell& cell, const unsigned long x, const unsigned long y) const
    //     {
    //         if (cell.dim == Coord<2>()) {
    //             return cell;
    //         }

    //         // Coord<2> relativeCoord(
    //         //     x & PATCH_DIM_BITMASK,
    //         //     y & PATCH_DIM_BITMASK);
    //         // return (*this)(cell[relativeCoord], x >> PATCH_DIM_BITS, y >> PATCH_DIM_BITS);
    //         // fixme
    //         return cell;
    //     }

    // private:
    //     const NEIGHBORHOOD *hood;
    //     unsigned long centerX;
    //     unsigned long centerY;
    // };


    inline
    explicit AMRDiffusionCell(
        const FloatCoord<2>& pos = FloatCoord<2>(0, 0),
        const FloatCoord<2>& dim = FloatCoord<2>(1, 1),
        const Coord<2>& logicalCoord = Coord<2>(),
        const Coord<2>& logicalOffset = Coord<2>(0, 0),
        const double value = 0,
        const double influx = 0,
        const int depth = 0,
        const int maxDepth = 0,
        const bool edgeCell = false,
        const double contagiousFlag = 0) :
        pos(pos),
        dim(dim),
        logicalCoord(logicalCoord),
        logicalOffset(logicalOffset),
        value(value),
        influx(influx),
        depth(depth),
        maxDepth(maxDepth),
        edgeCell(edgeCell),
        contagiousFlag(contagiousFlag)
    {}

    class API : public APITraits::HasUnstructuredGrid
    {};

    // fixme: use 2 nano steps to avoid refinement/coarseing to create gaps of 2 refinement levels

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, int)
    {
        *this = hood[FixedCoord<0, 0>()];
        HoodAdapter<NEIGHBORHOOD> adapter(hood);
        actualUpdate(adapter);
    }

    Iterator begin() const
    {
        return Iterator(this);
    }

    Iterator end() const
    {
        return Iterator(this + 1);
    }

    std::size_t size() const
    {
        std::size_t ret = 1;
        if (sublevel.size()) {
            for (const AMRDiffusionCell *i = sublevel[0].begin(); i != sublevel[0].end(); ++i) {
                ret += i->size();
            }
        }

        return ret;
    }

    FloatCoord<2> getPoint() const
    {
        return pos;
    }

    std::vector<FloatCoord<2> > getShape() const
    {
        std::vector<FloatCoord<2> > shape;
        shape << pos
              << pos + FloatCoord<2>(dim[0], 0)
              << pos + dim
              << pos + FloatCoord<2>(0, dim[1]);
        return shape;
    }

// fixme: private:

    std::vector<FixedArray<AMRDiffusionCell, SUBLEVEL_TILE_SIZE> > sublevel;
    FloatCoord<2> pos;
    FloatCoord<2> dim;
    Coord<2> logicalCoord;
    Coord<2> logicalOffset;
    double value;
    double influx;
    int depth;
    int maxDepth;
    bool edgeCell;
    double contagiousFlag;

#define HOOD(X, Y) hood(FixedCoord<X, Y>(), *this)

    template<typename NEIGHBORHOOD>
    void actualUpdate(const NEIGHBORHOOD& hood)
    {
        // fixme
        value =
            influx +
            0.25 *
            (HOOD( 0, -1)->value +
             HOOD(-1,  0)->value +
             HOOD( 1,  0)->value +
             HOOD( 0,  1)->value);

        // if (contagiousFlag == 1.0) {
        //     std::cout << "infection at " << logicalCoord << ", " << logicalOffset << "\n";
        // }

        // if (logicalCoord == Coord<2>(16, 0)) {
        //     const AMRDiffusionCell *c = HOOD(-1, 0);
        //     std::cout << "state at " << logicalCoord << " is " << contagiousFlag
        //               << ", left neighbor is " << c->logicalCoord
        //               << " of size " << c->logicalOffset
        //               << " with " << c->contagiousFlag
        //               << " and edge: " << c->edgeCell
        //               << "\n";
        // }

        contagiousFlag =
            (std::max)(contagiousFlag,
                       (std::max)(HOOD(0, -1)->contagiousFlag,
                                  (std::max)(HOOD(-1, 0)->contagiousFlag,
                                             (std::max)(HOOD(1, 0)->contagiousFlag,
                                                        HOOD(0, 1)->contagiousFlag))));

        if (sublevel.size()) {
            for (std::size_t i = 0; i < sublevel[0].size(); ++i) {
                sublevel[0][i].actualUpdate(hood);
            }
        } else {
            if (depth < maxDepth) {
                // check for refinement, fixme: extract into method
                double delta = 0;
                delta = (std::max)(delta, std::abs(value - HOOD(0, -1)->value));
                delta = (std::max)(delta, std::abs(value - HOOD(0,  1)->value));
                delta = (std::max)(delta, std::abs(value - HOOD(-1, 0)->value));
                delta = (std::max)(delta, std::abs(value - HOOD( 1, 0)->value));

                if (delta > DELTA_MAX) {
                    sublevel.resize(1);
                    FloatCoord<2> sublevelDim(
                        dim[0] / SUBLEVEL_DIM_X,
                        dim[1] / SUBLEVEL_DIM_Y);

                    Coord<2> newLogicalOffset(
                        logicalOffset.x() / SUBLEVEL_DIM_X,
                        logicalOffset.y() / SUBLEVEL_DIM_Y);

                    for (int y = 0; y < SUBLEVEL_DIM_Y; ++y) {
                        for (int x = 0; x < SUBLEVEL_DIM_X; ++x) {
                            Coord<2> index(x, y);
                            FloatCoord<2> newPos = pos + sublevelDim.scale(index);
                            Coord<2> newLogicalCoord = logicalCoord + index.scale(newLogicalOffset);

                            sublevel[0] << AMRDiffusionCell(
                                newPos,
                                sublevelDim,
                                newLogicalCoord,
                                newLogicalOffset,
                                value,
                                influx,
                                depth + 1,
                                maxDepth,
                                edgeCell,
                                contagiousFlag);
                        }
                    }
                }
            }
        }
    }

    const AMRDiffusionCell *lookup(const Coord<2>& searchPoint) const
    {
        Coord<2> oppositeSide = logicalCoord + logicalOffset;
        bool outside =
            (searchPoint.x() <  logicalCoord.x()) ||
            (searchPoint.x() >= oppositeSide.x()) ||
            (searchPoint.y() <  logicalCoord.y()) ||
            (searchPoint.y() >= oppositeSide.y());

        if (edgeCell && outside) {
            // std::cout << "  edgeCell, searchPoint : " << searchPoint  << "\n"
            //           << "  edgeCell, logicalCoord: " << logicalCoord << "\n"
            //           << "  edgeCell, oppositeSide: " << oppositeSide << "\n\n";
            return this;
        }

        if (edgeCell || outside) {
            // std::cout << "  outside(" << searchPoint << ")\n";
            return 0;
        }

        if (sublevel.size() == 0) {
            // std::cout << "  no more sublevels\n";
            return this;
        }

        for (std::size_t i = 0; i < sublevel[0].size(); ++i) {
            const AMRDiffusionCell *ret = sublevel[0][i].lookup(searchPoint);
            if (ret) {
                // std::cout << "  child hit\n";
                return ret;
            }
        }

        return 0;
    }

    // {
    //     double delta = 0;
    //     delta = (std::max)(std::abs(HOOD(0, 1) - HOOD( 0, -1)),
    //                      std::abs(HOOD(1, 0) - HOOD(-1,  0)));


    //     // do not descent if we're already on the finest level
    //     if (sublevel.dimensions == Coord<2>(0, 0)) {
    //         simpleUpdate(hood, delta);
    //     } else {
    //         hierarchicalUpdate(hood, delta);
    //     }

    // }

    // template<typename NEIGHBORHOOD>
    // void simpleUpdate(const NEIGHBORHOOD& hood, const double delta)
    // {
    //     // refine if delta of neighbors is too large
    //     if (delta > DELTA_MAX) {
    //         sublevel.resize(Coord<2>(PATCH_DIM, PATCH_DIM));
    //         FloatCoord newDim(dim.x() * (1.0 / PATCH_DIM),
    //                           dim.y() * (1.0 / PATCH_DIM));

    //         for (int y = 0; y < PATCH_DIM; ++y) {
    //             for (int x = 0; x < PATCH_DIM; ++x) {
    //                 sublevel[Coord<2>(x, y)] = AMRDiffusionCell(
    //                     pos + FloatCoord<2>(newDim.x() * x, newDim.y() * y),
    //                     newDim,
    //                     value,
    //                     depth + 1);
    //             }
    //         }

    //         return;
    //     }

    //     value =
    //         (HOOD( 0, -1).value +
    //          HOOD(-1,  0).value +
    //          HOOD( 1,  0).value +
    //          HOOD( 0,  1).value);
    // }

    // template<typename NEIGHBORHOOD>
    // void hierarchicalUpdate(const NEIGHBORHOOD& hood, const double delta)
    // {
    //     // coarsen if delta of neighbors is small enough
    //     if (delta <= DELTA_MAX) {
    //         value = gatherFromSublevel();
    //         sublevel.resize(Coord<2>(0, 0));

    //         return;
    //     }

    //     // 1 step on coarse level equals PATCH_DIM steps on the
    //     // finer levels.
    //     for (int t = 0; t < PATCH_DIM; ++t) {
    //         updateFinerLevel(hood);
    //     }

    //     value = gatherFromSublevel();
    // }

    // template<typename NEIGHBORHOOD>
    // void updateFinerLevel(const NEIGHBORHOOD& hood, double delta)
    // {
    //     for (int y = 0; y < PATCH_DIM; ++y) {
    //         for (int x = 0; x < PATCH_DIM; ++x) {
    //             // fixme: use wrapper object instead of hood
    //             sublevel[Coord<2>(x, y)].actualUpdate(hood);
    //         }
    //     }
    // }

    // double gatherFromSublevel() const
    // {
    //     double newValue = 0;
    //     for (int y = 0; y < PATCH_DIM; ++y) {
    //         for (int x = 0; x < PATCH_DIM; ++x) {
    //             newValue += sublevel[Coord(x, y)].value;
    //         }
    //     }
    //     newValue *= 1.0 / (double(PATCH_DIM) * double(PATCH_DIM));

    //     return newValue;
    // }

#undef HOOD
};

class AMRInitializer : public SimpleInitializer<AMRDiffusionCell<> >
{
public:
    explicit AMRInitializer(
        const Coord<2> dim = Coord<2>(),
        const unsigned steps = 0) :
        SimpleInitializer<AMRDiffusionCell<> >(dim, steps)
    {}

    virtual void grid(GridBase<AMRDiffusionCell<>, 2> *ret)
    {
        CoordBox<2> box = ret->boundingBox();
        int depth = 0;
        int maxDepth = 2;

        Coord<2> logicalOffset(1, 1);
        for (int j = 0; j < maxDepth; ++j) {
            logicalOffset = logicalOffset.scale(
                Coord<2>(
                    AMRDiffusionCell<>::sublevelDimX(),
                    AMRDiffusionCell<>::sublevelDimY()));
        }

        Coord<2> logicalEdgePos = box.origin.scale(logicalOffset);
        Coord<2> logicalEdgeOffset = Coord<2>(
            logicalOffset.x() * box.dimensions.x(),
            logicalOffset.y() * box.dimensions.y());

        ret->setEdge(AMRDiffusionCell<>(
                         // fixme:
                         FloatCoord<2>(-1, -1),
                         FloatCoord<2>( 2,  2),
                         logicalEdgePos,
                         logicalEdgeOffset,
                         0.0,
                         0.0,
                         0,
                         0,
                         true));

        for (CoordBox<2>::Iterator i = box.begin(); i != box.end(); ++i) {
            double influx = 0;
            if (i->y() == 0) {
                influx = i->x();
                if (i->x() >= (box.dimensions.x() / 2)) {
                    influx = box.dimensions.x() - influx - 1;
                }

                influx *= influx;
            }

            FloatCoord<2> dim(1, 1);
            FloatCoord<2> pos = dim.scale(*i);

            Coord<2> logicalPos = i->scale(logicalOffset);


            // fixme: kill this?
            // // no refinement on boundary
            // if ((i->y() == 0) || (i->y() == (box.dimensions.y() - 1)) ||
            //     (i->x() == 0) || (i->x() == (box.dimensions.x() - 1))) {
            //     maxDepth = 0;
            // }

            double flag = 0;
            if (*i == Coord<2>(0, 0)) {
                flag = 1;
            }
            AMRDiffusionCell<> cell(pos, dim, logicalPos, logicalOffset, 0, influx, depth, maxDepth, false, flag);
            ret->set(*i, cell);
        }
    }

};

// class FooBar
// {
// public:

//     template<typename WRITER>
//     void dumpData(WRITER *writer)
//     {
//         double dim = 10;

//         for (int i = 0; i < 10; ++i) {
//             for (int x = 0; x < 8; ++x) {
//                 for (int y = 0; y < 8; ++y) {
//                     writer->addQuad(FloatCoord<2>(x * dim, y * dim), FloatCoord<2>(dim, dim));
//                 }
//             }

//             dim *= 0.125;
//         }
//     }

//     int dummy;
// };


int main(int argc, char **argv)
{
    // Grid<FooBar> grid(Coord<2>(1, 1));
    // SiloWriter<FooBar> writer("foobar", 1);
    // writer.stepFinished(grid, 14, WRITER_INITIALIZED);

    Coord<2> gridDim(15, 10);
    int maxSteps = 1000;
    SerialSimulator<AMRDiffusionCell<> > sim(new AMRInitializer(gridDim, maxSteps));

    SiloWriter<AMRDiffusionCell<> > *siloWriter = new SiloWriter<AMRDiffusionCell<> >("AMR", 1);
    siloWriter->addSelectorForUnstructuredGrid(
        &AMRDiffusionCell<>::value, "value");
    siloWriter->addSelectorForUnstructuredGrid(
        &AMRDiffusionCell<>::contagiousFlag, "flag");
    sim.addWriter(siloWriter);

    sim.run();

    return 0;
}

#ifdef _MSC_BUILD
#pragma warning( disable : 4710 )
#endif
