#ifndef LIBGEODECOMP_PARALLELIZATION_NESTING_UPDATEGROUP_H
#define LIBGEODECOMP_PARALLELIZATION_NESTING_UPDATEGROUP_H

#include <libgeodecomp/io/initializer.h>
#include <libgeodecomp/io/steerer.h>
#include <libgeodecomp/io/parallelwriter.h>
#include <libgeodecomp/geometry/partitionmanager.h>
#include <libgeodecomp/geometry/region.h>
#include <libgeodecomp/misc/sharedptr.h>
#include <libgeodecomp/parallelization/nesting/stepper.h>
#include <libgeodecomp/parallelization/nesting/vanillastepper.h>
#include <libgeodecomp/storage/displacedgrid.h>
#include <libgeodecomp/storage/gridtypeselector.h>
#include <libgeodecomp/storage/patchaccepter.h>
#include <libgeodecomp/storage/patchprovider.h>

namespace LibGeoDecomp {

/**
 * The purpose of the UpdateGroup is to set up the basic building
 * blocks for a Simulator which implements LibGeoDecomp's hierarchical
 * parallelization for tightly coupled simulations. To be more
 * specific: this class sets up the PartitionManager (which decides
 * the domain decomposition) and, based on that, the PatchLinks (for
 * ghost zone communication) and the Stepper (which handles the actual
 * updates of the local subdomain).
 */
template<typename CELL_TYPE, template<typename GRID_TYPE> class PATCH_LINK>
class UpdateGroup
{
public:
    typedef LibGeoDecomp::Stepper<CELL_TYPE> StepperType;
    typedef typename StepperType::Topology Topology;
    const static int DIM = Topology::DIM;

    typedef typename SharedPtr<Partition<DIM> >::Type PartitionPtr;
    typedef typename SharedPtr<Initializer<CELL_TYPE> >::Type InitPtr;
    typedef typename SharedPtr<ParallelWriter<CELL_TYPE> >::Type WriterPtr;
    typedef typename SharedPtr<Steerer<CELL_TYPE> >::Type SteererPtr;
    typedef typename SharedPtr<StepperType>::Type StepperPtr;
    typedef typename APITraits::SelectSoA<CELL_TYPE>::Value SupportsSoA;
    typedef typename GridTypeSelector<CELL_TYPE, Topology, true, SupportsSoA>::Value GridType;
    typedef typename PATCH_LINK<GridType>::Link PatchLink;
    typedef typename PATCH_LINK<GridType>::Accepter PatchLinkAccepter;
    typedef typename PATCH_LINK<GridType>::Provider PatchLinkProvider;
    typedef typename SharedPtr<PatchLink>::Type PatchLinkPtr;
    typedef typename SharedPtr<PatchLinkAccepter>::Type PatchLinkAccepterPtr;
    typedef typename SharedPtr<PatchLinkProvider>::Type PatchLinkProviderPtr;
    typedef PartitionManager<Topology> PartitionManagerType;
    typedef typename PartitionManagerType::RegionVecMap RegionVecMap;
    typedef typename StepperType::PatchAccepterVec PatchAccepterVec;
    typedef typename StepperType::PatchProviderVec PatchProviderVec;
    typedef typename StepperType::PatchType PatchType;
    typedef typename StepperType::PatchProviderPtr PatchProviderPtr;
    typedef typename StepperType::PatchAccepterPtr PatchAccepterPtr;

    UpdateGroup(
        unsigned ghostZoneWidth,
        InitPtr initializer,
        unsigned rank) :
        partitionManager(new PartitionManagerType()),
        ghostZoneWidth(ghostZoneWidth),
        initializer(initializer),
        rank(rank)
    {
        // actual initialization is done in init() and can't be done
        // here as we need to call several virtual functions
        // implemented in derived classes. these functions will rely
        // on members which are uninitialized while this c-tor is
        // running.
    }

    virtual ~UpdateGroup()
    {
        for (typename std::vector<PatchLinkPtr>::iterator i = patchLinks.begin();
             i != patchLinks.end();
             ++i) {
            (*i)->cleanup();
        }
    }

    const Chronometer& statistics() const
    {
        return stepper->statistics();
    }

    void addPatchProvider(
        const PatchProviderPtr& patchProvider,
        const PatchType& patchType)
    {
        stepper->addPatchProvider(patchProvider, patchType);
    }

    void addPatchAccepter(
        const PatchAccepterPtr& patchAccepter,
        const PatchType& patchType)
    {
        stepper->addPatchAccepter(patchAccepter, patchType);
    }

    inline void update(unsigned nanoSteps)
    {
        stepper->update(nanoSteps);
    }

    const GridType& grid() const
    {
        return stepper->grid();
    }

    inline virtual std::pair<int, int> currentStep() const
    {
        return stepper->currentStep();
    }

    inline const std::vector<std::size_t>& getWeights() const
    {
        return partitionManager->getWeights();
    }

    inline double computeTimeInner() const
    {
        return stepper->computeTimeInner;
    }

    inline double computeTimeGhost() const
    {
        return stepper->computeTimeGhost;
    }

    inline double patchAcceptersTime() const
    {
        return stepper->patchAcceptersTime;
    }

    inline double patchProvidersTime() const
    {
        return stepper->patchAcceptersTime;
    }

protected:
    std::vector<PatchLinkPtr> patchLinks;
    StepperPtr stepper;
    typename SharedPtr<PartitionManagerType>::Type partitionManager;
    unsigned ghostZoneWidth;
    InitPtr initializer;
    unsigned rank;

    /**
     * Actual initialization of the UpdateGroup, can't be done in
     * c-tor as it relies on methods which are purely virtual in this
     * class and which may/will rely on members in derived classes.
     * Hence derived classes need to call this function in their
     * c-tor.
     */
    template<typename STEPPER>
    void init(
        PartitionPtr partition,
        const CoordBox<DIM>& box,
        unsigned ghostZoneWidth,
        InitPtr initializer,
        STEPPER *stepperType,
        PatchAccepterVec patchAcceptersGhost,
        PatchAccepterVec patchAcceptersInner,
        PatchProviderVec patchProvidersGhost,
        PatchProviderVec patchProvidersInner,
        bool enableFineGrainedParallelism)
    {
        partitionManager->resetRegions(
            initializer,
            box,
            partition,
            rank,
            ghostZoneWidth);
        std::size_t size = partition->getWeights().size();
        std::vector<CoordBox<DIM> > boundingBoxes =
            gatherBoundingBoxes(partitionManager->ownRegion().boundingBox(), size, 0);
        std::vector<CoordBox<DIM> > expandedBoundingBoxes =
            gatherBoundingBoxes(partitionManager->ownExpandedRegion().boundingBox(), size, 1);
        partitionManager->resetGhostZones(boundingBoxes, expandedBoundingBoxes);

        long firstSyncPoint =
            initializer->startStep() * APITraits::SelectNanoSteps<CELL_TYPE>::VALUE +
            ghostZoneWidth;

        // We need to create the patch providers first, as the HPX patch
        // accepters will look up their IDs upon creation:
        PatchProviderVec patchLinkProviders;
        const RegionVecMap& map1 = partitionManager->getOuterGhostZoneFragments();
        for (typename RegionVecMap::const_iterator i = map1.begin(); i != map1.end(); ++i) {
            if (!i->second.back().empty()) {
                PatchLinkProviderPtr link(
                    makePatchLinkProvider(i->first, i->second.back()));
                patchLinkProviders << link;
                patchLinks << link;

                link->charge(
                    firstSyncPoint,
                    PatchProvider<GridType>::infinity(),
                    ghostZoneWidth);

                link->setRegion(partitionManager->ownRegion());
            }
        }

        // we have to hand over a list of all ghostzone senders as the
        // stepper will perform an initial update of the ghostzones
        // upon creation and we have to send those over to our neighbors.
        PatchAccepterVec ghostZoneAccepterLinks;
        const RegionVecMap& map2 = partitionManager->getInnerGhostZoneFragments();
        for (typename RegionVecMap::const_iterator i = map2.begin(); i != map2.end(); ++i) {
            if (!i->second.back().empty()) {
                PatchLinkAccepterPtr link(
                    makePatchLinkAccepter(i->first, i->second.back()));
                ghostZoneAccepterLinks << link;
                patchLinks << link;

                link->charge(
                    firstSyncPoint,
                    PatchAccepter<GridType>::infinity(),
                    ghostZoneWidth);

                link->setRegion(partitionManager->ownRegion());
            }
        }

        // notify all PatchAccepters of the process' region:
        for (std::size_t i = 0; i < patchAcceptersGhost.size(); ++i) {
            patchAcceptersGhost[i]->setRegion(partitionManager->ownRegion());
        }
        for (std::size_t i = 0; i < patchAcceptersInner.size(); ++i) {
            patchAcceptersInner[i]->setRegion(partitionManager->ownRegion());
        }

        // notify all PatchProviders of the process' region:
        for (std::size_t i = 0; i < patchProvidersGhost.size(); ++i) {
            patchProvidersGhost[i]->setRegion(partitionManager->ownRegion());
        }
        for (std::size_t i = 0; i < patchProvidersInner.size(); ++i) {
            patchProvidersInner[i]->setRegion(partitionManager->ownRegion());
        }

        stepper.reset(
            new STEPPER(
                partitionManager,
                this->initializer,
                patchAcceptersGhost + ghostZoneAccepterLinks,
                patchAcceptersInner,
                // add external PatchProviders last to allow them to override
                // the local ghost zone providers (a.k.a. PatchLink::Source).
                patchLinkProviders,
                patchProvidersGhost,
                patchProvidersInner,
                enableFineGrainedParallelism));
    }

    virtual std::vector<CoordBox<DIM> > gatherBoundingBoxes(
        const CoordBox<DIM>& ownBoundingBox,
        std::size_t size,
        std::size_t tag) const = 0;

    virtual PatchLinkAccepterPtr makePatchLinkAccepter(int target, const Region<DIM>& region) = 0;
    virtual PatchLinkProviderPtr makePatchLinkProvider(int source, const Region<DIM>& region) = 0;
};

}

#endif
