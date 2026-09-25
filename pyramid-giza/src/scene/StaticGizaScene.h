#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"
#include "animation/ConstructionAnimation.h"
#include "animation/ConstructionTimeline.h"
#include "graphics/Frustum.h"
#include "graphics/InstanceBatch.h"
#include "graphics/Mesh.h"
#include "graphics/Texture.h"
#include "lighting/ShadowMap.h"
#include "lighting/SunController.h"
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
    std::size_t shadowDepthDrawCalls = 0;
    std::size_t combinedDrawCalls = 0;
};

struct RenderStats
{
    std::uint64_t visibleDrawCalls = 0;
    std::uint64_t shadowDrawCalls = 0;
    std::uint64_t visibleInstances = 0;
    std::uint64_t shadowInstances = 0;
    std::uint64_t visibleTriangles = 0;
    std::uint64_t shadowTriangles = 0;
    std::uint64_t culledObjects = 0;
    std::uint64_t culledInstances = 0;
    std::uint64_t materialChanges = 0;
    std::uint64_t textureBinds = 0;
    double cpuSubmissionMilliseconds = 0.0;
};

class StaticGizaScene
{
public:
    explicit StaticGizaScene(int shadowResolution = ShadowSettings::defaultResolution);

    void render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition,
                int viewportWidth, int viewportHeight);
    void update(float deltaTime);
    void togglePlayback();
    void toggleCoordinatedAnimation();
    void advanceAnimationState();
    void toggleAnimationLoop();
    void adjustAnimationSpeed(float amount);
    void cycleDemoPose();
    void resetAnimation();
    void toggleAutomaticSun();
    void adjustSunTime(float hours);
    void selectMorningSun();
    void selectNoonSun();
    void selectEveningSun();
    void setSunTime(float hours);
    void setSunAutomatic(bool enabled);
    void cycleLightingDebugMode();
    void setLightingDebugMode(LightingDebugMode mode);
    void toggleShadows() { shadowsEnabled_ = !shadowsEnabled_; }
    void setShadowsEnabled(bool enabled) { shadowsEnabled_ = enabled; }
    bool shadowsEnabled() const { return shadowsEnabled_; }
    void cycleShadowDebugMode();
    void setShadowDebugMode(ShadowDebugMode mode);
    ShadowDebugMode shadowDebugMode() const { return shadowDebugMode_; }
    const char* shadowDebugModeName() const;
    void toggleTextures() { texturesEnabled_ = !texturesEnabled_; }
    void setTexturesEnabled(bool enabled) { texturesEnabled_ = enabled; }
    bool texturesEnabled() const { return texturesEnabled_; }
    void toggleConstructionTimelapse() { constructionTimeline_.togglePlayback(); }
    void adjustConstructionSpeed(int direction) { constructionTimeline_.adjustSpeed(direction); }
    void resetConstruction() { constructionTimeline_.reset(); }
    void completeConstruction() { constructionTimeline_.complete(); }
    void setConstructionProgress(float progress) { constructionTimeline_.setProgress(progress); }
    void setConstructionSpeed(float speed) { constructionTimeline_.setSpeed(speed); }
    void setConstructionPlaying(bool playing) { constructionTimeline_.setPlaying(playing); }
    float constructionProgress() const { return constructionTimeline_.progress(); }
    float constructionSpeed() const { return constructionTimeline_.speed(); }
    const char* constructionStageName() const
    {
        return ConstructionTimelineController::stageName(constructionTimeline_.stage());
    }
    const ShadowSettings& shadowSettings() const { return shadowSettings_; }
    bool animationPaused() const { return animationController_.paused(); }
    bool animationLooping() const { return animationController_.looping(); }
    bool coordinatedAnimationEnabled() const { return coordinatedAnimationEnabled_; }
    float animationSpeed() const { return animationController_.speed(); }
    const char* animationStateName() const
    {
        return ConstructionAnimationController::stateName(animationController_.state());
    }
    const char* demoPoseName() const { return Worker::poseName(demoPose_); }
    const SunState& sunState() const { return sunController_.state(); }
    bool automaticSun() const { return sunController_.automatic(); }
    LightingDebugMode lightingDebugMode() const { return sunController_.debugMode(); }
    const char* lightingDebugModeName() const
    {
        return SunController::debugModeName(sunController_.debugMode());
    }
    glm::vec3 skyColor() const { return sunController_.state().skyColor; }
    glm::vec3 transportTarget() const;

    void toggleFrustumCulling() { frustumCullingEnabled_ = !frustumCullingEnabled_; }
    void setFrustumCullingEnabled(bool enabled) { frustumCullingEnabled_ = enabled; }
    bool frustumCullingEnabled() const { return frustumCullingEnabled_; }
    const RenderStats& renderStats() const { return renderStats_; }
    void printRenderStats(std::ostream& output) const;

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
        float minimumConstructionProgress = 0.0f;
        float maximumConstructionProgress = 1.01f;
    };

    struct StagedSceneObject
    {
        SceneObject object;
        float minimumProgress = 0.0f;
        float maximumProgress = 1.01f;
    };

    struct DynamicPulleyWheel
    {
        glm::mat4 root{1.0f};
        glm::vec3 center{0.0f};
        glm::vec3 scale{1.0f};
        float phase = 0.0f;
        float minimumProgress = 0.0f;
        float maximumProgress = 1.01f;
    };

    struct PyramidInstanceGroup
    {
        MaterialId material = MaterialId::Limestone;
        unsigned int levelChunk = 0;
        InstanceBatch batch;
        std::vector<float> stableThresholds;
        BoundingSphere bounds;
    };

    void addObject(ScenePrimitive primitive, const glm::mat4& model, MaterialId material);
    void addComposite(const glm::mat4& root, const std::vector<ObjectPart>& parts);
    void addWorker(const glm::vec3& position, float rotationY, WorkerPose pose,
                   MaterialId clothing, bool hero, bool secondary,
                   bool demoWorker = false, float phase = 0.0f);
    void addStaticSledge(const glm::vec3& position, float rotationY, float scale = 1.0f);
    void buildGround();
    void buildPyramid();
    void buildPyramidInstanceBatches();
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
    void buildConstructionStages();
    void collectFrameObjects();
    void updateFrontierBatches();
    std::size_t activeStableCount(const PyramidInstanceGroup& group) const;
    float primitiveLocalRadius(ScenePrimitive primitive) const;
    bool objectVisible(const SceneObject& object, const Frustum& frustum,
                       float margin = 0.0f) const;
    const Mesh& meshFor(ScenePrimitive primitive) const;

    Shader shader_;
    Shader instancedShader_;
    Shader depthShader_;
    Shader instancedDepthShader_;
    ShadowMap shadowMap_;
    ShadowSettings shadowSettings_;
    Mesh plane_;
    Mesh cube_;
    Mesh cylinder_;
    Mesh sphere_;
    TextureLibrary textures_;
    PyramidLayoutConfig pyramidConfig_;
    std::vector<PyramidBlockPlacement> pyramidBlocks_;
    std::vector<PyramidInstanceGroup> pyramidInstanceGroups_;
    std::array<InstanceBatch, 2> frontierBatches_;
    std::array<std::vector<InstanceData>, 2> frontierInstances_;
    std::array<BoundingSphere, 2> frontierBounds_{};
    std::vector<SceneObject> objects_;
    std::vector<SceneObject> frameObjects_;
    std::vector<StagedSceneObject> stagedObjects_;
    std::vector<DynamicPulleyWheel> dynamicPulleyWheels_;
    std::vector<WorkerInstance> workers_;
    std::vector<ObjectPart> loadedSledgeParts_;
    ConstructionAnimationController animationController_;
    ConstructionTimelineController constructionTimeline_;
    SunController sunController_;
    WorkerPose demoPose_ = WorkerPose::Standing;
    float articulationTime_ = 0.0f;
    float articulationSpeed_ = 1.0f;
    bool articulationPreviewEnabled_ = true;
    bool coordinatedAnimationEnabled_ = true;
    bool shadowsEnabled_ = true;
    bool texturesEnabled_ = true;
    bool frustumCullingEnabled_ = true;
    ShadowDebugMode shadowDebugMode_ = ShadowDebugMode::Normal;
    StaticGizaSceneStats stats_;
    RenderStats renderStats_;
};
