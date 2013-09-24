#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_FEATURE_HPX

#include <libgeodecomp/parallelization/hpxsimulator/createupdategroups.h>

#include <hpx/runtime/components/server/runtime_support.hpp>
#include <hpx/runtime/components/plain_component_factory.hpp>
#include <hpx/lcos/wait_all.hpp>

#include <boost/serialization/vector.hpp>

typedef LibGeoDecomp::HpxSimulator::Implementation::CreateUpdateGroupsAction CreateUpdateGroupsAction;

HPX_REGISTER_PLAIN_ACTION(
    LibGeoDecomp::HpxSimulator::Implementation::CreateUpdateGroupsAction,
    LibGeoDecomp_HpxSimulator_Implementation_CreateUpdateGroupsAction
);

HPX_REGISTER_BASE_LCO_WITH_VALUE(
    LibGeoDecomp::HpxSimulator::Implementation::CreateUpdateGroupsReturnType,
    hpx_base_lco_std_pair_std_size_t_std_vector_hpx_util_remote_locality_result
)

HPX_UTIL_REGISTER_FUNCTION(
    std::size_t(),
    LibGeoDecomp::HpxSimulator::Implementation::OvercommitFunctor,
    LibGeoDecompHpxSimulatorImplementationOvercommitFunctor)

HPX_UTIL_REGISTER_FUNCTION(
    std::size_t(),
    hpx::util::function<std::size_t()>,
    LibGeoDecompHpxSimulatorImplementationFunction)

namespace LibGeoDecomp {
namespace HpxSimulator {
namespace Implementation {

std::pair<std::size_t, std::vector<hpx::util::remote_locality_result> >
createUpdateGroups(
    std::vector<hpx::id_type> localities,
    hpx::components::component_type type,
    const hpx::util::function<std::size_t()>& numUpdateGroups)
{
    typedef hpx::util::remote_locality_result ValueType;
    typedef std::pair<std::size_t, std::vector<ValueType> > ResultType;

    ResultType res;
    if(localities.size() == 0) {
        return res;
    }

    hpx::id_type thisLoc = localities[0];

    typedef
        hpx::components::server::runtime_support::bulk_create_components_action
        ActionType;

    std::size_t numComponents = numUpdateGroups();

    typedef hpx::future<std::vector<hpx::naming::gid_type> > FutureType;

    FutureType f;
    {
        hpx::lcos::packaged_action<ActionType, std::vector<hpx::naming::gid_type> > p;
        p.apply(hpx::launch::async, thisLoc, type, numComponents);
        f = p.get_future();
    }

    std::vector<hpx::future<ResultType> > componentsFutures;
    componentsFutures.reserve(2);

    if(localities.size() > 1) {
        std::size_t half = (localities.size() / 2) + 1;
        std::vector<hpx::id_type> locsFirst(localities.begin() + 1, localities.begin() + half);
        std::vector<hpx::id_type> locsSecond(localities.begin() + half, localities.end());


        if(locsFirst.size() > 0)
        {
            hpx::lcos::packaged_action<CreateUpdateGroupsAction, ResultType > p;
            hpx::id_type id = locsFirst[0];
            p.apply(hpx::launch::async, id, boost::move(locsFirst), type, numUpdateGroups);
            componentsFutures.push_back(
                p.get_future()
            );
        }

        if(locsSecond.size() > 0)
        {
            hpx::lcos::packaged_action<CreateUpdateGroupsAction, ResultType > p;
            hpx::id_type id = locsSecond[0];
            p.apply(hpx::launch::async, id, boost::move(locsSecond), type, numUpdateGroups);
            componentsFutures.push_back(
                p.get_future()
            );
        }
    }

    res.first = numComponents;
    res.second.push_back(
        ValueType(thisLoc.get_gid(), type)
    );
    res.second.back().gids_ = boost::move(f.move());

    hpx::wait_all(componentsFutures);

    BOOST_FOREACH(hpx::future<ResultType> & rf, componentsFutures)
    {
        ResultType r = rf.move();
        res.second.insert(res.second.end(), r.second.begin(), r.second.end());
        res.first += r.first;
    }

    return res;
}

}
}
}

#endif
