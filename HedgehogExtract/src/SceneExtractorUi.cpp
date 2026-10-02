#include "api/SceneExtractor.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"
#include "HedgehogUI/api/RectTransform.hpp"
#include "HedgehogUI/api/TextLayout.hpp"
#include "HedgehogUI/api/UiDraw.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace
{
    using HedgehogEngine::UiButtonComponent;
    using HedgehogEngine::UiCanvasComponent;
    using HedgehogEngine::UiImageComponent;
    using HedgehogEngine::UiRectComponent;
    using HedgehogEngine::UiTextComponent;

    // What every element of one canvas shares.
    struct CanvasPass
    {
        const ECS::ECS& Ecs;
        float           Scale = 1.0f; // pixels per canvas unit
        HX::UiRect      Scissor;      // the whole target, in pixels
        HX::RenderScene& Scene;
        HedgehogEngine::FontContainer* Fonts = nullptr;
    };

    bool OnEditorLayer(const ECS::ECS& ecs, ECS::Entity entity)
    {
        return ecs.HasComponent<HedgehogEngine::RenderComponent>(entity) &&
               ecs.GetComponent<HedgehogEngine::RenderComponent>(entity).Layer == HX::EDITOR_LAYER;
    }

    HM::Vector4 Multiply(const HM::Vector4& a, const HM::Vector4& b)
    {
        return HM::Vector4(a.x() * b.x(), a.y() * b.y(), a.z() * b.z(), a.w() * b.w());
    }

    const HM::Vector4& ButtonTint(const UiButtonComponent& button)
    {
        if (!button.IsInteractable)
            return button.DisabledTint;
        switch (button.State)
        {
        case HedgehogEngine::UiButtonState::Hovered: return button.HoverTint;
        case HedgehogEngine::UiButtonState::Pressed: return button.PressedTint;
        case HedgehogEngine::UiButtonState::Normal:  break;
        }
        return button.NormalTint;
    }

    // The index of path in textures, appending it when new; UI_NO_TEXTURE for no path.
    uint32_t TextureIndex(std::vector<std::string>& textures, const std::string& path)
    {
        if (path.empty())
            return HX::UI_NO_TEXTURE;
        const auto found = std::find(textures.begin(), textures.end(), path);
        if (found != textures.end())
            return static_cast<uint32_t>(found - textures.begin());
        textures.push_back(path);
        return static_cast<uint32_t>(textures.size() - 1);
    }

    // The slot of font index in fonts, appending it when new, as a UI_FONT_TEXTURE Texture value.
    uint32_t FontTexture(std::vector<uint32_t>& fonts, size_t font)
    {
        const auto found = std::find(fonts.begin(), fonts.end(), static_cast<uint32_t>(font));
        if (found != fonts.end())
            return HX::UI_FONT_TEXTURE | static_cast<uint32_t>(found - fonts.begin());
        fonts.push_back(static_cast<uint32_t>(font));
        return HX::UI_FONT_TEXTURE | static_cast<uint32_t>(fonts.size() - 1);
    }

    // An element's text, laid out in rect (canvas units) with its font baked at its size in pixels.
    void EmitText(const CanvasPass& pass, const UiTextComponent& text, const HX::UiRect& rect)
    {
        if (!pass.Fonts || text.Text.empty())
            return;
        const long pixelSize = std::lround(text.FontSize * pass.Scale);
        if (pixelSize <= 0)
            return;
        const std::optional<size_t> font = pass.Fonts->FindOrBake(text.Font, static_cast<uint32_t>(pixelSize));
        if (!font)
            return;

        HUI::TextStyle style;
        style.Align         = static_cast<HUI::TextAlign>(text.Align);
        style.VerticalAlign = static_cast<HUI::TextVerticalAlign>(text.VerticalAlign);
        style.Wrap          = text.Wrap;
        style.Color         = HUI::PackColor(text.Color.x(), text.Color.y(), text.Color.z(), text.Color.w());
        style.Texture       = FontTexture(pass.Scene.UiFonts, *font);
        HUI::LayoutText(pass.Scene.Ui, pass.Fonts->GetFont(*font), text.Text, HUI::ToPixels(rect, pass.Scale), style,
                        pass.Scissor);
    }

    HUI::RectTransform ToRectTransform(const UiRectComponent& rect)
    {
        return { rect.AnchorMin, rect.AnchorMax, rect.Pivot, rect.Offset, rect.Size };
    }

    void EmitChildren(const CanvasPass& pass, ECS::Entity parent, const HX::UiRect& parentRect);

    void EmitElement(const CanvasPass& pass, ECS::Entity entity, const HX::UiRect& parentRect)
    {
        const ECS::ECS& ecs = pass.Ecs;
        if (!ecs.HasComponent<UiRectComponent>(entity) || ecs.HasComponent<UiCanvasComponent>(entity))
            return;
        const UiRectComponent& rectComponent = ecs.GetComponent<UiRectComponent>(entity);
        if (!rectComponent.IsVisible || OnEditorLayer(ecs, entity))
            return;

        const HX::UiRect rect = HUI::ResolveRect(ToRectTransform(rectComponent), parentRect);
        if (ecs.HasComponent<UiImageComponent>(entity))
        {
            const UiImageComponent& image = ecs.GetComponent<UiImageComponent>(entity);
            HM::Vector4             color = image.Color;
            if (ecs.HasComponent<UiButtonComponent>(entity))
                color = Multiply(color, ButtonTint(ecs.GetComponent<UiButtonComponent>(entity)));

            HUI::UiQuad quad;
            quad.Rect    = HUI::ToPixels(rect, pass.Scale);
            quad.Color   = HUI::PackColor(color.x(), color.y(), color.z(), color.w());
            quad.Texture = TextureIndex(pass.Scene.UiTextures, image.Texture);
            HUI::AppendQuad(pass.Scene.Ui, quad, pass.Scissor);
        }
        if (ecs.HasComponent<UiTextComponent>(entity))
            EmitText(pass, ecs.GetComponent<UiTextComponent>(entity), rect);
        EmitChildren(pass, entity, rect);
    }

    void EmitChildren(const CanvasPass& pass, ECS::Entity parent, const HX::UiRect& parentRect)
    {
        if (!pass.Ecs.HasComponent<ECS::HierarchyComponent>(parent))
            return;
        for (const ECS::Entity child : pass.Ecs.GetComponent<ECS::HierarchyComponent>(parent).Children)
            EmitElement(pass, child, parentRect);
    }

    void EmitCanvas(const ECS::ECS& ecs, ECS::Entity entity, const HM::Vector2& targetSize, HX::RenderScene& scene,
                    HedgehogEngine::FontContainer* fonts)
    {
        const UiCanvasComponent& canvas = ecs.GetComponent<UiCanvasComponent>(entity);
        HUI::CanvasScaler        scaler;
        scaler.Mode = canvas.ScaleMode == HedgehogEngine::UiCanvasScaleMode::ScaleWithTargetSize
                          ? HUI::CanvasScaleMode::ScaleWithTargetSize
                          : HUI::CanvasScaleMode::ConstantPixelSize;
        scaler.ReferenceResolution = canvas.ReferenceResolution;
        scaler.MatchWidthOrHeight  = canvas.MatchWidthOrHeight;

        const float      scale = HUI::ComputeCanvasScale(scaler, targetSize);
        const CanvasPass pass{ ecs, scale, { 0.0f, 0.0f, targetSize.x(), targetSize.y() }, scene, fonts };
        EmitChildren(pass, entity, HUI::CanvasRect(targetSize, scale));
    }
}

namespace HX
{
    void SceneExtractor::ExtractUi(const ECS::ECS& ecs, const HedgehogEngine::UiSystem& uiSystem,
                                   const HM::Vector2& targetSize, RenderScene& outScene,
                                   HedgehogEngine::FontContainer* fonts) const
    {
        outScene.UiTargetSize = targetSize;
        if (!(targetSize.x() > 0.0f && targetSize.y() > 0.0f))
            return;

        // Canvases in (SortOrder, entity) order, found one at a time so nothing is sorted or allocated.
        using Key = std::pair<int32_t, ECS::Entity>;
        const auto keyOf = [&](ECS::Entity entity)
        { return Key{ ecs.GetComponent<UiCanvasComponent>(entity).SortOrder, entity }; };

        std::optional<Key> previous;
        while (true)
        {
            std::optional<Key> next;
            for (const ECS::Entity entity : uiSystem.GetEntities())
            {
                const Key key = keyOf(entity);
                if ((!previous || *previous < key) && (!next || key < *next))
                    next = key;
            }
            if (!next)
                break;
            previous = next;

            const ECS::Entity entity = next->second;
            if (ecs.GetComponent<UiCanvasComponent>(entity).IsEnabled && !OnEditorLayer(ecs, entity))
                EmitCanvas(ecs, entity, targetSize, outScene, fonts);
        }
    }
}
