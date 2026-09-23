#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "objects/Worker.h"
#include "scene/PyramidLayout.h"
#include "scene/SceneTypes.h"

struct StaticGizaSceneStats
{
    std::size_t pyramidBlocks = 0;
    std::size_t rampComponents = 0;
    std::size_t quarryBlocks = 0;
    std::size_t stockpileBlocks = 0;
    std::size_t constructionProps = 0;
    std::size_t workerInstances = 0;
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
    void toggleArticulationPreview();
    void cycleDemoPose();
    void resetArticulationPreview();
    bool articulationPreviewEnabled() const { return articulationPreviewEnabled_; }
    const char* demoPoseName() const { return Worker::poseName(demoPose_); }

    const StaticGizaSceneStats& stats() const { return stats_; }
    const PyramidLayoutConfig& pyramidConfig() const { return pyramidConfig_; }

private:
    struct WorkerInstance
    {
        glm::mat4 root{1.0f};
        WorkerPose pose = WorkerPose::Standing;
        WorkerJointAngles jointAngles;
        WorkerStyle style;
        bool isDemoWorker = false;
    };

    void addObject(ScenePrimitive primitive, const glm::mat4& model, MaterialId material);
    void addComposite(const glm::mat4& root, const std::vector<ObjectPart>& parts);
    void buildGround();
    void buildPyramid();
    void buildRamp();
    void buildQuarry();
    void buildStockpile();
    void buildConstructionProps();
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
    WorkerPose demoPose_ = WorkerPose::Standing;
    float articulationTime_ = 0.0f;
    float articulationSpeed_ = 1.0f;
    bool articulationPreviewEnabled_ = true;
    StaticGizaSceneStats stats_;
};
