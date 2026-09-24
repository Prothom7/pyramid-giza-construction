#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"
#include "animation/ConstructionAnimation.h"
#include "graphics/Mesh.h"
#include "objects/Worker.h"
#include "scene/PyramidLayout.h"
#include "scene/SceneTypes.h"

struct StaticGizaSceneStats
{
    std::size_t pyramidBlocks = 0;
    std::size_t rampComponents = 0;
    std::size_t scaffoldModules = 0;
    std::size_t quarryBlocks = 0;
    std::size_t quarryObjects = 0;
    std::size_t stockpileBlocks = 0;
    std::size_t repositoryBlocks = 0;
    std::size_t constructionProps = 0;
    std::size_t treeInstances = 0;
    std::size_t liftingRigs = 0;
    std::size_t enrichmentRopeRigs = 0;
    std::size_t anchorPosts = 0;
    std::size_t ladders = 0;
    std::size_t boats = 0;
    std::size_t workshopClusters = 0;
    std::size_t repairStations = 0;
    std::size_t scaffoldAccessObjects = 0;
    std::size_t upperPlatformObjects = 0;
    std::size_t riverLandingObjects = 0;
    std::size_t enrichmentObjects = 0;
    std::size_t sphinxParts = 0;
    std::size_t environmentalObjects = 0;
    std::size_t workerInstances = 0;
    std::size_t heroWorkers = 0;
    std::size_t secondaryWorkers = 0;
    std::size_t backgroundWorkers = 0;
    std::size_t workerParts = 0;
    std::size_t sledgeInstances = 0;
    std::size_t compositeEquipmentParts = 0;
    std::size_t totalDrawCalls = 0;
};

class StaticGizaScene
{
public:
    StaticGizaScene();

    void render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition);
    void update(float deltaTime);
    void togglePlayback();
    void toggleCoordinatedAnimation();
    void advanceAnimationState();
    void toggleAnimationLoop();
    void adjustAnimationSpeed(float amount);
    void cycleDemoPose();
    void resetAnimation();
    bool animationPaused() const { return animationController_.paused(); }
    bool animationLooping() const { return animationController_.looping(); }
    bool coordinatedAnimationEnabled() const { return coordinatedAnimationEnabled_; }
    float animationSpeed() const { return animationController_.speed(); }
    const char* animationStateName() const
    {
        return ConstructionAnimationController::stateName(animationController_.state());
    }
    const char* demoPoseName() const { return Worker::poseName(demoPose_); }
    glm::vec3 transportTarget() const;

    const StaticGizaSceneStats& stats() const { return stats_; }
    const PyramidLayoutConfig& pyramidConfig() const { return pyramidConfig_; }

private:
    struct WorkerInstance
    {
        glm::mat4 root{1.0f};
        WorkerPose pose = WorkerPose::Standing;
        WorkerJointAngles jointAngles;
        WorkerStyle style;
        bool isHero = false;
        bool isSecondary = false;
        bool isDemoWorker = false;
        float animationPhase = 0.0f;
    };

    void addObject(ScenePrimitive primitive, const glm::mat4& model, MaterialId material);
    void addComposite(const glm::mat4& root, const std::vector<ObjectPart>& parts);
    void addWorker(const glm::vec3& position, float rotationY, WorkerPose pose,
                   MaterialId clothing, bool hero, bool secondary,
                   bool demoWorker = false, float phase = 0.0f);
    void addStaticSledge(const glm::vec3& position, float rotationY, float scale = 1.0f);
    void buildGround();
    void buildPyramid();
    void buildRampNetwork();
    void buildScaffolding();
    void buildQuarryAndCutting();
    void buildStockpiles();
    void buildTransportLanes();
    void buildTimberAndCamp();
    void buildNileAndContext();
    void buildHeavyLiftingRig();
    void buildObjectEnrichment();
    void buildRopeInfrastructure();
    void buildScaffoldAccess();
    void buildWorkshopRepairAndInspection();
    void buildRiverLanding();
    void buildUpperPlatformDetails();
    void buildCompositeObjects();
    const Mesh& meshFor(ScenePrimitive primitive) const;

    Shader shader_;
    Mesh plane_;
    Mesh cube_;
    Mesh cylinder_;
    Mesh sphere_;
    PyramidLayoutConfig pyramidConfig_;
    std::vector<SceneObject> objects_;
    std::vector<WorkerInstance> workers_;
    std::vector<ObjectPart> loadedSledgeParts_;
    ConstructionAnimationController animationController_;
    WorkerPose demoPose_ = WorkerPose::Standing;
    float articulationTime_ = 0.0f;
    float articulationSpeed_ = 1.0f;
    bool articulationPreviewEnabled_ = true;
    bool coordinatedAnimationEnabled_ = true;
    StaticGizaSceneStats stats_;
};
