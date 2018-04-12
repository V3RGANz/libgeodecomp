#include <libgeodecomp/io/logger.h>
#include <libgeodecomp/misc/math.h>
#include <libgeodecomp/misc/patternoptimizer.h>

// Kill some warnings in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 4710 4711 )
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

namespace LibGeoDecomp {

PatternOptimizer::PatternOptimizer(
    SimulationParameters params,
    std::vector<double> stepwidth,
    std::vector<double> minStepwidth) :
    Optimizer(params),
    stepwidth(stepwidth),
    minStepwidth(minStepwidth)
{
    LOG(Logger::DBG, "Constructor call PatternOptimizer")

    if ((stepwidth.size() == 0) && (minStepwidth.size() == 0)) {
        for (std::size_t i = 0; i < params.size(); ++i) {
            double dimsize = Optimizer::params[i].getMax()
                - Optimizer::params[i].getMin();
            if ((dimsize / 4) >= 1) {
                this->stepwidth.push_back(dimsize / 4);
            } else {
                this->stepwidth.push_back(params[i].getGranularity());
            }
            this->params[i].setValue(dimsize / 2);
            // minStepwidht >= granularity
            // it will be used as abort criterion
            this->minStepwidth.push_back(params[i].getGranularity());
        }
    }

    if ((minStepwidth.size() == 0) && (stepwidth.size() == params.size())) {
        for (std::size_t i = 0; i < params.size(); ++i) {
            this->minStepwidth.push_back(params[i].getGranularity());
        }
    }

    // fixme: exceptions in c-tor are evil!
    if (this->stepwidth.size() != this->params.size() ) {
        throw std::invalid_argument("Wrong size of Stepwidth in PatternOptimizer Constructor!");
    }

    if (this->minStepwidth.size() != this->params.size()) {
        throw std::invalid_argument("Wrong size of i minStepwidth in PatternOptimizer Constructor!");
    }
}

/**
 * returns false if all parameters alrady on minimum
 */
bool PatternOptimizer::reduceStepwidth()
{
    bool allWasMin = true;
    std::stringstream log;
    log << "Reduce Stepwidth:" << std::endl;

    for (std::size_t i = 0;i < stepwidth.size(); ++i) {
        log << "Dimension "<< i << ": " << stepwidth[i];
        if (stepwidth[i] <= minStepwidth[i]) {
            log << " --> " << stepwidth[i] << "; " << std::endl;
            stepwidth[i] = minStepwidth[i];
            continue;
        }
        stepwidth[i] = stepwidth[i] / 2;
        if (stepwidth[i] <= minStepwidth[i]) {
            stepwidth[i] = minStepwidth[i];
        }
        allWasMin = false;
        log << " --> " << stepwidth[i] << "; " << std::endl;
    }

    LOG(Logger::DBG, log.str())
    return !allWasMin;
}

std::vector<SimulationParameters> PatternOptimizer::genPattern(SimulationParameters middle)
{
    std::vector<SimulationParameters> result(middle.size() * 2 + 1);
    result[0]=middle;

    for (std::size_t i = 0; i < middle.size(); ++i) {
        SimulationParameters tmp1(middle),tmp2(middle);
        tmp1[i] += stepwidth[i];
        tmp2[i] += (stepwidth[i] * -1.0);
        result[1 + i * 2] = tmp1;
        result[2 + i * 2] = tmp2;
    }

    return result;
}

std::size_t PatternOptimizer::getMaxPos(
    const std::vector<SimulationParameters>& pattern,
    Evaluator& eval,
    std::size_t oldMiddle)
{
    std::size_t retval = 0;
    double newFitness;

    // i = 1 middle doesn't need to be evaluate again
    for (std::size_t i = 1; i < pattern.size(); ++i) {
        std::size_t halfI = (i - 1) / 2;
        // all pattern[i] with the same coordinates as middle, oldMiddle don't need to be evaluate
        if ((pattern[0][halfI].getValue() == pattern[i][halfI].getValue()) ||
            ((i % 2) == 0 && (i - 1) == oldMiddle) ||
            ((i % 2) != 0 && (i + 1) == oldMiddle)) {
            continue;
        }

        newFitness = eval(pattern[i]);

        if (newFitness >= Optimizer::fitness) {
            retval = i;
            Optimizer::fitness = newFitness;
        }
    }

    return retval;
}

SimulationParameters PatternOptimizer::operator()(unsigned steps, Evaluator& eval)
{
    SimulationParameters middle = Optimizer::params;
    std::vector<SimulationParameters> pattern = genPattern(middle);
    std::size_t maxPos = 0;
    fitness = eval(middle);
    for (unsigned k = 0; k < steps; ++k) {
        pattern = genPattern(middle);
        maxPos = getMaxPos(pattern, eval,maxPos);
        LOG(Logger::DBG, patternToString(pattern) << "maxPos: " << maxPos )

        if (maxPos == 0) {            // center was the Maximum
            if (!reduceStepwidth()) {  // abort test
                LOG(Logger::DBG,  "fitness: " << Optimizer::fitness)
                return middle;
            }
        } else {                      // do next step
            middle = pattern[maxPos];
        }
    }
    // unreachable if steps is big enough
    return middle;
}

std::string PatternOptimizer::patternToString(std::vector<SimulationParameters> pattern){
    std::stringstream result;
    result << "Pattern: " << std::endl;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (i == 0) {
            result << "Middle:         ";
        } else {
            result << "Direction: " << i << " :	";
        }
        for (std::size_t j = 0; j < pattern[i].size(); ++j) {
            result << pattern[i][j].getValue() << " - ";
        }
        result << std::endl;
    }

    result << std::endl;

    return result.str();
}

}

#ifdef _MSC_BUILD
#pragma warning( disable : 4710 )
#endif
