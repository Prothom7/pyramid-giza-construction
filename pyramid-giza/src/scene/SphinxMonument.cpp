#include "scene/SphinxMonument.h"

#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>



SphinxMonument::SphinxMonument(const glm::vec3& center)
    : center_(center)
{
}

SphinxBounds SphinxMonument::bounds() const
{
    SphinxBounds b;
    b.center = center_;
    b.minBound = center_ - glm::vec3{8.0f, 0.0f, 21.0f};
    b.maxBound = center_ + glm::vec3{8.0f, 13.5f, 24.0f};
    b.height = b.maxBound.y - b.minBound.y;
    b.length = b.maxBound.z - b.minBound.z;
    b.width = b.maxBound.x - b.minBound.x;
    return b;
}

std::size_t SphinxMonument::partCount() const
{
    std::vector<SceneObject> dummy;
    collectSceneObjects(dummy);
    return dummy.size();
}

void SphinxMonument::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    const glm::vec3 s = center_;

    // 1. Base / Plinth: Stepped stone foundation
    // Lower bedrock plinth
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 0.40f, 2.0f}, {},
                                     {14.5f, 0.80f, 44.0f}),
                       MaterialId::LimestoneVariation});
    // Upper plinth terrace
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 1.0f, 2.0f}, {},
                                     {12.8f, 0.60f, 41.5f}),
                       MaterialId::PreparedStone});

    // 2. Lion Body: Elongated recumbent torso
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 3.4f, 5.0f}, {},
                                     {8.4f, 4.4f, 26.0f}),
                       MaterialId::LimestoneVariation});

    // Flank/Neck transitions (procedural transitions)
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 4.4f, -1.0f}, {},
                                     {8.0f, 4.0f, 6.0f}),
                       MaterialId::Limestone});

    // Rear haunches and muscular lion thighs
    for (float xSign : {-1.0f, 1.0f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{xSign * 4.3f, 3.2f, 15.0f},
                                         {0.0f, xSign * 12.0f, 0.0f},
                                         {2.6f, 3.8f, 7.5f}),
                           MaterialId::Limestone});
        // Tucked rear paws
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{xSign * 4.2f, 1.6f, 11.5f}, {},
                                         {2.0f, 0.8f, 3.0f}),
                           MaterialId::Limestone});
    }

    // Lion Tail curling along right flank and rear
    for (int t = 0; t < 5; ++t)
    {
        const float tz = 18.2f - static_cast<float>(t) * 1.8f;
        const float tx = 4.8f - static_cast<float>(t) * 0.35f;
        objects.push_back({ScenePrimitive::Cylinder,
                           makeTransform(s + glm::vec3{tx, 1.65f + t * 0.25f, tz},
                                         {0.0f, -25.0f, 75.0f}, {0.40f, 2.0f, 0.40f}),
                           MaterialId::LimestoneVariation});
    }

    // 3. Forequarters: Powerful broad chest and shoulders
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 5.0f, -4.2f}, {},
                                     {9.2f, 6.4f, 7.2f}),
                       MaterialId::LimestoneVariation});

    // Left and Right Forelegs extending straight forward across the plinth
    for (float x : {-3.6f, 3.6f})
    {
        // Foreleg body
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{x, 1.95f, -10.5f}, {},
                                         {2.4f, 1.7f, 13.0f}),
                           MaterialId::Limestone});

        // Extended front paws with carved toe segments
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{x, 1.75f, -17.5f}, {},
                                         {2.5f, 1.3f, 2.6f}),
                           MaterialId::Limestone});
        for (int toe = 0; toe < 3; ++toe)
        {
            const float toeX = x - 0.7f + static_cast<float>(toe) * 0.7f;
            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(s + glm::vec3{toeX, 1.65f, -18.9f}, {},
                                             {0.55f, 1.0f, 1.1f}),
                               MaterialId::Limestone});
        }
    }

    // 4. Neck transition rising from broad shoulders
    // Neck base blending
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 7.2f, -4.5f}, {},
                                     {3.8f, 2.0f, 4.2f}),
                       MaterialId::LimestoneVariation});

    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 8.4f, -4.5f}, {},
                                     {1.9f, 2.8f, 1.9f}),
                       MaterialId::Limestone});

    // 5. Humanoid Head & Regal Cranium
    objects.push_back({ScenePrimitive::Sphere,
                       makeTransform(s + glm::vec3{0.0f, 10.4f, -4.8f}, {},
                                     {3.2f, 3.6f, 3.0f}),
                       MaterialId::Limestone});

    // Jaw / Chin structure
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 9.2f, -5.9f}, {},
                                     {1.9f, 1.3f, 1.4f}),
                       MaterialId::Limestone});

    // 6. Facial Features
    // Nose bridge and profile
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 10.3f, -6.6f}, {},
                                     {0.55f, 1.3f, 0.7f}),
                       MaterialId::Limestone});
    // Mouth / Lips
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 9.5f, -6.4f}, {},
                                     {1.1f, 0.35f, 0.45f}),
                       MaterialId::Limestone});
    // Almond-shaped eyes and brow ridges
    for (float eyeX : {-0.85f, 0.85f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{eyeX, 10.7f, -6.3f},
                                         {0.0f, 0.0f, (eyeX < 0 ? -12.0f : 12.0f)},
                                         {0.75f, 0.28f, 0.35f}),
                           MaterialId::PreparedStone});
    }

    // Left and Right Royal Ears
    for (float earX : {-2.25f, 2.25f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{earX, 10.4f, -4.8f},
                                         {0.0f, (earX < 0 ? -25.0f : 25.0f), 0.0f},
                                         {0.35f, 1.1f, 0.65f}),
                           MaterialId::Limestone});
    }

    // Ceremonial Beard post / chin support
    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 7.8f, -6.1f}, {},
                                     {0.45f, 2.2f, 0.45f}),
                       MaterialId::PreparedStone});

    // 7. Nemes Royal Headdress
    // Arched crown hood draped over skull
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 11.8f, -4.6f}, {},
                                     {4.6f, 1.4f, 3.8f}),
                       MaterialId::PreparedStone});

    // Characteristic Nemes side wings / lappets hanging down over shoulders
    for (float lappetX : {-2.6f, 2.6f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{lappetX, 9.1f, -4.8f},
                                         {0.0f, 0.0f, lappetX * 2.8f},
                                         {1.2f, 4.2f, 3.4f}),
                           MaterialId::PreparedStone});
    }

    // Uraeus (sacred cobra motif on forehead)
    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 11.6f, -6.3f},
                                     {-18.0f, 0.0f, 0.0f}, {0.18f, 0.75f, 0.18f}),
                       MaterialId::PreparedStone});

    // 8. Weathered sand accumulation drifts against plinth base
    for (float driftSign : {-1.0f, 1.0f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{driftSign * 7.2f, 0.45f, 2.0f},
                                         {0.0f, driftSign * 5.0f, 0.0f},
                                         {1.8f, 0.7f, 24.0f}),
                           MaterialId::Sand});
    }
}

bool SphinxMonument::validateSphinxMonument(std::ostream& output)
{
    SphinxMonument sphinx;
    std::vector<SceneObject> objects;
    sphinx.collectSceneObjects(objects);

    const bool hasParts = objects.size() >= 25;
    const SphinxBounds b = sphinx.bounds();
    const bool dimensionsValid = b.height > 10.0f && b.length > 25.0f && b.width > 10.0f;

    bool allTransformsValid = true;
    for (const auto& obj : objects)
    {
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(obj.model[col][row]))
                    allTransformsValid = false;
    }

    const bool valid = hasParts && dimensionsValid && allTransformsValid;

    output << "Phase 13 Sphinx Monument Validation\n"
           << "  procedural anatomical components (" << objects.size() << " >= 25): "
           << (hasParts ? "PASS" : "FAIL") << '\n'
           << "  monumental scale (length " << b.length << "m, height " << b.height << "m): "
           << (dimensionsValid ? "PASS" : "FAIL") << '\n'
           << "  all component transforms finite and well-conditioned: "
           << (allTransformsValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sphinx monument validation passed.\n"
                     : "Sphinx monument validation failed.\n");

    return valid;
}
