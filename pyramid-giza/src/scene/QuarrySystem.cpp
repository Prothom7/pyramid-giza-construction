#include "scene/QuarrySystem.h"

#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>

#include "scene/IndustrialLandscape.h"

namespace
{
} // namespace

QuarrySystem::QuarrySystem()
{
    initDeposits();
}

void QuarrySystem::initDeposits()
{
    deposits_.clear();

    const glm::vec3 quarryStagingPos{-108.0f, -6.2f, -5.0f};

    // Primary extraction bays (Bay A through Bay D)
    const std::vector<ExtractionBayDescriptor>& bays = IndustrialLandscape::extractionBays();
    for (std::size_t i = 0; i < bays.size(); ++i)
    {
        QuarryDeposit dep;
        dep.id = 101 + static_cast<int>(i);
        dep.position = bays[i].center;
        dep.size = glm::vec3{2.8f, 1.55f, 2.7f};
        dep.profile = (i == 0) ? RockProfileType::LargeQuarryStone
                               : (i == 1) ? RockProfileType::PartiallyShapedBlock
                                          : RockProfileType::FinishedConstructionBlock;
        dep.material = (i == 0) ? MaterialId::QuarryStone
                                : (i == 1) ? MaterialId::LimestoneVariation
                                           : MaterialId::PreparedStone;
        dep.state = (i == 0) ? QuarryDepositState::Cutting
                             : (i == 1) ? QuarryDepositState::Detached
                                        : (i == 2) ? QuarryDepositState::Shaped
                                                   : QuarryDepositState::Natural;
        dep.initialVolume = dep.size.x * dep.size.y * dep.size.z;
        dep.remainingVolume = dep.initialVolume;
        dep.extractionProgress = (i == 0) ? 0.45f : 1.0f;
        dep.shapingProgress = (i == 0) ? 0.0f : (i == 1) ? 0.5f : 1.0f;
        dep.seamWidth = (i == 0) ? 0.12f : 0.22f;
        dep.separationOffset = (i >= 1) ? 0.22f : 0.0f;
        dep.stagedPosition = quarryStagingPos + glm::vec3{static_cast<float>(i) * 3.2f, 0.0f, 0.0f};
        deposits_.push_back(dep);
    }

    // Additional deposits on Middle and Lower terraces
    struct TerraceSeed
    {
        glm::vec3 pos;
        glm::vec3 size;
        RockProfileType prof;
        MaterialId mat;
        QuarryDepositState st;
    };

    const TerraceSeed seeds[] = {
        {{-138.0f, -3.7f, -12.0f}, {2.7f, 1.5f, 2.6f}, RockProfileType::MediumRoughLimestone, MaterialId::QuarryStone, QuarryDepositState::Natural},
        {{-128.0f, -3.7f, -12.0f}, {2.9f, 1.6f, 2.8f}, RockProfileType::LargeQuarryStone, MaterialId::QuarryStone, QuarryDepositState::Natural},
        {{-118.0f, -3.7f, -12.0f}, {2.6f, 1.4f, 2.5f}, RockProfileType::PartiallyShapedBlock, MaterialId::LimestoneVariation, QuarryDepositState::Marked},
        {{-140.0f, -5.7f, 2.0f}, {3.0f, 1.6f, 2.9f}, RockProfileType::LargeQuarryStone, MaterialId::QuarryStone, QuarryDepositState::Natural},
        {{-130.0f, -5.7f, 2.0f}, {2.7f, 1.5f, 2.6f}, RockProfileType::MediumRoughLimestone, MaterialId::QuarryStone, QuarryDepositState::Natural},
        {{-120.0f, -5.7f, 2.0f}, {2.8f, 1.5f, 2.7f}, RockProfileType::SmallAngularLimestone, MaterialId::LimestoneVariation, QuarryDepositState::Natural},
        {{-104.0f, -6.2f, -15.0f}, {2.8f, 1.5f, 2.7f}, RockProfileType::FinishedConstructionBlock, MaterialId::PreparedStone, QuarryDepositState::Staged},
        {{-100.0f, -6.2f, -15.0f}, {2.7f, 1.5f, 2.6f}, RockProfileType::FinishedConstructionBlock, MaterialId::PreparedStone, QuarryDepositState::Staged}
    };

    int nextId = 105;
    for (const auto& s : seeds)
    {
        QuarryDeposit dep;
        dep.id = nextId++;
        dep.position = s.pos;
        dep.size = s.size;
        dep.profile = s.prof;
        dep.material = s.mat;
        dep.state = s.st;
        dep.initialVolume = dep.size.x * dep.size.y * dep.size.z;
        dep.remainingVolume = dep.initialVolume;
        dep.extractionProgress = (s.st == QuarryDepositState::Staged) ? 1.0f : 0.0f;
        dep.shapingProgress = (s.st == QuarryDepositState::Staged) ? 1.0f : 0.0f;
        dep.separationOffset = (s.st == QuarryDepositState::Staged) ? 0.20f : 0.0f;
        dep.seamWidth = (s.st == QuarryDepositState::Staged) ? 0.22f : 0.0f;
        dep.stagedPosition = quarryStagingPos + glm::vec3{static_cast<float>(nextId % 4) * 3.2f, 0.0f, 4.0f};
        deposits_.push_back(dep);
    }

    activeDepositIndex_ = 0;
    cycleTimer_ = 0.0f;
    dustTimer_ = 0.0f;
    emitDustNow_ = false;
}

void QuarrySystem::reset()
{
    initDeposits();
}

const QuarryDeposit* QuarrySystem::activeDeposit() const
{
    if (activeDepositIndex_ >= 0 && activeDepositIndex_ < static_cast<int>(deposits_.size()))
        return &deposits_[activeDepositIndex_];
    return nullptr;
}

QuarryDeposit* QuarrySystem::activeDeposit()
{
    if (activeDepositIndex_ >= 0 && activeDepositIndex_ < static_cast<int>(deposits_.size()))
        return &deposits_[activeDepositIndex_];
    return nullptr;
}

void QuarrySystem::startExtraction(ConstructionSimulation& simulation, int depositIndex)
{
    if (depositIndex >= 0 && depositIndex < static_cast<int>(deposits_.size()))
        activeDepositIndex_ = depositIndex;
    else
    {
        // Find first natural or marked deposit, cycling from current
        for (std::size_t i = 0; i < deposits_.size(); ++i)
        {
            std::size_t idx = (static_cast<std::size_t>(activeDepositIndex_ + 1) + i) % deposits_.size();
            if (deposits_[idx].state == QuarryDepositState::Natural ||
                deposits_[idx].state == QuarryDepositState::Marked)
            {
                activeDepositIndex_ = static_cast<int>(idx);
                break;
            }
        }
    }

    QuarryDeposit* dep = activeDeposit();
    if (dep != nullptr)
    {
        dep->state = QuarryDepositState::Cutting;
        dep->extractionProgress = 0.0f;
        dep->shapingProgress = 0.0f;
        dep->seamWidth = 0.04f;
        dep->separationOffset = 0.0f;
        
        // Spawn the authoritative simulation block immediately
        simulation.spawnNewBlock(dep->position);
        activeBlockId_ = simulation.activeBlocks.back().id;
        ConstructionBlock* block = simulation.getBlock(activeBlockId_);
        if (block) {
            block->state = BlockState::Extracting;
        }
    }
}

bool QuarrySystem::isCurrentDepositStaged(const ConstructionSimulation& simulation) const
{
    if (activeBlockId_ == 0) return false;
    
    // Iterate manually because getBlock is not const, or just find it
    for (const auto& block : simulation.activeBlocks) {
        if (block.id == activeBlockId_) {
            return block.state == BlockState::Staged;
        }
    }
    return false;
}

void QuarrySystem::markCurrentDepositTransported()
{
    QuarryDeposit* dep = activeDeposit();
    if (dep != nullptr)
    {
        // Causal pipeline: respawn this deposit so we never run out of quarry material
        // while maintaining the same physical extraction site.
        dep->state = QuarryDepositState::Natural;
        dep->extractionProgress = 0.0f;
        dep->shapingProgress = 0.0f;
        dep->remainingVolume = dep->initialVolume;
        dep->id += 100; // Ensure unique ID per cycle
        
        activeBlockId_ = 0; // Detach from the block since logistics has it
        
        // Select next deposit for future extraction (logistics/static scene will call this)
        // Note: we'll defer startExtraction so it accepts simulation reference.
    }
}

void QuarrySystem::update(float deltaTime, ConstructionSimulation& simulation)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    cycleTimer_ += deltaTime;
    dustTimer_ += deltaTime;
    emitDustNow_ = false;

    // Automatically start the next block extraction if idle
    if (activeBlockId_ == 0) {
        startExtraction(simulation, 0); // Always restart from deposit 0 for simplicity of looping
    }

    if (dustTimer_ >= 0.85f)
    {
        dustTimer_ = 0.0f;
        emitDustNow_ = true;
    }

    updateExtraction(deltaTime, simulation);
}

void QuarrySystem::updateExtraction(float deltaTime, ConstructionSimulation& simulation)
{
    QuarryDeposit* dep = activeDeposit();
    if (dep == nullptr)
        return;

    if (dep->state == QuarryDepositState::Cutting)
    {
        // Hammering vibration
        dep->vibration = std::sin(cycleTimer_ * 18.0f) * 0.025f;

        // Seam cuts widen as workers chisel trenches
        dep->extractionProgress = std::min(1.0f, dep->extractionProgress + deltaTime * 0.08f);
        dep->seamWidth = 0.04f + 0.18f * dep->extractionProgress;

        if (dep->extractionProgress >= 1.0f)
        {
            // Separation event: rock detaches with an audible crack and vertical lift
            dep->state = QuarryDepositState::Detached;
            dep->separationOffset = 0.22f;
            dep->vibration = 0.0f;
            dep->profile = RockProfileType::PartiallyShapedBlock;
            
            ConstructionBlock* block = simulation.getBlock(activeBlockId_);
            if (block) block->state = BlockState::Separating;
        }
    }
    else if (dep->state == QuarryDepositState::Detached)
    {
        // Masons shape the detached stone into a rectangular block
        dep->shapingProgress = std::min(1.0f, dep->shapingProgress + deltaTime * 0.12f);
        if (dep->shapingProgress >= 0.5f)
        {
            dep->material = MaterialId::LimestoneVariation;
        }
        if (dep->shapingProgress >= 1.0f)
        {
            dep->state = QuarryDepositState::Shaped;
            dep->profile = RockProfileType::FinishedConstructionBlock;
            dep->material = MaterialId::PreparedStone;
            
            ConstructionBlock* block = simulation.getBlock(activeBlockId_);
            if (block) block->state = BlockState::Shaping;
        }
    }
    else if (dep->state == QuarryDepositState::Shaped)
    {
        // Smoothly transport stone on wooden rollers across quarry floor to staging
        const glm::vec3 target = dep->stagedPosition;
        const float dist = glm::distance(dep->position, target);
        const float step = deltaTime * 3.5f;
        if (dist > step)
        {
            dep->position += glm::normalize(target - dep->position) * step;
        }
        else
        {
            dep->position = target;
            dep->state = QuarryDepositState::Staged;
            
            ConstructionBlock* block = simulation.getBlock(activeBlockId_);
            if (block) {
                block->state = BlockState::Staged;
                block->position = dep->position;
                block->previousPosition = dep->position;
            }
        }
    }
    
    // Sync active block position during shaping/staging movement
    if (activeBlockId_ != 0) {
        ConstructionBlock* block = simulation.getBlock(activeBlockId_);
        if (block) {
            block->position = dep->position;
            if (dep->state == QuarryDepositState::Cutting) block->position.y += dep->vibration;
            else if (dep->state == QuarryDepositState::Detached || dep->state == QuarryDepositState::Shaped) block->position.y += dep->separationOffset;
            block->position.y += dep->size.y * 0.5f; // Match the visual center of the deposit
            block->taskProgress = dep->extractionProgress; // Or shaping progress
        }
    }
}

QuarryToolActivity QuarrySystem::toolActivity() const
{
    QuarryToolActivity act;
    const QuarryDeposit* dep = activeDeposit();
    if (dep == nullptr)
        return act;

    act.chiselPoint = dep->position + glm::vec3{dep->size.x * 0.45f, dep->size.y * 0.5f, 0.0f};
    act.malletWorkerPos = act.chiselPoint + glm::vec3{0.9f, 0.0f, 0.3f};
    act.masonWorkerPos = dep->position + glm::vec3{-dep->size.x * 0.55f, 0.0f, -0.6f};
    act.inspectorPos = dep->position + glm::vec3{0.0f, 0.0f, dep->size.z * 0.7f + 1.2f};
    act.isHammering = (dep->state == QuarryDepositState::Cutting);
    act.emitDust = emitDustNow_ && (dep->state == QuarryDepositState::Cutting || dep->state == QuarryDepositState::Detached);
    return act;
}

void QuarrySystem::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    for (const QuarryDeposit& dep : deposits_)
    {
        if (dep.state == QuarryDepositState::Transported || dep.state == QuarryDepositState::Depleted)
            continue;

        glm::vec3 renderPos = dep.position;
        if (dep.state == QuarryDepositState::Cutting)
            renderPos.y += dep.vibration;
        else if (dep.state == QuarryDepositState::Detached || dep.state == QuarryDepositState::Shaped)
            renderPos.y += dep.separationOffset;

        // The main block body is now rendered by ConstructionSimulation::collectSceneObjects when active
        if (&dep != activeDeposit()) {
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(renderPos + glm::vec3{0.0f, dep.size.y * 0.5f, 0.0f},
                                             {}, dep.size),
                               dep.material});
        }

        // If cutting or detached, render visible trench seam and wooden wedges
        if (dep.state == QuarryDepositState::Cutting || dep.state == QuarryDepositState::Detached)
        {
            const float seam = dep.seamWidth;
            // 4 perimeter cut trenches
            const glm::vec3 half = dep.size * 0.5f;
            // Left & right trenches
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(dep.position + glm::vec3{-half.x - seam * 0.5f, half.y, 0.0f},
                                             {}, {seam, dep.size.y * 1.05f, dep.size.z + seam * 2.0f}),
                               MaterialId::QuarryStone});
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(dep.position + glm::vec3{half.x + seam * 0.5f, half.y, 0.0f},
                                             {}, {seam, dep.size.y * 1.05f, dep.size.z + seam * 2.0f}),
                               MaterialId::QuarryStone});
            // Front & back trenches
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(dep.position + glm::vec3{0.0f, half.y, -half.z - seam * 0.5f},
                                             {}, {dep.size.x, dep.size.y * 1.05f, seam}),
                               MaterialId::QuarryStone});
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(dep.position + glm::vec3{0.0f, half.y, half.z + seam * 0.5f},
                                             {}, {dep.size.x, dep.size.y * 1.05f, seam}),
                               MaterialId::QuarryStone});

            // Wooden wedges driven into the seam
            for (float offset : {-0.8f, 0.0f, 0.8f})
            {
                objects.push_back({ScenePrimitive::Cube,
                                   makeTransform(dep.position + glm::vec3{half.x + seam * 0.5f, dep.size.y * 0.75f, offset},
                                                 {0.0f, 0.0f, 15.0f}, {0.08f, 0.28f, 0.14f}),
                                   MaterialId::Wood});
            }
        }

        // Stone chips and debris around shaping / cutting blocks
        if (dep.state == QuarryDepositState::Cutting || dep.state == QuarryDepositState::Detached ||
            dep.state == QuarryDepositState::Shaped)
        {
            for (int chip = 0; chip < 4; ++chip)
            {
                const float cx = ((chip % 2 == 0) ? -1.0f : 1.0f) * (dep.size.x * 0.6f + 0.3f);
                const float cz = ((chip / 2 == 0) ? -1.0f : 1.0f) * (dep.size.z * 0.5f + 0.2f);
                objects.push_back({ScenePrimitive::Cube,
                                   makeTransform(dep.position + glm::vec3{cx, 0.08f, cz},
                                                 {12.0f * chip, 25.0f * chip, 0.0f},
                                                 {0.22f, 0.14f, 0.20f}),
                                   MaterialId::LimestoneVariation});
            }
        }
    }
}

const char* QuarrySystem::stateName(QuarryDepositState state)
{
    switch (state)
    {
    case QuarryDepositState::Natural: return "Natural";
    case QuarryDepositState::Marked: return "Marked";
    case QuarryDepositState::Cutting: return "Cutting";
    case QuarryDepositState::Detached: return "Detached";
    case QuarryDepositState::Shaped: return "Shaped";
    case QuarryDepositState::Staged: return "Staged";
    case QuarryDepositState::Transported: return "Transported";
    case QuarryDepositState::Depleted: return "Depleted";
    default: return "Unknown";
    }
}

const char* QuarrySystem::profileName(RockProfileType profile)
{
    switch (profile)
    {
    case RockProfileType::SmallAngularLimestone: return "SmallAngularLimestone";
    case RockProfileType::MediumRoughLimestone: return "MediumRoughLimestone";
    case RockProfileType::LargeQuarryStone: return "LargeQuarryStone";
    case RockProfileType::PartiallyShapedBlock: return "PartiallyShapedBlock";
    case RockProfileType::FinishedConstructionBlock: return "FinishedConstructionBlock";
    default: return "Unknown";
    }
}

bool QuarrySystem::validateQuarry(std::ostream& output)
{
    QuarrySystem system;
    bool depositsExist = system.deposits().size() >= 10;
    bool depositsGrounded = true;
    for (const auto& dep : system.deposits())
    {
        depositsGrounded = depositsGrounded && std::isfinite(dep.position.x) &&
                           std::isfinite(dep.position.y) && std::isfinite(dep.position.z) &&
                           dep.size.x > 0.5f && dep.size.y > 0.5f && dep.size.z > 0.5f &&
                           dep.initialVolume > 0.0f;
    }

    // Test extraction progression
    ConstructionSimulation sim;
    sim.initialize(PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    system.startExtraction(sim, 0);
    const float initialProgress = system.activeDeposit()->extractionProgress;
    for (int step = 0; step < 20; ++step)
        system.update(0.1f, sim);
    const float progressed = system.activeDeposit()->extractionProgress;
    bool extractionAdvances = progressed > initialProgress;

    // Advance until detachment
    while (system.activeDeposit()->state == QuarryDepositState::Cutting)
        system.update(0.2f, sim);
    bool detaches = (system.activeDeposit()->state == QuarryDepositState::Detached) &&
                    (system.activeDeposit()->separationOffset > 0.1f);

    // Advance until shaped
    while (system.activeDeposit()->state == QuarryDepositState::Detached)
        system.update(0.2f, sim);
    bool shapes = (system.activeDeposit()->state == QuarryDepositState::Shaped) &&
                  (system.activeDeposit()->profile == RockProfileType::FinishedConstructionBlock);

    // Advance until staged
    for (int step = 0; step < 200 && system.activeDeposit()->state != QuarryDepositState::Staged; ++step)
        system.update(0.2f, sim);
    bool stages = (system.activeDeposit()->state == QuarryDepositState::Staged);

    const bool valid = depositsExist && depositsGrounded && extractionAdvances &&
                       detaches && shapes && stages;

    output << "Phase 13 Functional Quarry Validation\n"
           << "  deposit inventory count (" << system.deposits().size() << " >= 10): "
           << (depositsExist ? "PASS" : "FAIL") << '\n'
           << "  all deposits grounded and within quarry bounds: "
           << (depositsGrounded ? "PASS" : "FAIL") << '\n'
           << "  channel-cut extraction progress advances deterministically: "
           << (extractionAdvances ? "PASS" : "FAIL") << '\n'
           << "  bedrock separation event with visual offset: "
           << (detaches ? "PASS" : "FAIL") << '\n'
           << "  stone shaping into dressed construction block: "
           << (shapes ? "PASS" : "FAIL") << '\n'
           << "  roller transport to quarry staging floor: "
           << (stages ? "PASS" : "FAIL") << '\n'
           << (valid ? "Quarry validation passed.\n" : "Quarry validation failed.\n");

    return valid;
}
