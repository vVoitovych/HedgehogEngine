#pragma once

#include "AnimationClip.hpp"
#include "Skeleton.hpp"

#include <array>
#include <cstdint>
#include <vector>

// The clips an animator is blending: the clip it plays now and those it is fading out of, as plain
// data and free functions. A switch during a fade keeps the half-finished blend fading out rather
// than dropping it, so the pose never jumps, and each clip keeps its own speed and loop.
namespace HedgehogAnimation
{
    // How many clips blend at once; a push past it drops the oldest.
    inline constexpr uint32_t MAX_BLEND_ENTRIES = 4;

    struct BlendEntry
    {
        uint32_t Clip   = 0;    // an index into the mesh's clips
        float    Time   = 0.0f; // seconds into the clip
        float    Speed  = 1.0f; // times the time the stack advances by
        bool     Loop   = true;
        float    Weight = 1.0f; // the entry's share of the fading-out entries (they sum to 1)
    };

    struct BlendStack
    {
        // Oldest first; Entries[Count - 1] is the current clip, fading in over FadeDuration.
        std::array<BlendEntry, MAX_BLEND_ENTRIES> Entries{};
        uint32_t                                  Count        = 0;
        float                                     FadeDuration = 0.0f;
        float                                     FadeElapsed  = 0.0f;
    };

    void ClearBlendStack(BlendStack& stack);

    // Starts clip from its start. With a fade above 0 (and something playing) it fades in over fade
    // seconds while everything else fades out from its present weight; otherwise it plays alone.
    void PushClip(BlendStack& stack, uint32_t clip, float speed, bool loop, float fade);

    // Advances every entry by clipDt times its speed and the fade by fadeDt; once the fade is
    // complete only the current clip is left.
    void AdvanceBlendStack(BlendStack& stack, float clipDt, float fadeDt);

    // The clip playing now, or nullptr for an empty stack.
    [[nodiscard]] BlendEntry*       GetCurrentEntry(BlendStack& stack);
    [[nodiscard]] const BlendEntry* GetCurrentEntry(const BlendStack& stack);

    // Whether older clips are still fading out.
    [[nodiscard]] bool IsFading(const BlendStack& stack);

    // An entry's weight in the blend now; the weights of a stack's entries sum to 1.
    [[nodiscard]] float GetEffectiveWeight(const BlendStack& stack, uint32_t entry);

    // The blended local pose: each entry sampled with its own loop and folded in by weight. An
    // entry whose clip index is out of range is skipped; an empty stack gives the bind pose.
    // scratch and outLocalPose are caller-owned, so once sized evaluation allocates nothing.
    void EvaluateBlendStack(const BlendStack& stack, const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
                            std::vector<JointTransform>& scratch, std::vector<JointTransform>& outLocalPose);
}
