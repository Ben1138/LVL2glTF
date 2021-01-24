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
using LibSWBF2::Types::Color4u8;
using LibSWBF2::Wrappers::World;
using LibSWBF2::Wrappers::Terrain;
using LibSWBF2::Wrappers::Instance;
using LibSWBF2::Wrappers::Model;
using LibSWBF2::Wrappers::Segment;
using LibSWBF2::Wrappers::Material;


bool grabLibSWBF2Logs()
{
    LoggerEntry logEntry;
    while (Logger::GetNextLog(logEntry))
    {
        LOG(logEntry.ToString().Buffer());
    }
}

void convertColor(const Color4u8& swbfColor, std::vector<double>& outColor)
{
    outColor.resize(4);
    outColor[0] = swbfColor.m_Red / 255.0;
    outColor[1] = swbfColor.m_Green / 255.0;
    outColor[2] = swbfColor.m_Blue / 255.0;
    outColor[3] = swbfColor.m_Alpha / 255.0;
}

void copyBuffer(Vector3* srcBuffer, uint32_t srcCount, tinygltf::Buffer& dstBuffer, int dstOffset)
{
    for (uint32_t i = 0; i < srcCount; ++i)
    {
        int vecIdx = dstOffset + i * sizeof(float) * 3;
        *reinterpret_cast<float*>(&dstBuffer.data[vecIdx])                     = srcBuffer[i].m_X;
        *reinterpret_cast<float*>(&dstBuffer.data[vecIdx + sizeof(float)])     = srcBuffer[i].m_Y;
        *reinterpret_cast<float*>(&dstBuffer.data[vecIdx + sizeof(float) * 2]) = srcBuffer[i].m_Z;
    }
}

void copyBuffer(Vector2* srcBuffer, uint32_t srcCount, tinygltf::Buffer& dstBuffer, int dstOffset)
{
    for (uint32_t i = 0; i < srcCount; ++i)
    {
        int vecIdx = dstOffset + i * sizeof(float) * 2;
        *reinterpret_cast<float*>(&dstBuffer.data[vecIdx])                 = srcBuffer[i].m_X;
        *reinterpret_cast<float*>(&dstBuffer.data[vecIdx + sizeof(float)]) = srcBuffer[i].m_Y;
    }
}

void copyBuffer(uint16_t* srcBuffer, uint32_t srcCount, tinygltf::Buffer& dstBuffer, int dstOffset)
{
    for (uint32_t i = 0; i < srcCount; ++i)
    {
        int vecIdx = dstOffset + i * sizeof(uint16_t);
        *reinterpret_cast<uint16_t*>(&dstBuffer.data[vecIdx]) = srcBuffer[i];
    }
}

inline void copyBuffers(
    Vector3*  swbfVertexBuffer,
    uint32_t  swbfVertexBufferCount,
    Vector3*  swbfNormalBuffer,
    uint32_t  swbfNormalBufferCount,
    Vector2*  swbfUVBuffer,
    uint32_t  swbfUVBufferCount,
    uint16_t* swbfIndexBuffer,
    uint32_t  swbfIndexBufferCount,
    tinygltf::Model& dstModel,
    int& gltfVertexBufferAccIdx,
    int& gltfNormalBufferAccIdx,
    int& gltfUVBufferAccIdx,
    int& gltfIndexBufferAccIdx
)
{
    int segmBufferIdx = 0;
    int offset = 0;

    uint32_t swbfVertexBufferSize = swbfVertexBufferCount * sizeof(float) * 3;
    uint32_t swbfNormalBufferSize = swbfNormalBufferCount * sizeof(float) * 3;
    uint32_t swbfUVBufferSize = swbfUVBufferCount * sizeof(float) * 2;
    uint32_t swbfIndexBufferSize = swbfIndexBufferCount * sizeof(uint16_t);

    tinygltf::Buffer& segmBuffer = dstModel.buffers.emplace_back();
    segmBufferIdx = (int)dstModel.buffers.size() - 1;
    segmBuffer.data.resize(
        (size_t)swbfVertexBufferSize +
        (size_t)swbfNormalBufferSize +
        (size_t)swbfUVBufferSize +
        (size_t)swbfIndexBufferSize
    );

    {
        copyBuffer(swbfVertexBuffer, swbfVertexBufferCount, segmBuffer, offset);
        tinygltf::BufferView& view = dstModel.bufferViews.emplace_back();
        view.buffer = segmBufferIdx;
        view.byteOffset = offset;
        view.byteLength = swbfVertexBufferSize;
        view.byteStride = sizeof(float) * 3;
        offset += swbfVertexBufferSize;
        tinygltf::Accessor& acc = dstModel.accessors.emplace_back();
        gltfVertexBufferAccIdx = (int)dstModel.accessors.size() - 1;
        acc.bufferView = (int)dstModel.bufferViews.size() - 1;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        acc.type = TINYGLTF_TYPE_VEC3;
        acc.count = swbfVertexBufferCount;
    }
    {
        copyBuffer(swbfNormalBuffer, swbfNormalBufferCount, segmBuffer, offset);
        tinygltf::BufferView& view = dstModel.bufferViews.emplace_back();
        view.buffer = segmBufferIdx;
        view.byteOffset = offset;
        view.byteLength = swbfNormalBufferSize;
        view.byteStride = sizeof(float) * 3;
        offset += swbfNormalBufferSize;
        tinygltf::Accessor& acc = dstModel.accessors.emplace_back();
        gltfNormalBufferAccIdx = (int)dstModel.accessors.size() - 1;
        acc.bufferView = (int)dstModel.bufferViews.size() - 1;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        acc.type = TINYGLTF_TYPE_VEC3;
        acc.count = swbfNormalBufferCount;
    }
    {
        copyBuffer(swbfUVBuffer, swbfUVBufferCount, segmBuffer, offset);
        tinygltf::BufferView& view = dstModel.bufferViews.emplace_back();
        view.buffer = segmBufferIdx;
        view.byteOffset = offset;
        view.byteLength = swbfUVBufferSize;
        view.byteStride = sizeof(float) * 2;
        offset += swbfUVBufferSize;
        tinygltf::Accessor& acc = dstModel.accessors.emplace_back();
        gltfUVBufferAccIdx = (int)dstModel.accessors.size() - 1;
        acc.bufferView = (int)dstModel.bufferViews.size() - 1;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        acc.type = TINYGLTF_TYPE_VEC2;
        acc.count = swbfUVBufferCount;
    }
    {
        copyBuffer(swbfIndexBuffer, swbfIndexBufferCount, segmBuffer, offset);
        tinygltf::BufferView& view = dstModel.bufferViews.emplace_back();
        view.buffer = segmBufferIdx;
        view.byteOffset = offset;
        view.byteLength = swbfIndexBufferSize;
        view.byteStride = sizeof(uint16_t);
        offset += swbfIndexBufferSize;
        tinygltf::Accessor& acc = dstModel.accessors.emplace_back();
        gltfIndexBufferAccIdx = (int)dstModel.accessors.size() - 1;
        acc.bufferView = (int)dstModel.bufferViews.size() - 1;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        acc.type = TINYGLTF_TYPE_SCALAR;
        acc.count = swbfIndexBufferCount;
    }
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
            return TINYGLTF_MODE_TRIANGLE_STRIP;
        default:
            LOG("Unknown ETopology type: {0}! Assuming Triangle List!", (int)topology);
            return TINYGLTF_MODE_TRIANGLES;
    }
}

void printMenu(const std::vector<std::string>& worldNames, std::vector<bool>& chosenWorlds)
{
    LOG("Choose which Layers to convert:");
    for (size_t i = 0; i < worldNames.size(); ++i)
    {
        LOG("  {0:2d}) [{1}] {2}", i + 1, chosenWorlds[i] ? 'X' : ' ', worldNames[i]);
    }
    LOG("\n  0) Convert choosen layers");
}

int main(int argc, char** argv)
{
    CLI::App app{ "LVL to glTF 2.0 converter" };
    std::string fileIn = "";
    std::string fileCom = "";
    std::string fileOut = "";
    bool bGLTF = false;
    app.add_option("-i,--inlvl", fileIn, "Path to the world LVL file to convert");
    app.add_option("-c,--incommon", fileCom, "(optional) Path to ingame.lvl (needed for command posts, turrets, health droids, etc.");
    app.add_option("-o,--outglb", fileOut, "(optional) output file. If not specified, the output file path will match the input file path, with just the file extension changed.");
    app.add_option("--gltf", bGLTF, "The output file will be a .gltf file (text format). Default is .glb (binary format). Note that for the .gltf format, textures won't get exported!");
    CLI11_PARSE(app, argc, argv);

    if (fileIn.empty())
    {
        LOG("No input LVL file specified!");
        LOG(app.help());
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
        p.replace_extension(bGLTF ? ".gltf" : ".glb");
        fileOut = p.u8string();
    }
    
    Logger::SetLogfileLevel(ELogType::Error);

    Container* con = Container::Create();
    con->AddLevel(fileIn.c_str());
    if (!fileCom.empty())
    {
        if (fs::exists(fileCom))
        {
            con->AddLevel(fileCom.c_str());
        }
        else
        {
            LOG("Could not find '{0}'!", fileCom.c_str());
        }
    }
    con->StartLoading();

    std::string filename = fs::path(fileIn).filename().u8string();
    fmt::memory_buffer updateLine;
    fmt::format_to(updateLine, "Start Loading '{0}'...", filename.c_str());
    std::cout << updateLine.data();

    while (!con->IsDone())
    {
        if (grabLibSWBF2Logs())
        {
            LOG("Loading '{0}'... {1}%", fileIn.c_str(), (int)(con->GetOverallProgress() * 100.0f));
        }
        else
        {
            updateLine.clear();
            fmt::format_to(updateLine, "Loading '{0}'... {1}%", filename.c_str(), (int)(con->GetOverallProgress() * 100.0f));
            std::cout << '\r' << updateLine.data();
        }
    }

    Level* lvl = con->TryGetWorldLevel();
    if (lvl == nullptr)
    {
        LOG("Loading '{0}' failed!", filename.c_str());
        return 1;
    }

    const List<World> worlds = lvl->GetWorlds();
    if (worlds.Size() == 0)
    {
        LOG("Seems like '{0}' doesn't contain any world data! Nothing to do...", filename.c_str());
        return 1;
    }

    std::vector<std::string> worldNames;
    std::vector<bool> chosenWorlds;
    for (uint32_t i = 0; i < worlds.Size(); ++i)
    {
        worldNames.emplace_back(fmt::format("{0:25s} [{1} objects]", worlds[i].GetName().Buffer(), worlds[i].GetInstances().Size()));
        chosenWorlds.emplace_back(false);
    }

    int option = -1;
    int numLayers = 0;
    do
    {
        printMenu(worldNames, chosenWorlds);
        std::cout << "\nChoose: ";
        std::cin >> option;

        if (option < 0 || option >= worldNames.size())
        {
            LOG("{0} is not a valid option!");
            option = -1;
        }
        else if (option != 0)
        {
            chosenWorlds[option - 1] = !chosenWorlds[option - 1];
            numLayers += chosenWorlds[option - 1] ? 1 : -1;
        }
        if (numLayers == 0)
        {
            LOG("No layers choosen for conversion! Choose at least one layer!");
        }
    }
    while (option != 0 || numLayers == 0);

    tinygltf::Model gltf;
    gltf.asset.copyright = "https://github.com/Ben1138/LVL2glTF";
    gltf.asset.generator = "LVL2glTF converter";
    gltf.asset.minVersion = "2.0";
    gltf.asset.version = "2.0";

    std::unordered_map<std::string, int> geomNameToMeshIdx;

    for (uint32_t i = 0; i < worlds.Size(); ++i)
    {
        // skip unwanted layers
        if (!chosenWorlds[i]) continue;

        const World& wld = worlds[i];
        String wldName = wld.GetName();
        const Terrain* terr = wld.GetTerrain();

        tinygltf::Scene& scene = gltf.scenes.emplace_back();
        scene.name = wldName.Buffer();
        
        if (terr != nullptr)
        {
            tinygltf::Node& terrNode = gltf.nodes.emplace_back();
            terrNode.name = terr->GetName().Buffer();
            scene.nodes.emplace_back((int)gltf.nodes.size() - 1);
            terrNode.translation = { 0.0, 0.0, 0.0 };
            terrNode.rotation = { 0.0, 0.0, 0.0, 1.0 };

            tinygltf::Mesh& terrMesh = gltf.meshes.emplace_back();
            int terrMeshIdx = (int)gltf.meshes.size() - 1;
            terrMesh.name = terr->GetName().Buffer();
            terrNode.mesh = terrMeshIdx;

            Vector3*  swbfVertexBuffer = nullptr;
            uint32_t  swbfVertexBufferCount = 0;
            Vector3*  swbfNormalBuffer = nullptr;
            uint32_t  swbfNormalBufferCount = 0;
            Vector2*  swbfUVBuffer = nullptr;
            uint32_t  swbfUVBufferCount = 0;
            uint16_t* swbfIndexBuffer = nullptr;
            uint32_t  swbfIndexBufferCount = 0;
            int gltfVertexBufferAccIdx = 0;
            int gltfNormalBufferAccIdx = 0;
            int gltfUVBufferAccIdx = 0;
            int gltfIndexBufferAccIdx = 0;

            terr->GetVertexBuffer(swbfVertexBufferCount, swbfVertexBuffer);
            terr->GetNormalBuffer(swbfNormalBufferCount, swbfNormalBuffer);
            terr->GetUVBuffer(swbfUVBufferCount, swbfUVBuffer);
            terr->GetIndexBuffer(ETopology::TriangleList, swbfIndexBufferCount, swbfIndexBuffer);

            copyBuffers(
                swbfVertexBuffer,
                swbfVertexBufferCount,
                swbfNormalBuffer,
                swbfNormalBufferCount,
                swbfUVBuffer,
                swbfUVBufferCount,
                swbfIndexBuffer,
                swbfIndexBufferCount,
                gltf,
                gltfVertexBufferAccIdx,
                gltfNormalBufferAccIdx,
                gltfUVBufferAccIdx,
                gltfIndexBufferAccIdx
            );

            // TODO: prevent material duplicates
            tinygltf::Material& gltfMat = gltf.materials.emplace_back();
            gltfMat.pbrMetallicRoughness.baseColorFactor = { 1.0, 1.0, 1.0, 1.0 };
            gltfMat.pbrMetallicRoughness.metallicFactor = 0.0f;

            // TODO: textures

            tinygltf::Primitive& prim = terrMesh.primitives.emplace_back();
            prim.attributes =
            {
                { "POSITION",   gltfVertexBufferAccIdx },
                { "NORMAL",     gltfNormalBufferAccIdx },
                { "TEXCOORD_0", gltfUVBufferAccIdx     },
            };

            prim.indices = gltfIndexBufferAccIdx;
            prim.mode = TINYGLTF_MODE_TRIANGLES;
            prim.material = (int)gltf.materials.size() - 1;
        }

        List<Instance> insts = wld.GetInstances();
        for (uint32_t j = 0; j < insts.Size(); ++j)
        {
            const Instance& inst = insts[j];
            String instName = inst.GetName();

            String geometryName;
            if (!inst.GetProperty("GeometryName", geometryName))
            {
                //LOG("Could not resolve 'GeometryName' property of instance '{0}' in world '{1}'", instName.Buffer(), wldName.Buffer());
                continue;
            }
            
            const Model* model = con->FindModel(geometryName);
            if (model == nullptr)
            {
                //LOG("Could not find model '{0}' for instance '{1}'!", geometryName.Buffer(), instName.Buffer());
                continue;
            }

            tinygltf::Node& node = gltf.nodes.emplace_back();
            node.name = instName.Buffer();
            scene.nodes.emplace_back((int)gltf.nodes.size() - 1);

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
                int meshIdx = (int)gltf.meshes.size() - 1;
                geomNameToMeshIdx.emplace(geometryName.Buffer(), meshIdx);
                mesh.name = model->GetName().Buffer();
                node.mesh = meshIdx;

                LOG("Converting mesh '{0}'", mesh.name.c_str());

                const List<Segment>& segments = model->GetSegments();
                for (uint32_t k = 0; k < segments.Size(); ++k)
                {
                    const Segment& segm = segments[k];

                    Vector3*  swbfVertexBuffer = nullptr;
                    uint32_t  swbfVertexBufferCount = 0;
                    Vector3*  swbfNormalBuffer = nullptr;
                    uint32_t  swbfNormalBufferCount = 0;
                    Vector2*  swbfUVBuffer = nullptr;
                    uint32_t  swbfUVBufferCount = 0;
                    uint16_t* swbfIndexBuffer = nullptr;
                    uint32_t  swbfIndexBufferCount = 0;
                    int gltfVertexBufferAccIdx = 0;
                    int gltfNormalBufferAccIdx = 0;
                    int gltfUVBufferAccIdx = 0;
                    int gltfIndexBufferAccIdx = 0;

                    segm.GetVertexBuffer(swbfVertexBufferCount, swbfVertexBuffer);
                    segm.GetNormalBuffer(swbfNormalBufferCount, swbfNormalBuffer);
                    segm.GetUVBuffer(swbfUVBufferCount, swbfUVBuffer);
                    segm.GetIndexBuffer(swbfIndexBufferCount, swbfIndexBuffer);

                    copyBuffers(
                        swbfVertexBuffer,
                        swbfVertexBufferCount,
                        swbfNormalBuffer,
                        swbfNormalBufferCount,
                        swbfUVBuffer,
                        swbfUVBufferCount,
                        swbfIndexBuffer,
                        swbfIndexBufferCount,
                        gltf,
                        gltfVertexBufferAccIdx,
                        gltfNormalBufferAccIdx,
                        gltfUVBufferAccIdx,
                        gltfIndexBufferAccIdx
                    );

                    // TODO: prevent material duplicates
                    const Material& swbfMat = segm.GetMaterial();
                    tinygltf::Material& gltfMat = gltf.materials.emplace_back();
                    convertColor(swbfMat.GetDiffuseColor(), gltfMat.pbrMetallicRoughness.baseColorFactor);
                    gltfMat.pbrMetallicRoughness.metallicFactor = 0.0f;
                    
                    // TODO: textures
                    

                    tinygltf::Primitive& prim = mesh.primitives.emplace_back();
                    prim.attributes =
                    { 
                        { "POSITION",   gltfVertexBufferAccIdx },
                        { "NORMAL",     gltfNormalBufferAccIdx },
                        { "TEXCOORD_0", gltfUVBufferAccIdx     },
                    };

                    prim.indices = gltfIndexBufferAccIdx;
                    prim.mode = gltfTopology(segm.GetTopology());
                    prim.material = (int)gltf.materials.size() - 1;
                }
            }
        }
    }

    con->FreeAll();
    Container::Delete(con);

    grabLibSWBF2Logs();

    LOG("Writing output file: {0}...", fileOut.c_str());
    tinygltf::TinyGLTF writer;
    writer.WriteGltfSceneToFile(&gltf, fileOut, false, true, true, !bGLTF);
    LOG("Done!");

    return 0;
}