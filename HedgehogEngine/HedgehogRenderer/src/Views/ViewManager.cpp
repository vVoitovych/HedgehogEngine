#include "HedgehogRenderer/Views/ViewManager.hpp"

#include "HedgehogRenderer/Views/ViewOrdering.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        ViewDesc DescFromCamera(const HX::RenderCamera& camera)
        {
            ViewDesc desc;
            desc.Camera    = camera;
            desc.Targets   = { camera.TargetMode == HX::CameraTargetMode::Main
                                   ? std::string(RenderTargetRegistry::MAIN_TARGET)
                                   : camera.TargetName };
            desc.GraphName = camera.GraphName;
            // A scene camera never sees the editor's overlay (the selection gizmo): only the editor's
            // own views may include that layer.
            desc.LayerMask = camera.LayerMask & ~HX::EDITOR_LAYER_MASK;
            desc.Priority  = camera.Priority;
            return desc;
        }

        std::string Label(ViewId id, const View& view)
        {
            std::string label = "view " + std::to_string(id);
            if (view.Origin == ViewOrigin::Derived)
                label += " (camera " + std::to_string(view.SourceId) + ")";
            if (!view.Desc.GraphName.empty())
                label += " ('" + view.Desc.GraphName + "')";
            return label;
        }
    }

    ViewId ViewManager::CreateView(ViewDesc desc)
    {
        const ViewId id = AllocateId();
        m_Descs[id] = { ViewOrigin::Application, 0, std::move(desc) };
        return id;
    }

    bool ViewManager::UpdateView(ViewId id, ViewDesc desc)
    {
        const auto it = m_Descs.find(id);
        if (it == m_Descs.end() || it->second.Origin != ViewOrigin::Application || m_PendingDestroy.contains(id))
            return false;
        it->second.Desc = std::move(desc);
        return true;
    }

    void ViewManager::DestroyView(ViewId id)
    {
        const auto it = m_Descs.find(id);
        if (it != m_Descs.end() && it->second.Origin == ViewOrigin::Application)
            m_PendingDestroy.insert(id);
    }

    void ViewManager::SetTargetOverride(uint64_t cameraSourceId, std::vector<std::string> targets)
    {
        m_TargetOverrides[cameraSourceId] = std::move(targets);
    }

    void ViewManager::ClearTargetOverride(uint64_t cameraSourceId)
    {
        m_TargetOverrides.erase(cameraSourceId);
    }

    const std::vector<View>& ViewManager::BuildViews(const HX::RenderScene& scene)
    {
        m_Built.clear();
        m_Dropped.clear();

        // 1. Reconcile derived views against this frame's cameras, by stable key.
        std::unordered_set<uint64_t> seen;
        for (const HX::RenderCamera& camera : scene.Cameras)
        {
            seen.insert(camera.SourceId);
            auto [slot, isNew] = m_DerivedBySource.try_emplace(camera.SourceId, INVALID_VIEW_ID);
            if (isNew)
                slot->second = AllocateId();

            Entry& entry   = m_Descs[slot->second];
            entry.Origin   = ViewOrigin::Derived;
            entry.SourceId = camera.SourceId;
            entry.Desc     = DescFromCamera(camera);
            if (const auto over = m_TargetOverrides.find(camera.SourceId); over != m_TargetOverrides.end())
                entry.Desc.Targets = over->second;
            m_PendingDestroy.erase(slot->second); // a camera that came back is live again
        }
        for (const auto& [sourceId, id] : m_DerivedBySource)
        {
            if (!seen.contains(sourceId))
                m_PendingDestroy.insert(id);
        }

        // 2. Build every live view: one path whatever the count. ViewId order only makes the build
        //    deterministic; the render order comes from step 3.
        std::vector<ViewId> ids;
        ids.reserve(m_Descs.size());
        for (const auto& [id, entry] : m_Descs)
        {
            if (!m_PendingDestroy.contains(id))
                ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        for (const ViewId id : ids)
            BuildOne(id, m_Descs.at(id));

        // 3. Order by render-target dependency; a cycle drops one view with a named error.
        m_Built = OrderViews(std::move(m_Built), m_Dropped);
        return m_Built;
    }

    void ViewManager::BuildOne(ViewId id, const Entry& entry)
    {
        View view;
        view.Id       = id;
        view.Origin   = entry.Origin;
        view.SourceId = entry.SourceId;
        view.Desc     = entry.Desc;

        const auto drop = [&](ViewDropReason reason, const std::string& why)
        {
            m_Dropped.push_back({ id, reason, Label(id, view) + " dropped: " + why });
        };

        if (!view.Desc.IsEnabled)
            return drop(ViewDropReason::Disabled, "disabled");
        if (view.Desc.Targets.empty())
            return drop(ViewDropReason::NoTargets, "it has no targets");

        view.ResolvedTargets.reserve(view.Desc.Targets.size());
        for (const std::string& target : view.Desc.Targets)
        {
            ResolvedRenderTarget resolved = m_Targets.Resolve(target);
            if (resolved.Status == RenderTargetStatus::Unknown)
                return drop(ViewDropReason::UnknownTarget, resolved.Message);
            if (resolved.Status == RenderTargetStatus::ZeroArea)
                return drop(ViewDropReason::ZeroAreaTarget, resolved.Message);
            view.ResolvedTargets.push_back(std::move(resolved));
        }
        m_Built.push_back(std::move(view));
    }

    void ViewManager::EndFrame()
    {
        for (const ViewId id : m_PendingDestroy)
        {
            const auto it = m_Descs.find(id);
            if (it == m_Descs.end())
                continue;
            if (it->second.Origin == ViewOrigin::Derived)
                m_DerivedBySource.erase(it->second.SourceId);
            m_Descs.erase(it);
        }
        m_PendingDestroy.clear();
    }
}
