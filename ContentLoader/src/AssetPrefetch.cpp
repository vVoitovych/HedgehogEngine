#include "api/AssetPrefetch.hpp"
#include "api/EnvironmentBake.hpp"
#include "api/MeshLoader.hpp"
#include "api/TextureLoader.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <mutex>
#include <unordered_map>
#include <utility>

namespace ContentLoader
{
    namespace
    {
        struct KeptEnvironment
        {
            EnvironmentBakeDesc Desc;
            BakedEnvironment    Baked;
        };

        struct Store
        {
            std::mutex                                         Mutex;
            std::unordered_map<std::string, LoadedMesh>        Meshes;
            std::unordered_map<std::string, PrefetchedTexture> Textures;
            std::unordered_map<std::string, KeptEnvironment>   Environments;
        };

        Store& GetStore()
        {
            static Store store;
            return store;
        }

        std::string KeyOf(const std::string& path)
        {
            return FS::MakeAssetKey(FS::ToAssetVirtualPath(path));
        }

        size_t BytesOf(const LoadedMesh& mesh)
        {
            size_t bytes = mesh.vertices.size() * sizeof(LoadedVertexData) + mesh.indices.size() * sizeof(uint32_t) +
                           mesh.Joints.size() * sizeof(HM::Vector4u) + mesh.Weights.size() * sizeof(HM::Vector4);
            for (const auto& level : mesh.LodIndices)
                bytes += level.size() * sizeof(uint32_t);
            return bytes;
        }

        size_t BytesOf(const BakedEnvironment& baked)
        {
            size_t bytes = 0;
            for (const auto& faces : baked.Mips)
                for (const auto& face : faces)
                    bytes += face.size() * sizeof(uint16_t);
            return bytes;
        }

        template <typename Map>
        bool IsKept(Map& map, const std::string& key)
        {
            Store&          store = GetStore();
            std::lock_guard lock(store.Mutex);
            return map.contains(key);
        }

        template <typename Map, typename Value>
        void Keep(Map& map, const std::string& key, Value value)
        {
            Store&          store = GetStore();
            std::lock_guard lock(store.Mutex);
            map.insert_or_assign(key, std::move(value));
        }

        template <typename Map>
        auto Take(Map& map, const std::string& key) -> std::optional<typename Map::mapped_type>
        {
            Store&          store = GetStore();
            std::lock_guard lock(store.Mutex);
            const auto      it = map.find(key);
            if (it == map.end())
                return std::nullopt;
            std::optional<typename Map::mapped_type> value(std::move(it->second));
            map.erase(it);
            return value;
        }
    }

    bool PrefetchMesh(const std::string& path, const FS::FileSystemManager& fileSystem)
    {
        const std::string key = KeyOf(path);
        if (IsKept(GetStore().Meshes, key))
            return true;
        // Decoded outside the lock, so loads on other threads carry on meanwhile.
        std::optional<LoadedMesh> mesh = LoadMesh(path, fileSystem);
        if (!mesh)
            return false;
        Keep(GetStore().Meshes, key, std::move(*mesh));
        return true;
    }

    bool PrefetchTexture(const std::string& path, const FS::FileSystemManager& fileSystem)
    {
        const std::string key = KeyOf(path);
        if (IsKept(GetStore().Textures, key))
            return true;
        TextureLoader loader;
        if (!loader.LoadTexture(path, fileSystem))
            return false;
        PrefetchedTexture texture;
        texture.Width    = loader.GetWidth();
        texture.Height   = loader.GetHeight();
        texture.Channels = loader.GetChannels();
        const auto* data = static_cast<const unsigned char*>(loader.GetData());
        texture.Pixels.assign(data, data + static_cast<size_t>(texture.Width) * texture.Height * 4);
        Keep(GetStore().Textures, key, std::move(texture));
        return true;
    }

    bool PrefetchEnvironment(const std::string& path, const FS::FileSystemManager& fileSystem,
                             const EnvironmentBakeDesc& desc)
    {
        const std::string key = KeyOf(path);
        {
            Store&          store = GetStore();
            std::lock_guard lock(store.Mutex);
            const auto      it = store.Environments.find(key);
            if (it != store.Environments.end() && it->second.Desc.FaceSize == desc.FaceSize &&
                it->second.Desc.SampleCount == desc.SampleCount)
                return true;
        }
        std::optional<BakedEnvironment> baked = BakeEnvironment(path, fileSystem, desc);
        if (!baked)
            return false;
        Keep(GetStore().Environments, key, KeptEnvironment{ desc, std::move(*baked) });
        return true;
    }

    std::optional<LoadedMesh> TakeMesh(const std::string& path)
    {
        return Take(GetStore().Meshes, KeyOf(path));
    }

    std::optional<PrefetchedTexture> TakeTexture(const std::string& path)
    {
        return Take(GetStore().Textures, KeyOf(path));
    }

    std::optional<BakedEnvironment> TakeEnvironment(const std::string& path, const EnvironmentBakeDesc& desc)
    {
        Store&          store = GetStore();
        std::lock_guard lock(store.Mutex);
        const auto      it = store.Environments.find(KeyOf(path));
        if (it == store.Environments.end() || it->second.Desc.FaceSize != desc.FaceSize ||
            it->second.Desc.SampleCount != desc.SampleCount)
            return std::nullopt;
        std::optional<BakedEnvironment> baked(std::move(it->second.Baked));
        store.Environments.erase(it);
        return baked;
    }

    void ClearPrefetched()
    {
        Store&          store = GetStore();
        std::lock_guard lock(store.Mutex);
        store.Meshes.clear();
        store.Textures.clear();
        store.Environments.clear();
    }

    size_t GetPrefetchedBytes()
    {
        Store&          store = GetStore();
        std::lock_guard lock(store.Mutex);
        size_t          bytes = 0;
        for (const auto& [key, mesh] : store.Meshes)
            bytes += BytesOf(mesh);
        for (const auto& [key, texture] : store.Textures)
            bytes += texture.Pixels.size();
        for (const auto& [key, environment] : store.Environments)
            bytes += BytesOf(environment.Baked);
        return bytes;
    }
}
