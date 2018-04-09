#include <libgeodecomp.h>
#include <libgeodecomp/io/tracingwriter.h>
#include <libgeodecomp/io/asciiwriter.h>
#include <libgeodecomp/io/simpleinitializer.h>
#include <libgeodecomp/misc/apitraits.h>
#include <libgeodecomp/misc/math.h>
#include <libgeodecomp/parallelization/serialsimulator.h>
#include <libgeodecomp/storage/unstructuredgrid.h>

// Kill warning 4514 in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

#include <cstdlib>
#include <map>
#include <fstream>
#include <string>
#include <stdexcept>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

using namespace LibGeoDecomp;

// defining settings for SELL-C-q
typedef double ValueType;
const std::size_t MATRICES = 1;
const int C = 4;
const int SIGMA = 1;

class Cell
{
public:
    class API :
        public APITraits::HasUpdateLineX,
        public APITraits::HasUnstructuredTopology,
        public APITraits::HasPredefinedMPIDataType<double>,
        public APITraits::HasSellType<ValueType>,
        public APITraits::HasSellMatrices<MATRICES>,
        public APITraits::HasSellC<C>,
        public APITraits::HasSellSigma<SIGMA>
    {};

    inline explicit Cell(double v = 0) :
        value(v), sum(0)
    {}

    template<typename HOOD_NEW, typename HOOD_OLD>
    static void updateLineX(
        HOOD_NEW& hoodNew,
        int indexEnd,
        HOOD_OLD& hoodOld,
        unsigned /* nanoStep */)
    {
        for (int i = hoodOld.index(); i < indexEnd; ++i, ++hoodOld) {
            hoodNew[i].sum = 0.;
            for (const auto& j: hoodOld.weights(0)) {
                hoodNew[i].sum += hoodOld[j.first()].value * j.second();
            }
        }
    }

    double value;
    double sum;
};

class CellInitializerDiagonal : public SimpleInitializer<Cell>
{
public:
    typedef UnstructuredGrid<Cell, MATRICES, ValueType, C, SIGMA> Grid;

    inline explicit
    CellInitializerDiagonal(unsigned steps)
        : SimpleInitializer<Cell>(Coord<1>(100), steps)
    {}

    virtual void grid(GridBase<Cell, 1> *grid)
    {
        // setup diagonal matrix, one neighbor per cell
        GridBase<Cell, 1>::SparseMatrix weights;

        for (int i = 0; i < 100; ++i) {
            grid->set(Coord<1>(i), Cell(static_cast<double>(i) + 0.1));
            weights << std::make_pair(Coord<2>(i, i),  static_cast<ValueType>(i) + 0.1);
        }

        grid->setWeights(0, weights);
    }
};

class CellInitializerMatrix : public SimpleInitializer<Cell>
{
private:
    typedef UnstructuredGrid<Cell, MATRICES, ValueType, C, SIGMA> Grid;
    std::size_t size;           // size of matrix and rhs vector
    std::string rhsFile;        // matrix file name
    std::string matrixFile;     // rhs vector file name

public:
    inline
    CellInitializerMatrix(
        std::size_t size,
        unsigned steps,
        const std::string& rhsFile,
        const std::string& matrixFile) :
        SimpleInitializer<Cell>(Coord<1>(int(size)), steps),
        size(size),
        rhsFile(rhsFile),
        matrixFile(matrixFile)
    {}

    virtual void grid(GridBase<Cell, 1> *grid)
    {
        // read RHS and matrix from file
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
                ValueType tmp;
                if (!(matrixIfs >> tmp)) {
                    throw std::logic_error("Failed to read data from matrix");
                }
                if (tmp != 0.0) {
                    weights << std::make_pair(Coord<2>(row, col), tmp);
                }
            }
        }

        rhsIfs.close();
        matrixIfs.close();

        grid->setWeights(0, weights);
    }
};

void runSimulation(int argc, char *argv[])
{
    SimpleInitializer<Cell> *init;
    unsigned steps = 1;
    int outputFrequency = 1;

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
    sim.addWriter(new ASCIIWriter<Cell>("sum", &Cell::sum, outputFrequency));

    sim.run();
}

int main(int argc, char *argv[])
{
    runSimulation(argc, argv);
    return EXIT_SUCCESS;
}

#ifdef _MSC_BUILD
#pragma warning( disable : 4710 )
#endif
