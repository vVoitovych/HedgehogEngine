#pragma once

#include "View.hpp"

#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Renderer
{
    // Owns view lifetime (RENDERING.md sections 1 and 3.2): the views the application creates
    // directly and the views derived from every enabled camera in the extracted RenderScene.
    //
    // BuildViews reconciles derived views by their camera's SourceId, so a camera keeps the same
    // ViewId across frames and application views are never touched by reconciliation. It resolves
    // every target through the RenderTargetRegistry and drops, with a reason, any view that is
    // disabled, has no targets, or has a target that is unknown or zero-area. Views are dropped
    // here and nowhere later. The same loop handles 0, 1 or N views.
    //
    // The source camera is never mutated. The manager reads the scene through a const reference,
    // and an editor override of a derived view's targets (the game camera drawn into the editor's
    // game panel) is stored here and applied to the view, never written back to the component.
    //
    // DestroyView is deferred to EndFrame, like render-target resizes, so every view built this
    // frame stays valid until the frame ends. Nothing calls this yet; the frame loop does from
    // the frame-loop cutover onward.
    class ViewManager
    {
    public:
        explicit ViewManager(const RenderTargetRegistry& targets) : m_Targets(targets) {}

        // An application-owned view. Reconciliation never destroys it; only DestroyView does.
        [[nodiscard]] ViewId CreateView(ViewDesc desc);

        // Replaces an application view's description. False for an unknown or derived view.
        [[nodiscard]] bool UpdateView(ViewId id, ViewDesc desc);

        // Removes an application view at the end of the frame.
        void DestroyView(ViewId id);

        // Redirects the view derived from camera sourceId to other targets, leaving the camera
        // untouched. Applies from the next BuildViews until cleared.
        void SetTargetOverride(uint64_t cameraSourceId, std::vector<std::string> targets);
        void ClearTargetOverride(uint64_t cameraSourceId);

        // The build phase: reconciles derived views against scene.Cameras, resolves targets, and
        // returns every surviving view in ascending ViewId order. Valid until the next BuildViews.
        const std::vector<View>& BuildViews(const HX::RenderScene& scene);

        // The views the last BuildViews skipped, and why.
        std::span<const DroppedView> GetDroppedViews() const { return m_Dropped; }

        // Applies deferred destruction: application views passed to DestroyView, and derived
        // views whose camera disappeared from the scene.
        void EndFrame();

        size_t GetViewCount() const { return m_Descs.size(); }

    private:
        struct Entry
        {
            ViewOrigin Origin   = ViewOrigin::Application;
            uint64_t   SourceId = 0;
            ViewDesc   Desc;
        };

        ViewId AllocateId() { return ++m_LastId; }
        void   BuildOne(ViewId id, const Entry& entry);

        const RenderTargetRegistry& m_Targets;

        std::unordered_map<ViewId, Entry>                     m_Descs;
        std::unordered_map<uint64_t, ViewId>                  m_DerivedBySource;
        std::unordered_map<uint64_t, std::vector<std::string>> m_TargetOverrides;
        std::unordered_set<ViewId>                            m_PendingDestroy;

        std::vector<View>        m_Built;
        std::vector<DroppedView> m_Dropped;
        ViewId                   m_LastId = INVALID_VIEW_ID;
    };
}
