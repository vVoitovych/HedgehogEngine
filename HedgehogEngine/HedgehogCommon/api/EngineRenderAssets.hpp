#pragma once

#include <array>
#include <string_view>

namespace HedgehogEngine
{
    // Where the renderer's own assets live, shared by the renderer (which loads them) and the
    // engine's asset dependency followers (which tell the cook tool to package them).

    // Engine graphs: a camera's GraphName without a path names <name>.graph in this folder.
    constexpr std::string_view ENGINE_GRAPH_DIRECTORY = "engine://HedgehogEngine/HedgehogRenderer/assets/Graphs";
    // The engine graphs the renderer needs to start: the editor's scene and result views and every
    // camera's default, game.
    constexpr std::array<std::string_view, 3> SHIPPED_GRAPHS = { "scene", "game", "result" };

    constexpr std::string_view DEPTH_PREPASS_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepass.shader";
    constexpr std::string_view DEPTH_PREPASS_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepassSkinned.shader";
    constexpr std::string_view DEPTH_PREPASS_CUTOFF_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepassCutoff.shader";
    constexpr std::string_view DEPTH_PREPASS_CUTOFF_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepassCutoffSkinned.shader";
    constexpr std::string_view SHADOW_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPass.shader";
    constexpr std::string_view SHADOW_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPassSkinned.shader";
    constexpr std::string_view SHADOW_CUTOFF_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPassCutoff.shader";
    constexpr std::string_view SHADOW_CUTOFF_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPassCutoffSkinned.shader";
    constexpr std::string_view FORWARD_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GraphForward.shader";
    constexpr std::string_view FORWARD_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GraphForwardSkinned.shader";
    constexpr std::string_view FORWARD_TRANSPARENT_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GraphForwardTransparent.shader";
    constexpr std::string_view FORWARD_TRANSPARENT_SKINNED_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GraphForwardTransparentSkinned.shader";
    constexpr std::string_view GIZMO_SHADER   = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/Gizmo.shader";
    constexpr std::string_view GAME_UI_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GameUi.shader";
    constexpr std::string_view DEBUG_LINES_SHADER =
        "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DebugLines.shader";
    constexpr std::string_view TONE_MAP_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ToneMap.shader";
    constexpr std::string_view SKYBOX_SHADER   = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/Skybox.shader";

    // The shaders an engine pass type (as a .graph file names it) draws with. "Shadow" is the
    // shared phase's pass: a graph that imports the shadow atlas needs it. "Ui" draws the
    // application's callback and has no shader of its own.
    struct PassTypeShader
    {
        std::string_view PassType;
        std::string_view Shader;
    };

    constexpr std::array<PassTypeShader, 18> PASS_TYPE_SHADERS = { {
        { "DepthPrepass", DEPTH_PREPASS_SHADER },
        { "DepthPrepass", DEPTH_PREPASS_SKINNED_SHADER },
        { "DepthPrepass", DEPTH_PREPASS_CUTOFF_SHADER },
        { "DepthPrepass", DEPTH_PREPASS_CUTOFF_SKINNED_SHADER },
        { "Shadow", SHADOW_SHADER },
        { "Shadow", SHADOW_SKINNED_SHADER },
        { "Shadow", SHADOW_CUTOFF_SHADER },
        { "Shadow", SHADOW_CUTOFF_SKINNED_SHADER },
        { "Forward", FORWARD_SHADER },
        { "Forward", FORWARD_SKINNED_SHADER },
        { "ForwardTransparent", FORWARD_TRANSPARENT_SHADER },
        { "ForwardTransparent", FORWARD_TRANSPARENT_SKINNED_SHADER },
        { "Skybox", SKYBOX_SHADER },
        { "ToneMap", TONE_MAP_SHADER },
        { "Gizmo", GIZMO_SHADER },
        { "Gizmo", DEBUG_LINES_SHADER },
        { "GameUi", GAME_UI_SHADER },
        { "Ui", {} },
    } };

    // The pass type whose shaders a graph importing the shadow atlas needs.
    constexpr std::string_view SHADOW_PASS_TYPE = "Shadow";
}
