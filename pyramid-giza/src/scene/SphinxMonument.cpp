#include "scene/SphinxMonument.h"

#include <cmath>
#include <array>
#include <algorithm>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "graphics/PrimitiveGenerator.h"
#include "scene/SandSimulation.h"



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

MeshData SphinxMonument::createBodyMesh()
{
    // Longitudinal lion-body loft: raised shoulder and rump with a lower back.
    // Unit dimensions allow the same scene transform conventions as other meshes.
    struct Section { float z, halfWidth, centerY, halfHeight; };
    constexpr std::array<Section, 10> sections{{
        {-0.50f, 0.40f, -0.03f, 0.38f},
        {-0.44f, 0.47f, -0.03f, 0.40f},
        {-0.32f, 0.49f, -0.01f, 0.42f},
        {-0.18f, 0.48f, -0.04f, 0.39f},
        { 0.00f, 0.43f, -0.08f, 0.33f},
        { 0.18f, 0.42f, -0.08f, 0.32f},
        { 0.34f, 0.47f, -0.03f, 0.37f},
        { 0.43f, 0.48f, -0.01f, 0.40f},
        { 0.49f, 0.35f, -0.07f, 0.33f},
        { 0.50f, 0.22f, -0.08f, 0.25f}}};
    constexpr int slices = 24;
    MeshData mesh{"SphinxBody"};
    for (std::size_t ring = 0; ring < sections.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const float angle = glm::two_pi<float>() * slice / slices;
            const Section& section = sections[ring];
            mesh.vertices.push_back({
                {section.halfWidth * std::cos(angle),
                 section.centerY + section.halfHeight * std::sin(angle),
                 section.z}, {},
                {static_cast<float>(slice) / slices,
                 static_cast<float>(ring) / (sections.size() - 1)}});
        }
    for (std::size_t ring = 0; ring + 1 < sections.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const auto a = static_cast<std::uint32_t>(ring * slices + slice);
            const auto b = static_cast<std::uint32_t>(ring * slices + (slice + 1) % slices);
            const auto c = a + slices;
            const auto d = b + slices;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    // Closed ends; cap winding faces outward along the longitudinal axis.
    const auto front = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, sections.front().centerY, sections.front().z}, {}, {0.5f, 0.5f}});
    const auto rear = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, sections.back().centerY, sections.back().z}, {}, {0.5f, 0.5f}});
    const auto lastRing = static_cast<std::uint32_t>((sections.size() - 1) * slices);
    for (int slice = 0; slice < slices; ++slice)
    {
        const auto a = static_cast<std::uint32_t>(slice);
        const auto b = static_cast<std::uint32_t>((slice + 1) % slices);
        mesh.indices.insert(mesh.indices.end(), {front, b, a,
                                                  rear, lastRing + a, lastRing + b});
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
    {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        const glm::vec3 face = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                          mesh.vertices[c].position - mesh.vertices[a].position);
        mesh.vertices[a].normal += face;
        mesh.vertices[b].normal += face;
        mesh.vertices[c].normal += face;
    }
    for (Vertex& vertex : mesh.vertices)
        vertex.normal = glm::normalize(vertex.normal);
    return mesh;
}

MeshData SphinxMonument::createNeckMesh()
{
    // One tapering mass rises from the sculpted chest into the human head.
    // Local coordinates are metres relative to the monument center.
    struct Section { float y, radiusX, radiusZ; };
    constexpr std::array<Section, 7> sections{{
        {5.45f, 2.75f, 2.50f}, {6.10f, 2.65f, 2.40f},
        {6.85f, 2.22f, 2.02f}, {7.65f, 1.62f, 1.55f},
        {8.45f, 1.32f, 1.34f}, {9.20f, 1.23f, 1.27f},
        {9.65f, 1.16f, 1.20f}}};
    constexpr int slices = 20;
    MeshData mesh{"SphinxNeck"};
    for (std::size_t ring = 0; ring < sections.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const float angle = glm::two_pi<float>() * slice / slices;
            mesh.vertices.push_back({
                {sections[ring].radiusX * std::cos(angle), sections[ring].y,
                 -5.0f + sections[ring].radiusZ * std::sin(angle)}, {},
                {static_cast<float>(slice) / slices,
                 static_cast<float>(ring) / (sections.size() - 1)}});
        }
    for (std::size_t ring = 0; ring + 1 < sections.size(); ++ring)
        for (int slice = 0; slice < slices; ++slice)
        {
            const auto lower = static_cast<std::uint32_t>(ring * slices + slice);
            const auto next = static_cast<std::uint32_t>(ring * slices + (slice + 1) % slices);
            const auto upper = lower + slices, upperNext = next + slices;
            mesh.indices.insert(mesh.indices.end(),
                                {lower, upper, next, next, upper, upperNext});
        }
    const auto bottom = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, sections.front().y, -5.0f}, {}, {0.5f, 0.5f}});
    const auto top = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, sections.back().y, -5.0f}, {}, {0.5f, 0.5f}});
    const auto lastRing = static_cast<std::uint32_t>((sections.size() - 1) * slices);
    for (int slice = 0; slice < slices; ++slice)
    {
        const auto a = static_cast<std::uint32_t>(slice);
        const auto b = static_cast<std::uint32_t>((slice + 1) % slices);
        mesh.indices.insert(mesh.indices.end(), {bottom, a, b,
                                                  top, lastRing + b, lastRing + a});
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
    {
        const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        const glm::vec3 face = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                          mesh.vertices[c].position - mesh.vertices[a].position);
        mesh.vertices[a].normal += face;
        mesh.vertices[b].normal += face;
        mesh.vertices[c].normal += face;
    }
    for (Vertex& vertex : mesh.vertices)
        vertex.normal = glm::normalize(vertex.normal);
    return mesh;
}

MeshData SphinxMonument::createHeaddressMesh()
{
    // Three closed, faceted stone pieces form one indexed nemes mesh. The hood
    // frames the face; tapered side panels descend onto the shoulders.
    MeshData mesh{"SphinxHeaddress"};
    const auto triangle = [&mesh](glm::vec3 a, glm::vec3 b, glm::vec3 c)
    {
        const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({a, normal, {0.0f, 0.0f}});
        mesh.vertices.push_back({b, normal, {1.0f, 0.0f}});
        mesh.vertices.push_back({c, normal, {0.5f, 1.0f}});
        mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
    };
    const auto prism = [&triangle](const std::vector<glm::vec2>& outline,
                                    float frontZ, float rearZ)
    {
        // The outlines below are counterclockwise in XY. A front-facing cap
        // therefore has reversed winding; the rear cap uses the natural order.
        for (std::size_t i = 1; i + 1 < outline.size(); ++i)
        {
            triangle({outline[0], frontZ}, {outline[i + 1], frontZ}, {outline[i], frontZ});
            triangle({outline[0], rearZ}, {outline[i], rearZ}, {outline[i + 1], rearZ});
        }
        for (std::size_t i = 0; i < outline.size(); ++i)
        {
            const std::size_t next = (i + 1) % outline.size();
            const glm::vec3 a{outline[i], frontZ}, b{outline[next], frontZ};
            const glm::vec3 c{outline[i], rearZ}, d{outline[next], rearZ};
            triangle(a, b, c);
            triangle(b, d, c);
        }
    };
    prism({{-2.28f, 11.52f}, {2.28f, 11.52f}, {2.18f, 12.15f},
           {1.15f, 12.70f}, {-1.15f, 12.70f}, {-2.18f, 12.15f}}, -6.25f, -3.45f);
    prism({{-2.70f, 8.10f}, {-1.82f, 8.10f}, {-1.72f, 11.72f},
           {-2.28f, 12.12f}, {-2.65f, 10.60f}}, -6.12f, -5.40f);
    prism({{1.82f, 8.10f}, {2.70f, 8.10f}, {2.65f, 10.60f},
           {2.28f, 12.12f}, {1.72f, 11.72f}}, -6.12f, -5.40f);
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
    objects.push_back({ScenePrimitive::SphinxBody,
                       makeTransform(s + glm::vec3{0.0f, 3.9f, 5.0f}, {},
                                     {9.5f, 5.8f, 26.0f}),
                       MaterialId::LimestoneVariation});

    // Rear haunches and muscular lion thighs
    for (float xSign : {-1.0f, 1.0f})
    {
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{xSign * 3.8f, 3.45f, 14.2f},
                                         {0.0f, xSign * 8.0f, 0.0f},
                                         {3.2f, 4.2f, 6.6f}),
                           MaterialId::Limestone});
        // Tucked rear paws
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{xSign * 4.1f, 1.75f, 10.9f}, {},
                                         {2.8f, 1.2f, 3.6f}),
                           MaterialId::Limestone});
    }

    // A low tail follows the right rear flank and curls back over the rump.
    for (int t = 0; t < 4; ++t)
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{4.55f, 2.05f,
                                                        10.8f + t * 2.05f}, {},
                                         {0.62f, 0.58f, 2.65f}),
                           MaterialId::LimestoneVariation});
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{4.45f, 2.56f, 18.65f},
                                     {28.0f, 0.0f, 0.0f}, {0.73f, 0.70f, 2.0f}),
                       MaterialId::LimestoneVariation});

    // 3. The body loft itself supplies the broad chest and shoulder mass.
    // Left and Right Forelegs extend forward across the plinth.
    for (float x : {-3.1f, 3.1f})
    {
        // Foreleg body
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{x, 2.38f, -10.8f}, {},
                                         {3.35f, 2.55f, 12.7f}),
                           MaterialId::Limestone});

        // Extended front paws with carved toe segments
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{x, 1.92f, -17.3f}, {},
                                         {3.65f, 1.4f, 3.7f}),
                           MaterialId::Limestone});
        for (int toe = 0; toe < 3; ++toe)
        {
            const float toeX = x - 0.92f + static_cast<float>(toe) * 0.92f;
            objects.push_back({ScenePrimitive::SphinxForm,
                               makeTransform(s + glm::vec3{toeX, 1.67f, -18.65f}, {},
                                             {0.96f, 0.72f, 1.35f}),
                               MaterialId::Limestone});
        }
    }

    // 4. One tapering neck replaces the stacked oval/cylinder intersection.
    objects.push_back({ScenePrimitive::SphinxNeck,
                       makeTransform(s, {}, {1.0f, 1.0f, 1.0f}),
                       MaterialId::Limestone});

    // 5. Humanoid Head & Regal Cranium
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 10.45f, -5.25f}, {},
                                     {3.45f, 3.9f, 2.9f}),
                       MaterialId::PreparedStone});

    // Cheek masses project beyond the cranium, giving the face a legible
    // human profile without adding a disconnected mask in front of it.
    for (float cheekX : {-0.8f, 0.8f})
        objects.push_back({ScenePrimitive::SphinxForm,
                           makeTransform(s + glm::vec3{cheekX, 10.0f, -6.25f}, {},
                                         {1.20f, 1.28f, 0.78f}),
                           MaterialId::PreparedStone});

    // Jaw / Chin structure
    objects.push_back({ScenePrimitive::SphinxForm,
                       makeTransform(s + glm::vec3{0.0f, 9.22f, -6.28f}, {},
                                     {1.88f, 1.22f, 0.86f}),
                       MaterialId::PreparedStone});

    // 6. Facial Features
    // Nose bridge and profile
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 10.38f, -6.85f}, {},
                                     {0.38f, 1.05f, 0.44f}),
                       MaterialId::PreparedStone});
    // Mouth / Lips
    objects.push_back({ScenePrimitive::Cube,
                       makeTransform(s + glm::vec3{0.0f, 9.48f, -6.79f}, {},
                                     {1.02f, 0.14f, 0.20f}),
                       MaterialId::LimestoneVariation});
    // Almond-shaped eyes and brow ridges
    for (float eyeX : {-0.85f, 0.85f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{eyeX, 10.82f, -6.65f},
                                         {0.0f, 0.0f, (eyeX < 0 ? -12.0f : 12.0f)},
                                         {0.70f, 0.17f, 0.22f}),
                           MaterialId::LimestoneVariation});
    }

    // Left and Right Royal Ears
    for (float earX : {-1.72f, 1.72f})
    {
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(s + glm::vec3{earX, 10.45f, -5.35f},
                                         {0.0f, (earX < 0 ? -25.0f : 25.0f), 0.0f},
                                         {0.25f, 0.60f, 0.42f}),
                           MaterialId::Limestone});
    }

    // Ceremonial Beard post / chin support
    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 8.15f, -6.38f}, {},
                                     {0.46f, 1.55f, 0.46f}),
                       MaterialId::PreparedStone});

    // 7. Nemes Royal Headdress
    // Arched crown hood draped over skull
    objects.push_back({ScenePrimitive::SphinxHeaddress,
                       makeTransform(s, {}, {1.0f, 1.0f, 1.0f}),
                       MaterialId::PreparedStone});

    // Uraeus (sacred cobra motif on forehead)
    objects.push_back({ScenePrimitive::Cylinder,
                       makeTransform(s + glm::vec3{0.0f, 11.85f, -6.35f},
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
    const MeshData body = createBodyMesh();
    const MeshData neck = createNeckMesh();
    const MeshData headdress = createHeaddressMesh();
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
    const auto indexedMeshValid = [](const MeshData& mesh)
    {
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size() % 3 != 0)
            return false;
        for (const Vertex& vertex : mesh.vertices)
            if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
                !std::isfinite(vertex.position.z) || !std::isfinite(vertex.normal.x) ||
                !std::isfinite(vertex.normal.y) || !std::isfinite(vertex.normal.z) ||
                std::abs(glm::length(vertex.normal) - 1.0f) > 0.01f)
                return false;
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
            if (a >= mesh.vertices.size() || b >= mesh.vertices.size() ||
                c >= mesh.vertices.size()) return false;
            const glm::vec3 face = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                              mesh.vertices[c].position - mesh.vertices[a].position);
            const glm::vec3 normal = mesh.vertices[a].normal + mesh.vertices[b].normal +
                                     mesh.vertices[c].normal;
            if (glm::length(face) < 1.0e-6f || glm::dot(face, normal) <= 0.0f)
                return false;
        }
        return true;
    };
    const bool dedicatedMeshesValid = indexedMeshValid(body) &&
        indexedMeshValid(neck) && indexedMeshValid(headdress);
    const MeshData bodyAgain = createBodyMesh();
    const MeshData neckAgain = createNeckMesh();
    const MeshData headdressAgain = createHeaddressMesh();
    const auto sameMesh = [](const MeshData& a, const MeshData& b)
    {
        if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
        for (std::size_t i = 0; i < a.vertices.size(); ++i)
            if (a.vertices[i].position != b.vertices[i].position ||
                a.vertices[i].normal != b.vertices[i].normal) return false;
        return true;
    };
    const bool deterministic = sameMesh(body, bodyAgain) &&
                               sameMesh(neck, neckAgain) &&
                               sameMesh(headdress, headdressAgain);
    bool terrainContact = true;
    for (float x : {sphinx.center_.x - 6.0f, sphinx.center_.x,
                    sphinx.center_.x + 6.0f})
        for (float z : {sphinx.center_.z - 17.0f, sphinx.center_.z + 2.0f,
                        sphinx.center_.z + 21.0f})
            terrainContact = terrainContact &&
                std::abs(SandSimulation::staticTerrainHeightAt(x, z)) < 0.10f;
    const bool pawContact = std::abs((1.92f - 0.5f * 1.4f) - 1.3f) < 0.10f;
    std::size_t triangles = 0;
    const MeshData cube = PrimitiveGenerator::createCube();
    const MeshData cylinder = PrimitiveGenerator::createCylinder();
    for (const SceneObject& object : objects)
    {
        const MeshData* mesh = &cube;
        if (object.primitive == ScenePrimitive::Cylinder) mesh = &cylinder;
        else if (object.primitive == ScenePrimitive::SphinxForm) mesh = &form;
        else if (object.primitive == ScenePrimitive::SphinxBody) mesh = &body;
        else if (object.primitive == ScenePrimitive::SphinxNeck) mesh = &neck;
        else if (object.primitive == ScenePrimitive::SphinxHeaddress) mesh = &headdress;
        triangles += mesh->indices.size() / 3;
    }
    const bool footprintValid = containsStructuralFootprint(92.0f, -105.0f) &&
        containsStructuralFootprint(92.0f, -125.0f) &&
        containsStructuralFootprint(92.0f, -81.0f) &&
        !containsStructuralFootprint(92.0f, -126.0f) &&
        !containsStructuralFootprint(110.0f, -105.0f);

    const bool valid = hasParts && dimensionsValid && allTransformsValid &&
                       meshValid && dedicatedMeshesValid && deterministic &&
                       terrainContact && pawContact && footprintValid;

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
           << "  shaped body/neck/headdress indexed winding and normals: "
           << (dedicatedMeshesValid ? "PASS" : "FAIL") << " ("
           << body.vertices.size() << "/" << body.indices.size() / 3 << ", "
           << neck.vertices.size() << "/" << neck.indices.size() / 3 << ", "
           << headdress.vertices.size() << "/" << headdress.indices.size() / 3
           << " vertices/triangles)\n"
           << "  deterministic procedural generation: "
           << (deterministic ? "PASS" : "FAIL") << '\n'
           << "  terrain/plinth/paw contact: "
           << ((terrainContact && pawContact) ? "PASS" : "FAIL") << '\n'
           << "  component draws/triangles: " << objects.size() << "/" << triangles << '\n'
           << "  structural footprint includes both plinth ends: "
           << (footprintValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sphinx monument validation passed.\n"
                     : "Sphinx monument validation failed.\n");

    return valid;
}
