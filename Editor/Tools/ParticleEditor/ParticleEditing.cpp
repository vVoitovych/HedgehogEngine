#include "ParticleEditing.hpp"

#include <algorithm>
#include <vector>

namespace Editor
{
    namespace
    {
        bool IsNameTaken(const HedgehogEngine::ParticleEffect& effect, const std::string& name, size_t except)
        {
            for (size_t i = 0; i < effect.Emitters.size(); ++i)
                if (i != except && effect.Emitters[i].Name == name)
                    return true;
            return false;
        }
    }

    std::string MakeUniqueEmitterName(const HedgehogEngine::ParticleEffect& effect, const std::string& base)
    {
        const std::string stem = base.empty() ? "Emitter" : base;
        std::string       name = stem;
        for (uint32_t n = 1; IsNameTaken(effect, name, effect.Emitters.size()); ++n)
            name = stem + " " + std::to_string(n);
        return name;
    }

    uint32_t AddEmitter(HedgehogEngine::ParticleEffect& effect)
    {
        HedgehogEngine::ParticleEmitter emitter = HedgehogEngine::MakeDefaultParticleEffect().Emitters.front();
        emitter.Name = MakeUniqueEmitterName(effect, emitter.Name);
        effect.Emitters.push_back(std::move(emitter));
        return static_cast<uint32_t>(effect.Emitters.size() - 1);
    }

    uint32_t DuplicateEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index)
    {
        if (index >= effect.Emitters.size())
            return index;
        HedgehogEngine::ParticleEmitter copy = effect.Emitters[index];
        copy.Name = MakeUniqueEmitterName(effect, copy.Name);
        effect.Emitters.push_back(std::move(copy));
        return static_cast<uint32_t>(effect.Emitters.size() - 1);
    }

    void DeleteEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index)
    {
        if (index >= effect.Emitters.size())
            return;
        effect.Emitters.erase(effect.Emitters.begin() + index);
        for (HedgehogEngine::ParticleEmitter& emitter : effect.Emitters)
        {
            std::erase_if(emitter.SubEmitters, [&](const HedgehogEngine::ParticleSubEmitter& sub) { return sub.Emitter == index; });
            for (HedgehogEngine::ParticleSubEmitter& sub : emitter.SubEmitters)
                if (sub.Emitter > index)
                    --sub.Emitter;
        }
    }

    void MoveEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t from, uint32_t to)
    {
        const size_t count = effect.Emitters.size();
        if (from >= count || to >= count || from == to)
            return;
        // Where each emitter goes, then the list rotated and every reference mapped.
        std::vector<uint32_t> moved(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (i == from)
                moved[i] = to;
            else if (from < to && i > from && i <= to)
                moved[i] = i - 1;
            else if (to < from && i >= to && i < from)
                moved[i] = i + 1;
            else
                moved[i] = i;
        }
        if (from < to)
            std::rotate(effect.Emitters.begin() + from, effect.Emitters.begin() + from + 1, effect.Emitters.begin() + to + 1);
        else
            std::rotate(effect.Emitters.begin() + to, effect.Emitters.begin() + from, effect.Emitters.begin() + from + 1);
        for (HedgehogEngine::ParticleEmitter& emitter : effect.Emitters)
            for (HedgehogEngine::ParticleSubEmitter& sub : emitter.SubEmitters)
                if (sub.Emitter < count)
                    sub.Emitter = moved[sub.Emitter];
    }

    bool RenameEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index, const std::string& name)
    {
        if (index >= effect.Emitters.size() || name.empty() || IsNameTaken(effect, name, index))
            return false;
        effect.Emitters[index].Name = name;
        return true;
    }

    bool SpawnsEmitter(const HedgehogEngine::ParticleEffect& effect, uint32_t from, uint32_t to)
    {
        std::vector<uint32_t> pending = { from };
        std::vector<bool>     seen(effect.Emitters.size(), false);
        while (!pending.empty())
        {
            const uint32_t current = pending.back();
            pending.pop_back();
            if (current == to)
                return true;
            if (current >= effect.Emitters.size() || seen[current])
                continue;
            seen[current] = true;
            for (const HedgehogEngine::ParticleSubEmitter& sub : effect.Emitters[current].SubEmitters)
                pending.push_back(sub.Emitter);
        }
        return false;
    }

    bool AddSubEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t parent, uint32_t child, uint32_t count)
    {
        if (parent >= effect.Emitters.size() || child >= effect.Emitters.size() || SpawnsEmitter(effect, child, parent))
            return false;
        effect.Emitters[parent].SubEmitters.push_back({ child, count });
        return true;
    }
}
