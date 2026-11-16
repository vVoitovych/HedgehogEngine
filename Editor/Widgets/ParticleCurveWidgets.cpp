#include "ParticleCurveWidgets.hpp"

#include "Tools/ParticleEditor/CurveEditing.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Editor
{
    namespace
    {
        constexpr float CURVE_HEIGHT   = 90.0f;
        constexpr float STRIP_HEIGHT   = 20.0f;
        constexpr float MARKER_HEIGHT  = 12.0f;
        constexpr float KEY_RADIUS     = 4.5f;
        constexpr float PICK_RADIUS    = 8.0f; // pixels
        constexpr int   CURVE_SEGMENTS = 64;
        constexpr int   STRIP_SEGMENTS = 48;

        constexpr ImU32 BACKGROUND = IM_COL32(30, 32, 36, 255);
        constexpr ImU32 GRID       = IM_COL32(70, 74, 80, 255);
        constexpr ImU32 LINE       = IM_COL32(110, 190, 255, 255);
        constexpr ImU32 KEY        = IM_COL32(230, 230, 230, 255);
        constexpr ImU32 ACTIVE_KEY = IM_COL32(255, 200, 80, 255);

        constexpr std::array<const char*, 4> PRESET_NAMES = { "Constant", "Fade in", "Fade out", "Ease in and out" };

        ImU32 ToColor(const HM::Vector4& c)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(std::clamp(c.x(), 0.0f, 1.0f), std::clamp(c.y(), 0.0f, 1.0f),
                                                         std::clamp(c.z(), 0.0f, 1.0f), std::clamp(c.w(), 0.0f, 1.0f)));
        }
    }

    bool ParticleCurveEditor(const char* id, HedgehogEngine::ParticleCurve& curve)
    {
        ImGui::PushID(id);
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID dragId  = ImGui::GetID("drag");
        const ImGuiID topId   = ImGui::GetID("top");

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 40.0f), CURVE_HEIGHT);
        ImGui::InvisibleButton("##curve", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool active  = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();

        // The value range, held while a key is dragged so the drag does not rescale under it.
        float top = 1.0f;
        for (const HedgehogEngine::ParticleCurveKey& key : curve.Keys)
            top = std::max(top, std::ceil(key.Value));
        if (active)
            top = std::max(top, storage->GetFloat(topId, top));
        storage->SetFloat(topId, top);

        const auto toScreen = [&](float time, float value)
        { return ImVec2(origin.x + time * size.x, origin.y + size.y - value / top * size.y); };
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float  time  = (mouse.x - origin.x) / size.x;
        const float  value = std::max(0.0f, (origin.y + size.y - mouse.y) / size.y * top);
        const float  scale = top * size.x / size.y; // a value step drawn as long as this time step
        const std::optional<size_t> under = FindCurveKey(curve, time, value, PICK_RADIUS / size.x, scale);

        bool changed = false;
        int  dragged = storage->GetInt(dragId, -1);
        if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            dragged = under ? static_cast<int>(*under) : -1;
        if (active && dragged >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
        {
            dragged = static_cast<int>(MoveCurveKey(curve, static_cast<size_t>(dragged), time, value));
            changed = true;
        }
        if (!active)
            dragged = -1;
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !under)
            changed = AddCurveKey(curve, time, value).has_value() || changed;
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            if (under)
            {
                DeleteCurveKey(curve, *under);
                changed = true;
            }
            else
            {
                ImGui::OpenPopup("presets");
            }
        }
        storage->SetInt(dragId, dragged);
        if (ImGui::BeginPopup("presets"))
        {
            for (size_t i = 0; i < PRESET_NAMES.size(); ++i)
                if (ImGui::MenuItem(PRESET_NAMES[i]))
                {
                    curve   = MakeCurvePreset(static_cast<CurvePreset>(i));
                    changed = true;
                }
            ImGui::EndPopup();
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), BACKGROUND);
        draw->AddLine(toScreen(0.0f, 1.0f), toScreen(1.0f, 1.0f), GRID);
        std::array<ImVec2, CURVE_SEGMENTS + 1> points{};
        for (int i = 0; i <= CURVE_SEGMENTS; ++i)
        {
            const float t = static_cast<float>(i) / CURVE_SEGMENTS;
            points[i]     = toScreen(t, HedgehogEngine::EvaluateCurve(curve, t));
        }
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), LINE, ImDrawFlags_None, 2.0f);
        for (size_t i = 0; i < curve.Keys.size(); ++i)
        {
            const bool highlighted = static_cast<int>(i) == dragged || (under && *under == i && hovered);
            draw->AddCircleFilled(toScreen(curve.Keys[i].Time, curve.Keys[i].Value), KEY_RADIUS, highlighted ? ACTIVE_KEY : KEY);
        }
        if (hovered)
            ImGui::SetTooltip(under ? "t %.2f  value %.2f (drag; right-click deletes)" : "t %.2f  value %.2f (double-click adds a key)",
                              under ? curve.Keys[*under].Time : time, under ? curve.Keys[*under].Value : value);
        ImGui::PopID();
        return changed;
    }

    bool ParticleGradientEditor(const char* id, HedgehogEngine::ParticleGradient& gradient)
    {
        ImGui::PushID(id);
        ImGuiStorage* storage    = ImGui::GetStateStorage();
        const ImGuiID selectedId = ImGui::GetID("selected");

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 40.0f), STRIP_HEIGHT + MARKER_HEIGHT);
        ImGui::InvisibleButton("##gradient", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool active  = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();

        const float mouseX = ImGui::GetIO().MousePos.x;
        const float time   = (mouseX - origin.x) / size.x;
        // The stop whose marker is nearest the pointer, within the pick radius.
        std::optional<size_t> under;
        float                 best = PICK_RADIUS;
        for (size_t i = 0; i < gradient.Keys.size(); ++i)
        {
            const float distance = std::abs(origin.x + gradient.Keys[i].Time * size.x - mouseX);
            if (distance <= best)
            {
                best  = distance;
                under = i;
            }
        }

        bool changed  = false;
        int  selected = storage->GetInt(selectedId, -1);
        if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && under)
            selected = static_cast<int>(*under);
        if (active && selected >= 0 && selected < static_cast<int>(gradient.Keys.size())
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
        {
            selected = static_cast<int>(MoveGradientKey(gradient, static_cast<size_t>(selected), time));
            changed  = true;
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !under)
            if (const std::optional<size_t> added = AddGradientKey(gradient, time))
            {
                selected = static_cast<int>(*added);
                changed  = true;
            }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && under)
        {
            DeleteGradientKey(gradient, *under);
            selected = -1;
            changed  = true;
        }

        ImDrawList* draw  = ImGui::GetWindowDrawList();
        const float step  = size.x / STRIP_SEGMENTS;
        for (int i = 0; i < STRIP_SEGMENTS; ++i)
        {
            const HM::Vector4 color = HedgehogEngine::EvaluateGradient(gradient, (static_cast<float>(i) + 0.5f) / STRIP_SEGMENTS);
            draw->AddRectFilled(ImVec2(origin.x + step * i, origin.y), ImVec2(origin.x + step * (i + 1), origin.y + STRIP_HEIGHT),
                                ToColor(color));
        }
        for (size_t i = 0; i < gradient.Keys.size(); ++i)
        {
            const float x   = origin.x + gradient.Keys[i].Time * size.x;
            const float y   = origin.y + STRIP_HEIGHT;
            const ImU32 rim = static_cast<int>(i) == selected ? ACTIVE_KEY : KEY;
            draw->AddTriangleFilled(ImVec2(x, y), ImVec2(x - 5.0f, y + MARKER_HEIGHT - 1.0f), ImVec2(x + 5.0f, y + MARKER_HEIGHT - 1.0f), rim);
            draw->AddRectFilled(ImVec2(x - 3.0f, y + 4.0f), ImVec2(x + 3.0f, y + MARKER_HEIGHT - 2.0f), ToColor(gradient.Keys[i].Color));
        }
        if (hovered)
            ImGui::SetTooltip(under ? "Drag to move; right-click deletes" : "Double-click adds a stop");

        // The selected stop's colour, linear and HDR as the effect stores it.
        if (selected >= 0 && selected < static_cast<int>(gradient.Keys.size()))
        {
            ImGui::SetNextItemWidth(-1.0f);
            changed = ImGui::ColorEdit4("##stop", &gradient.Keys[static_cast<size_t>(selected)].Color[0],
                                        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR) || changed;
        }
        else
        {
            selected = -1;
        }
        storage->SetInt(selectedId, selected);
        ImGui::PopID();
        return changed;
    }
}
