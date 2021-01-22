#include <iostream>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <CLI11.hpp>
#include <LibSWBF2.h>
#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace fs = std::filesystem;

#define LOG(formatStr, ...) std::cout << fmt::format(formatStr, __VA_ARGS__) << std::endl;

using LibSWBF2::Logging::Logger;
using LibSWBF2::Logging::LoggerEntry;
using LibSWBF2::ELogType;
using LibSWBF2::Level;
using LibSWBF2::Container;
using LibSWBF2::SWBF2Handle;
using LibSWBF2::ETopology;
using LibSWBF2::Types::List;
using LibSWBF2::Types::Vector2;
using LibSWBF2::Types::Vector3;
using LibSWBF2::Types::Vector4;
using LibSWBF2::Types::String;
using LibSWBF2::Types::Color;
using LibSWBF2::Wrappers::World;
using LibSWBF2::Wrappers::Terrain;
using LibSWBF2::Wrappers::Instance;
using LibSWBF2::Wrappers::Model;
using LibSWBF2::Wrappers::Segment;
using LibSWBF2::Wrappers::Material;


bool grabLibSWBF2Logs()
{
    bool sthLogged = false;
    LoggerEntry logEntry;
    while (Logger::GetNextLog(logEntry))
    {
        std::cout << '\n';
        LOG(logEntry.ToString().Buffer());
        sthLogged = true;
    }
    return sthLogged;
}

void convertColor(const Color& swbfColor, std::vector<double>& outColor)
{
    outColor.resize(4);
    outColor[0] = swbfColor.m_Red / 255.0;
    outColor[1] = swbfColor.m_Green / 255.0;
    outColor[2] = swbfColor.m_Blue / 255.0;
    outColor[3] = swbfColor.m_Alpha / 255.0;
}

int gltfTopology(ETopology topology)
{
    switch (topology)
    {
        case ETopology::LineList:
            return TINYGLTF_MODE_LINE_LOOP;
        case ETopology::LineStrip:
            return TINYGLTF_MODE_LINE_STRIP;
        case ETopology::PointList:
            return TINYGLTF_MODE_POINTS;
        case ETopology::TriangleFan:
            return TINYGLTF_MODE_TRIANGLE_FAN;
        case ETopology::TriangleList:
            return TINYGLTF_MODE_TRIANGLES;
        case ETopology::TriangleStrip:
            return TINYGLTF_MODE_LINE_STRIP;
        default:
            LOG("Unknown ETopology type: {0}! Assuming Triangle List!", (int)topology);
            return TINYGLTF_MODE_TRIANGLES;
    }
}

int main(int argc, char** argv)
{
    CLI::App app{ "LVL to glTF converter" };
    std::string fileIn = "";
    std::string fileOut = "";
    app.add_option("-i,--inlvl", fileIn, "The world LVL file to convert");
    app.add_option("-o,--outglb", fileOut, "(optional) output file");
    CLI11_PARSE(app, argc, argv);

    if (fileIn.empty())
    {
        LOG("No input LVL file specified!");
        return 1;
    }

    if (!fs::exists(fileIn))
    {
        LOG("Specified file '{0}' doesn't exist!", fileIn.c_str());
        return 1;
    }

    if (fileOut.empty())
    {
        fs::path p = fileIn;
        p.replace_extension(".glb");
        fileOut = p.u8string();
    }
    
    Logger::SetLogfileLevel(ELogType::Warning);

    Container* con = Container::Create();
    SWBF2Handle lvlHandle = con->AddLevel(fileIn.c_str());
    con->StartLoading();

    std::string filename = fs::path(fileIn).filename().u8string();
    fmt::memory_buffer updateLine;
    fmt::format_to(updateLine, "Start Loading '{0}'...\0", filename.c_str());
    std::cout << updateLine.data() << '\0';

    while (!con->IsDone())
    {
        if (grabLibSWBF2Logs())
        {
            LOG("Loading '{0}'... {1}%", fileIn.c_str(), (int)(con->GetOverallProgress() * 100.0f));
        }
        else
        {
            updateLine.clear();
            fmt::format_to(updateLine, "Loading '{0}'... {1}%\0", filename.c_str(), (int)(con->GetOverallProgress() * 100.0f));
            std::cout << '\r' << updateLine.data() << '\0';
        }
    }

    Level* lvl = con->TryGetWorldLevel();
    if (lvl == nullptr)
    {
        LOG("Loading '{0}' failed!", fileIn.c_str());
        return 1;
    }

    const List<World> worlds = lvl->GetWorlds();
    if (worlds.Size() == 0)
    {
        LOG("Seems like '{0}' doesn't contain any world data! Nothing to do...", fileIn.c_str());
        return 1;
    }

    tinygltf::Model gltf;
    std::unordered_map<std::string, int> geomNameToMeshIdx;

    for (uint32_t i = 0; i < worlds.Size(); ++i)
    {
        const World& wld = worlds[i];
        String wldName = wld.GetName();
        //const Terrain* terr = wld.GetTerrain();

        tinygltf::Scene& scene = gltf.scenes.emplace_back();
        scene.name = wldName.Buffer();
        
        List<Instance> insts = wld.GetInstances();
        for (uint32_t j = 0; j < insts.Size(); ++j)
        {
            const Instance& inst = insts[j];
            String instName = inst.GetName();

            String geometryName;
            if (!inst.GetProperty("GeometryName", geometryName))
            {
                LOG("Could not resolve 'GeometryName' property of instance '{0}' in world '{1}'", instName.Buffer(), wldName.Buffer());
                continue;
            }
            
            const Model* model = con->FindModel(geometryName);
            if (model == nullptr)
            {
                LOG("Could not find model '{0}' for instance '{1}'!", geometryName.Buffer(), instName.Buffer());
                continue;
            }

            tinygltf::Node& node = gltf.nodes.emplace_back();
            node.name = instName.Buffer();

            Vector3 pos = inst.GetPosition();
            Vector4 rot = inst.GetRotation();
            node.translation = { pos.m_X, pos.m_Y, pos.m_Z };
            node.rotation = { rot.m_X, rot.m_Y, rot.m_Z, rot.m_W };

            // check whether the referenced mesh was already converted
            auto it = geomNameToMeshIdx.find(geometryName.Buffer());
            if (it != geomNameToMeshIdx.end())
            {
                node.mesh = it->second;
            }
            else
            {
                tinygltf::Mesh& mesh = gltf.meshes.emplace_back();
                geomNameToMeshIdx.emplace(geometryName.Buffer(), (int)gltf.meshes.size() - 1);
                mesh.name = model->GetName().Buffer();

                const List<Segment>& segments = model->GetSegments();
                for (uint32_t k = 0; k < segments.Size(); ++k)
                {
                    const Segment& segm = segments[k];

                    tinygltf::Buffer& vertexBuffer = gltf.buffers.emplace_back();
                    int vertexBufferIdx = (int)gltf.buffers.size() - 1;

                    tinygltf::Buffer& normalBuffer = gltf.buffers.emplace_back();
                    int normalBufferIdx = (int)gltf.buffers.size() - 1;

                    tinygltf::Buffer& uvBuffer = gltf.buffers.emplace_back();
                    int uvBufferIdx = (int)gltf.buffers.size() - 1;

                    tinygltf::Buffer& indexBuffer = gltf.buffers.emplace_back();
                    int indexBufferIdx = (int)gltf.buffers.size() - 1;

                    uint16_t* idxBuffer = nullptr;
                    Vector2* vec2Buffer = nullptr;
                    Vector3* vec3Buffer = nullptr;
                    uint32_t count = 0;

                    // TODO: copy buffer method, use for loop for copying
                    segm.GetVertexBuffer(count, vec3Buffer);
                    vertexBuffer.data.resize(count * sizeof(Vector3));
                    std::memcpy(vertexBuffer.data.data(), vec3Buffer, count * sizeof(Vector3));

                    segm.GetNormalBuffer(count, vec3Buffer);
                    normalBuffer.data.resize(count * sizeof(Vector3));
                    std::memcpy(normalBuffer.data.data(), vec3Buffer, count * sizeof(Vector3));

                    segm.GetUVBuffer(count, vec2Buffer);
                    uvBuffer.data.resize(count * sizeof(Vector2));
                    std::memcpy(uvBuffer.data.data(), vec3Buffer, count * sizeof(Vector2));
                    
                    segm.GetIndexBuffer(count, idxBuffer);
                    indexBuffer.data.resize(count * sizeof(uint16_t));
                    std::memcpy(indexBuffer.data.data(), idxBuffer, count * sizeof(uint16_t));

                    const Material& swbfMat = segm.GetMaterial();
                    tinygltf::Material& gltfMat = gltf.materials.emplace_back();
                    convertColor(swbfMat.GetDiffuseColor(), gltfMat.pbrMetallicRoughness.baseColorFactor);
                    gltfMat.pbrMetallicRoughness.metallicFactor = 0.0f;
                    gltfMat.pbrMetallicRoughness.roughnessFactor = 1.0f;
                    
                    // TODO: textures

                    tinygltf::Primitive& prim = mesh.primitives.emplace_back();
                    prim.attributes =
                    { 
                        { "POSITION",   vertexBufferIdx },
                        { "NORMAL",     normalBufferIdx },
                        { "TEXCOORD_0", uvBufferIdx     },
                    };
                    prim.indices = indexBufferIdx;
                    prim.mode = gltfTopology(segm.GetTopology());
                    prim.material = (int)gltf.materials.size() - 1;
                }
            }
        }
    }

    con->FreeAll();
    Container::Delete(con);

    grabLibSWBF2Logs();

    tinygltf::TinyGLTF writer;
    writer.WriteGltfSceneToFile(&gltf, fileOut, false, true, true, true);
    LOG("Output: {0}", fileOut.c_str());

    return 0;
}