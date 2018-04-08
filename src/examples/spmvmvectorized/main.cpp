#include <cstdlib>

#include <map>
#include <fstream>
#include <string>
#include <stdexcept>

#include <libgeodecomp.h>
#include <libgeodecomp/io/tracingwriter.h>
#include <libgeodecomp/io/asciiwriter.h>
#include <libgeodecomp/io/simpleinitializer.h>
#include <libgeodecomp/misc/apitraits.h>
#include <libgeodecomp/misc/math.h>
#include <libgeodecomp/parallelization/serialsimulator.h>
#include <libgeodecomp/storage/unstructuredsoagrid.h>
#include <libgeodecomp/io/sellsortingwriter.h>

#include <libflatarray/api_traits.hpp>
#include <libflatarray/macros.hpp>
#include <libflatarray/short_vec.hpp>

using namespace LibGeoDecomp;
using namespace LibFlatArray;

// defining settings for SELL-C-q
typedef double ValueType;
static const std::size_t MATRICES = 1;
static const int C = 4;
static const int SIGMA = 1;
typedef short_vec<ValueType, C> ShortVec;

class Cell
{
public:
    class API :
        public APITraits::HasUpdateLineX,
        public APITraits::HasSoA,
        public APITraits::HasUnstructuredTopology,
        public APITraits::HasPredefinedMPIDataType<double>,
        public APITraits::HasSellType<ValueType>,
        public APITraits::HasSellMatrices<MATRICES>,
        public APITraits::HasSellC<C>,
        public APITraits::HasSellSigma<SIGMA>,
        public LibFlatArray::api_traits::has_default_1d_sizes
    {};

    inline explicit Cell(double v = 0) :
        value(v), sum(0)
    {}

    template<typename HOOD_NEW, typename HOOD_OLD>
    static void updateLineX(HOOD_NEW& hoodNew, int indexEnd, HOOD_OLD& hoodOld, unsigned /* nanoStep */)
    {
        for (; hoodNew.index() < indexEnd; hoodNew += C, ++hoodOld) {
            ShortVec tmp;
            tmp.load_aligned(&hoodNew->sum());

            for (const auto& j: hoodOld.weights(0)) {
                ShortVec weights;
                ShortVec values;
                weights.load_aligned(j.second());
                values.gather(&hoodOld->value(), j.first());
                tmp += values * weights;
            }

            tmp.store_aligned(&hoodNew->sum());
        }
    }

    inline bool operator==(const Cell& cell) const
    {
        return (cell.sum == sum) && (cell.value == value);
    }

    inline bool operator!=(const Cell& cell) const
    {
        return !(*this == cell);
    }

    double value;
    double sum;
};

LIBFLATARRAY_REGISTER_SOA(Cell, ((double)(sum))((double)(value)))

class CellInitializerDiagonal : public SimpleInitializer<Cell>
{
public:
    typedef UnstructuredSoAGrid<Cell, MATRICES, ValueType, C, SIGMA> Grid;

    inline explicit
    CellInitializerDiagonal(unsigned steps) :
        SimpleInitializer<Cell>(Coord<1>(100), steps)
    {}

    virtual void grid(GridBase<Cell, 1> *grid)
    {
        // setup diagonal matrix, one neighbor per cell
        Grid::SparseMatrix weights;

        for (int i = 0; i < 100; ++i) {
            grid->set(Coord<1>(i), Cell(static_cast<double>(i) + 0.1));
            weights << std::make_pair(Coord<2>(i, i), static_cast<ValueType>(i) + 0.1);
        }

        grid->setWeights(0, weights);
    }
};

class CellInitializerMatrix : public SimpleInitializer<Cell>
{
private:
    typedef UnstructuredSoAGrid<Cell, MATRICES, ValueType, C, SIGMA> Grid;
    std::size_t size;           // size of matrix and rhs vector
    std::string rhsFile;        // matrix file name
    std::string matrixFile;     // rhs vector file name

public:
    inline
    CellInitializerMatrix(std::size_t size, unsigned steps,
                          const std::string& rhsFile,
                          const std::string& matrixFile) :
        SimpleInitializer<Cell>(Coord<1>(int(size)), steps),
        size(size), rhsFile(rhsFile), matrixFile(matrixFile)
    {}

    virtual void grid(GridBase<Cell, 1> *grid)
    {
        // read rhs and matrix from file
        GridBase<Cell, 1>::SparseMatrix weights;
        std::ifstream rhsIfs;
        std::ifstream matrixIfs;

        rhsIfs.open(rhsFile);
        matrixIfs.open(matrixFile);
        if (rhsIfs.fail() || matrixIfs.fail()) {
            throw std::logic_error("Failed to open files");
        }

        int i = 0;
        double tmp;
        // read rhs vector
        while (rhsIfs >> tmp) {
            grid->set(Coord<1>(i), Cell(tmp));
            ++i;
        }

        // read matrix
        unsigned rows, cols;
        if (!(matrixIfs >> rows >> cols)) {
            throw std::logic_error("Failed to read from matrix file");
        }
        if ((rows != cols) || (rows != i) || (rows != size)) {
            throw std::logic_error("Dimensions do not match");
        }

        for (unsigned row = 0; row < rows; ++row) {
            for (unsigned col = 0; col < cols; ++col) {
                if (!(matrixIfs >> tmp)) {
                    throw std::logic_error("Failed to read data from matrix");
                }
                if (tmp != 0.0) {
                    weights << std::make_pair(Coord<2>(int(row), int(col)), tmp);
                }
            }
        }

        rhsIfs.close();
        matrixIfs.close();

        grid->setWeights(0, weights);
    }
};

static
void runSimulation(int argc, char *argv[])
{
    SimpleInitializer<Cell> *init;
    unsigned steps = 1;
    unsigned outputFrequency = 1;

    // init
    if (argc > 1) {
        if (argc != 4) {
            throw std::logic_error("usage: spmvm [size] [rhs] [matrix]");
        }
        std::size_t size = static_cast<std::size_t>(std::stoul(argv[1]));
        std::string rhsFile = argv[2];
        std::string matrixFile = argv[3];
        init = new CellInitializerMatrix(size, steps, rhsFile, matrixFile);
    } else {
        init = new CellInitializerDiagonal(steps);
    }
    SerialSimulator<Cell> sim(init);
    sim.addWriter(new TracingWriter<Cell>(outputFrequency, init->maxSteps()));
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4127 )
#endif
    if (SIGMA == 1) {
#ifdef _MSC_BUILD
#pragma warning( pop )
#endif
        sim.addWriter(new ASCIIWriter<Cell>("sum", &Cell::sum, outputFrequency));
    } else {
        // fixme
        // auto asciiWriter = new ASCIIWriter<Cell>("sum", &Cell::sum, outputFrequency);
        // sim.addWriter(new SellSortingWriter<Cell, ASCIIWriter<Cell> >(
        //                   asciiWriter, 0, "sum", &Cell::sum, outputFrequency));
    }
    sim.run();
}

int main(int argc, char *argv[])
{
    runSimulation(argc, argv);

    return EXIT_SUCCESS;
}
