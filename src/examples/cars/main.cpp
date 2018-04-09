#include <mpi.h>

#include <libgeodecomp/communication/typemaps.h>
#include <libgeodecomp/io/remotesteerer.h>
#include <libgeodecomp/io/simpleinitializer.h>
#include <libgeodecomp/io/tracingwriter.h>
#include <libgeodecomp/io/visitwriter.h>
#include <libgeodecomp/io/remotesteerer.h>
#include <libgeodecomp/parallelization/serialsimulator.h>
#include <libgeodecomp/storage/dataaccessor.h>

using namespace LibGeoDecomp;

enum State {
    FREE = 0,
    EAST = -1,
    SOUTH = 2,
    BLOCK = 1
};

void runSimulation();

class Cell
{
public:
    friend void runSimulation();
    friend class CellInitializer;

    class API :
        public APITraits::HasNanoSteps<2>
    {};

    explicit Cell(int direction = FREE, int border = 0, const int rate = 5) :
        direction(direction),
        border(border),
        rate(rate)
    {}

    // fixme: shorten by decomposition
    void update(const CoordMap<Cell>& neighborhood, unsigned nanoStep)
    {
        *this = neighborhood[Coord<2>(0, 0)];

        if (nanoStep == 0) {
            if ((direction == FREE) && (neighborhood[Coord<2>(0, 1)].direction == SOUTH)) {
                direction = SOUTH;
            } else if ((direction == SOUTH) && (neighborhood[Coord<2>(0, -1)].direction == FREE)) {
                direction = FREE;
            }

            else if ((direction == FREE) && (neighborhood[Coord<2>(-1, -1)].direction == BLOCK)
                     && (neighborhood[Coord<2>(-1, 0)].direction == SOUTH)
                     && (neighborhood[Coord<2>(0, 1)].direction == FREE)) {
                direction = SOUTH;
            } else if ((direction == SOUTH) && (neighborhood[Coord<2>(0, -1)].direction == BLOCK)) {
                if ((neighborhood[Coord<2>(1, 0)].direction == FREE) &&
                    (neighborhood[Coord<2>(1, 1)].direction == FREE)) {
                    direction = FREE;
                }
            }

            else if ((direction == FREE) && (neighborhood[Coord<2>(-1, 0)].direction == EAST)
                && (neighborhood[Coord<2>(0, 1)].direction != SOUTH)) {
                direction = EAST;
            } else if ((direction == EAST) && (neighborhood[Coord<2>(1, 0)].direction == FREE)
                 && (neighborhood[Coord<2>(1, 1)].direction != SOUTH)) {
                direction = FREE;
            }

            else if ((direction == FREE) && (neighborhood[Coord<2>(1, 1)].direction == BLOCK)
                     && (neighborhood[Coord<2>(0, 1)].direction == EAST)
                     && (neighborhood[Coord<2>(-1, 0)].direction == FREE)) {
                direction = EAST;
            } else if ((direction == EAST) && (neighborhood[Coord<2>(1, 0)].direction == BLOCK)) {
                if ((neighborhood[Coord<2>(0, -1)].direction == FREE) &&
                    (neighborhood[Coord<2>(-1, -1)].direction == FREE)) {
                    direction = FREE;
                }
            }
        }
        if (nanoStep == 1) {
            if ((direction == FREE) && (border == 1)) {
                int random_integer = (rand()%100);;
                if (random_integer >= (100 - rate)) {
                    direction = SOUTH;
                }
            }
            if ((direction == FREE) && (border == -1)) {
                int random_integer = (rand()%100);;
                if (random_integer >= (100 - rate)) {
                    direction = EAST;
                }
            }
        }
    }

private:
    int direction;
    int border;
    int rate;
};

class CellInitializer : public SimpleInitializer<Cell>
{
public:
    explicit CellInitializer(int maxSteps) :
        SimpleInitializer<Cell>(Coord<2>(90, 90), maxSteps)
    {}

    virtual void grid(GridBase<Cell, 2> *ret)
    {
        for (int i = 1; i < 90; ++i) {
            ret->set(Coord<2>(i, 89), Cell(0, 1));
        }

        for (int j = 0; j < 89; ++j) {
            for (int i = 0; i < 90; ++i) {
                if (i == 0) {
                    ret->set(Coord<2>(i, j), Cell(0, -1));
                } else {
                    ret->set(Coord<2>(i, j), Cell(0));
                }
            }
        }

        for (int j = 43; j < 48; ++j) {
            for (int i = 43; i < 48; ++i) {
                Cell c = ret->get(Coord<2>(i, j));
                c.direction = BLOCK;
                ret->set(Coord<2>(i, j), c);
            }
        }

        for (int j = 23; j < 28; ++j) {
            for (int i = 73; i < 78; ++i) {
                Cell c = ret->get(Coord<2>(i, j));
                c.direction = BLOCK;
                ret->set(Coord<2>(i, j), c);
            }
        }
    }
};

// DEFINE_DATAACCESSOR(BorderAccessor,    Cell, int, border)
// DEFINE_DATAACCESSOR(DirectionAccessor, Cell, int, direction)
// DEFINE_DATAACCESSOR(RateAccessor,      Cell, int, rate)

// fixme: does this even do anything?
// static void getStepFunction(
//     std::vector<std::string> stringVec,
//     RemoteSteererHelpers::CommandServer<Cell> *server)
// {
//     std::string helpMessage = "    Usage: getstep\n";
//     helpMessage += "          get the size of the region\n";
//     if (stringVec.size() > 1) {
//         if (stringVec.at(1).compare("help") == 0) {
//             server->sendMessage(helpMessage);
//             return;
//         }
//     }
//     std::string msg = "send step request\n";
//     server->sendMessage(msg);
// }

// fixme: shorten function by decomposition
// static void setRateFunction(
//     std::vector<std::string> stringVec,
//     RemoteSteererHelpers::CommandServer<Cell> *server)
// {
//     std::string helpMessage = "    Usage: setrate <direction> <probability>\n";
//     helpMessage += "          sets the probability of a new car for the given direction\n";
//     helpMessage += "          directions: east and south\n";
//     helpMessage += "          probability: int between 0 and 100\n";
//     std::cout << "called setRateFunction" << std::endl;
//     if (stringVec.size() != 3) {
//         std::cout << "wrong number" << std::endl;
//         server->sendMessage(helpMessage);
//         return;
//     }
//     int value;
//     try {
//         value = mystrtoi(stringVec.at(2).c_str());
//     }
//     catch (std::exception& e) {
//         std::cout << "toi fehler" << std::endl;
//         server->sendMessage(helpMessage);
//         return;
//     }
//     if ((value < 0) || (value > 100)) {
//         std::cout << "range fehler" << std::endl;
//         server->sendMessage(helpMessage);
//         server->sendMessage(helpMessage);
//         return;
//     }
//     if (stringVec.at(1).compare("east") == 0) {
//         for (int i = 0; i < 89; ++i) {
//             std::vector<std::string> newCommandVec;
//             newCommandVec.push_back("set");
//             newCommandVec.push_back("rate");
//             newCommandVec.push_back(stringVec.at(2));
//             newCommandVec.push_back("0");
//             newCommandVec.push_back(boost::to_string(i));
//             RemoteSteerer<Cell>::setFunction(newCommandVec, server);
//         }
//     } else if (stringVec.at(1).compare("south") == 0) {
//         for (int i = 1; i < 90; ++i) {
//             std::vector<std::string> newCommandVec;
//             newCommandVec.push_back("set");
//             newCommandVec.push_back("rate");
//             newCommandVec.push_back(stringVec.at(2));
//             newCommandVec.push_back(boost::to_string(i));
//             newCommandVec.push_back("89");
//             RemoteSteerer<Cell>::setFunction(newCommandVec, server);
//         }
//     } else {
//         std::cout << "direction falsch" << std::endl;
//         server->sendMessage(helpMessage);
//         return;
//     }
//     std::string msg = "send setrate request\n";
//     std::cout << "setrate finish" << std::endl;
//     server->sendMessage(msg);
// }

void runSimulation()
{
    int maxSteps = 1000000;
    SerialSimulator<Cell> sim(new CellInitializer(maxSteps));

    VisItWriter<Cell> *visItWriter = new VisItWriter<Cell>("cars", 1, true);
    visItWriter->addVariable(&Cell::border,    "border");
    visItWriter->addVariable(&Cell::direction, "direction");
    visItWriter->addVariable(&Cell::rate,      "rate");
    sim.addWriter(visItWriter);

    sim.addWriter(new TracingWriter<Cell>(1, maxSteps));

    // SteererData<Cell> *myData = new SteererData<Cell>();
    // myData->addVariable(new borderDataAccessor());
    // myData->addVariable(new directionDataAccessor());
    // myData->addVariable(new rateDataAccessor());

    // CommandServer<Cell>::FunctionMap functionMap = RemoteSteerer<Cell>::getDefaultMap();
    // fixme: reactivate these
    // functionMap["getstep"] = getStepFunction;
    // functionMap["setrate"] = setRateFunction;

    // Steerer<Cell> *steerer =
    //     new RemoteSteerer<Cell>(
    //     1,
    //     1234,
    //     functionMap,
    //     myData,
    //     MPI_COMM_WORLD);
    // sim.addSteerer(steerer);

    sim.run();
}

int main(int argc, char* argv[])
{
    srand((unsigned)time(0));

    runSimulation();

    return 0;
}

#ifdef _MSC_BUILD
#pragma warning( disable : 4710 )
#endif
