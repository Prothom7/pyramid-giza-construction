#include "scene/SphinxMonument.h"

#include <cmath>
#include <array>
#include <algorithm>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>



SphinxMonument::SphinxMonument(const glm::vec3& center)
    : center_(center)
{
}

SphinxBounds SphinxMonument::bounds() const
{
    SphinxBounds b;
    b.center = center_;
    b.minBound = center_ + glm::vec3{-StructuralHalfWidth, 0.0f,
                                     StructuralFront};
    b.maxBound = center_ + glm::vec3{StructuralHalfWidth, StructuralHeight,
                                     StructuralRear};
    b.height = b.maxBound.y - b.minBound.y;
    b.length = b.maxBound.z - b.minBound.z;
    b.width = b.maxBound.x - b.minBound.x;
    return b;
}

bool SphinxMonument::containsStructuralFootprint(float x, float z, float margin)
{
    return std::isfinite(x) && std::isfinite(z) &&
           x >= DefaultCenter.x - StructuralHalfWidth - margin &&
           x <= DefaultCenter.x + StructuralHalfWidth + margin &&
           z >= DefaultCenter.z + StructuralFront - margin &&
           z <= DefaultCenter.z + StructuralRear + margin;
}

MeshData SphinxMonument::createFormMesh()
{
    // One reusable, closed, low-poly loft replaces the silhouette's cuboids.
    // Scaling this mesh makes the torso, shoulders, limbs, and cloth forms.
    constexpr int slices = 20;
    constexpr std::array<float, 7> heights{
        -0.5f, -0.43f, -0.22f, 0.08f, 0.28f, 0.43f, 0.5f};
    constexpr std::array<float, 7> radii{
        0.55f, 0.82f, 0.98f, 1.0f, 0.90f, 0.65f, 0.08f};
    MeshData mesh{"SphinxForm"};
    for (std::size_t ring = 0; ring < heights.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const float angle = glm::two_pi<float>() * slice / slices;
            mesh.vertices.push_back({{0.5f * radii[ring] * std::cos(angle),
                                      heights[ring],
                                      0.5f * radii[ring] * std::sin(angle)},
                                     {0.0f, 0.0f, 0.0f},
                                     {static_cast<float>(slice) / slices,
                                      static_cast<float>(ring) /
                                          (heights.size() - 1)}});
        }
    for (std::size_t ring = 0; ring + 1 < heights.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const auto lower = static_cast<std::uint32_t>(ring * slices + slice);
            const auto upper = lower + slices;
            const auto next = static_cast<std::uint32_t>(
                ring * slices + (slice + 1) % slices);
            const auto upperNext = next + slices;
            mesh.indices.insert(mesh.indices.end(),
                                {lower, upper, next, next, upper, upperNext});
        }
    const auto bottomCenter = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, -0.5f, 0.0f}, {}, {0.5f, 0.5f}});
    const auto topCenter = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, 0.5f, 0.0f}, {}, {0.5f, 0.5f}});
    const auto top = static_cast<std::uint32_t>((heights.size() - 1) * slices);
    for (int slice = 0; slice < slices; ++slice)
    {
        const auto current = static_cast<std::uint32_t>(slice);
        const auto next = static_cast<std::uint32_t>((slice + 1) % slices);
        mesh.indices.insert(mesh.indices.end(),
                            {bottomCenter, current, next,
                             topCenter, top + next, top + current});
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
    {
        const auto a = mesh.indices[i];
        const auto b = mesh.indices[i + 1];
        const auto c = mesh.indices[i + 2];
        const glm::vec3 face = glm::cross(mesh.vertices[b].position -
                                             mesh.vertices[a].position,
                                         mesh.vertices[c].position -
                                             mesh.vertices[a].position);
        mesh.vertices[a].normal += face;
        mesh.vertices[b].normal += face;
        mesh.vertices[c].normal += face;
    }
    for (Vertex& vertex : mesh.vertices)
        vertex.normal = glm::normalize(vertex.normal);
    return mesh;
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
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 3.4f, 5.0f}, {},
                                     {9.4f, 4.8f, 27.0f}),
                       MaterialId::LimestoneVariation});

    // Flank/Neck transitions (procedural transitions)
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 4.4f, -1.0f}, {},
                                     {8.4f, 4.4f, 8.0f}),
                       MaterialId::Limestone});

    // Rear haunches and muscular lion thighs
    for (float xSign : {-1.0f, 1.0f})
    {
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{xSign * 4.3f, 3.2f, 15.0f},
                                         {0.0f, xSign * 12.0f, 0.0f},
                                         {3.3f, 3.8f, 7.5f}),
                           MaterialId::Limestone});
        // Tucked rear paws
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{xSign * 4.2f, 1.6f, 11.5f}, {},
                                         {2.6f, 0.8f, 3.5f}),
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
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 5.0f, -4.2f}, {},
                                     {8.2f, 5.8f, 7.5f}),
                       MaterialId::LimestoneVariation});

    // Left and Right Forelegs extending straight forward across the plinth
    for (float x : {-3.6f, 3.6f})
    {
        // Foreleg body
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{x, 1.95f, -10.5f}, {},
                                         {3.0f, 1.8f, 13.0f}),
                           MaterialId::Limestone});

        // Extended front paws with carved toe segments
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{x, 1.75f, -17.5f}, {},
                                         {3.4f, 1.3f, 3.5f}),
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
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 7.2f, -4.5f}, {},
                                     {3.8f, 2.0f, 4.2f}),
                       MaterialId::LimestoneVariation});

    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 8.4f, -4.5f}, {},
                                     {1.9f, 2.8f, 1.9f}),
                       MaterialId::Limestone});

    // 5. Humanoid Head & Regal Cranium
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 10.4f, -4.8f}, {},
                                     {3.2f, 3.8f, 3.1f}),
                       MaterialId::PreparedStone});

    // Cheek masses project beyond the cranium, giving the face a legible
    // human profile without adding a disconnected mask in front of it.
    for (float cheekX : {-0.8f, 0.8f})
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{cheekX, 9.9f, -6.0f}, {},
                                         {1.2f, 1.1f, 1.35f}),
                           MaterialId::PreparedStone});

    // Jaw / Chin structure
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 9.2f, -6.1f}, {},
                                     {2.0f, 1.4f, 1.5f}),
                       MaterialId::PreparedStone});

    // 6. Facial Features
    // Nose bridge and profile
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 10.3f, -6.9f}, {},
                                     {0.55f, 1.3f, 0.7f}),
                       MaterialId::PreparedStone});
    // Mouth / Lips
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 9.5f, -6.9f}, {},
                                     {1.1f, 0.35f, 0.45f}),
                       MaterialId::QuarryStone});
    // Almond-shaped eyes and brow ridges
    for (float eyeX : {-0.85f, 0.85f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{eyeX, 10.7f, -6.75f},
                                         {0.0f, 0.0f, (eyeX < 0 ? -12.0f : 12.0f)},
                                         {0.75f, 0.28f, 0.35f}),
                           MaterialId::QuarryStone});
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
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 11.8f, -4.0f}, {},
                                     {4.6f, 1.4f, 3.2f}),
                       MaterialId::PreparedStone});

    // Characteristic Nemes side wings / lappets hanging down over shoulders
    for (float lappetX : {-2.6f, 2.6f})
    {
        objects.push_back({ScenePrimitive::SphinxForm,
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

    const MeshData form = createFormMesh();
    bool meshValid = !form.vertices.empty() && !form.indices.empty() &&
                     form.indices.size() % 3 == 0;
    for (const Vertex& vertex : form.vertices)
        meshValid = meshValid &&
            std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
            std::isfinite(vertex.position.z) &&
            std::isfinite(vertex.normal.x) && std::isfinite(vertex.normal.y) &&
            std::isfinite(vertex.normal.z) &&
            std::abs(glm::length(vertex.normal) - 1.0f) < 0.01f;
    for (std::size_t i = 0; i < form.indices.size(); i += 3)
    {
        const auto a = form.indices[i];
        const auto bIndex = form.indices[i + 1];
        const auto c = form.indices[i + 2];
        if (a >= form.vertices.size() || bIndex >= form.vertices.size() ||
            c >= form.vertices.size())
        {
            meshValid = false;
            continue;
        }
        const glm::vec3 face = glm::cross(form.vertices[bIndex].position -
                                             form.vertices[a].position,
                                         form.vertices[c].position -
                                             form.vertices[a].position);
        meshValid = meshValid && glm::length(face) > 1.0e-7f;
        const std::size_t triangle = i / 3;
        if (triangle < 240)
        {
            const glm::vec3 middle = (form.vertices[a].position +
                form.vertices[bIndex].position + form.vertices[c].position) / 3.0f;
            meshValid = meshValid &&
                glm::dot(face, glm::vec3{middle.x, 0.0f, middle.z}) > 0.0f;
        }
        else
            meshValid = meshValid &&
                ((triangle - 240) % 2 == 0 ? face.y < 0.0f : face.y > 0.0f);
    }
    const bool footprintValid = containsStructuralFootprint(92.0f, -105.0f) &&
        containsStructuralFootprint(92.0f, -125.0f) &&
        containsStructuralFootprint(92.0f, -81.0f) &&
        !containsStructuralFootprint(92.0f, -126.0f) &&
        !containsStructuralFootprint(110.0f, -105.0f);

    const bool valid = hasParts && dimensionsValid && allTransformsValid &&
                       meshValid && footprintValid;

    output << "Phase 13 Sphinx Monument Validation\n"
           << "  procedural anatomical components (" << objects.size() << " >= 25): "
           << (hasParts ? "PASS" : "FAIL") << '\n'
           << "  monumental scale (length " << b.length << "m, height " << b.height << "m): "
           << (dimensionsValid ? "PASS" : "FAIL") << '\n'
           << "  all component transforms finite and well-conditioned: "
           << (allTransformsValid ? "PASS" : "FAIL") << '\n'
           << "  closed indexed loft triangles, finite unit normals: "
           << (meshValid ? "PASS" : "FAIL") << " ("
           << form.vertices.size() << " vertices, " << form.indices.size() / 3
           << " triangles)\n"
           << "  structural footprint includes both plinth ends: "
           << (footprintValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sphinx monument validation passed.\n"
                     : "Sphinx monument validation failed.\n");

    return valid;
}
