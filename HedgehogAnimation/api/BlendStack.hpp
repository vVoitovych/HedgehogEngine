#pragma once

#include "AnimationClip.hpp"
#include "AnimatorController.hpp"
#include "Skeleton.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// The clips an animator is blending: the clip it plays now and those it is fading out of, as plain
// data and free functions. A switch during a fade keeps the half-finished blend fading out rather
// than dropping it, so the pose never jumps, and each clip keeps its own speed and loop.
namespace HedgehogAnimation
{
    // How many clips blend at once; a push past it drops the oldest.
    inline constexpr uint32_t MAX_BLEND_ENTRIES = 4;

    // A clip of a blend tree entry.
    struct BlendEntryChild
    {
        uint32_t Clip     = 0;    // an index into the mesh's clips
        float    Weight   = 0.0f; // its share of the tree (SetEntryWeights)
        float    Duration = 0.0f; // the clip's length in seconds
        float    Speed    = 1.0f; // its own speed within the tree; its cycle is Duration / Speed
    };

    struct BlendEntry
    {
        uint32_t Clip   = 0;    // an index into the mesh's clips (a single-clip entry's)
        float    Time   = 0.0f; // seconds into the clip; a tree's normalized time, 0 to 1
        float    Speed  = 1.0f; // times the time the stack advances by
        bool     Loop   = true;
        float    Weight = 1.0f; // the entry's share of the fading-out entries (they sum to 1)
        // A blend tree entry's clips (none for a single clip). Its children play in step: each is
        // sampled at Time times its Duration, and Time advances by one cycle of the weighted mean of
        // their Duration / Speed.
        uint32_t                                                ChildCount = 0;
        std::array<BlendEntryChild, MAX_BLEND_TREE_CHILDREN>    Children{};
        // The caller's, kept with the entry as the stack moves it (an animator's state index).
        uint32_t Tag = 0;
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

    // Starts a blend tree entry of children (at most MAX_BLEND_TREE_CHILDREN; the rest are dropped)
    // from the start of its cycle, fading in as PushClip's clip does.
    void PushBlendTree(BlendStack& stack, std::span<const BlendEntryChild> children, float speed, bool loop, float fade);

    // Sets a tree entry's child weights (as ComputeBlendTreeWeights gives them) for this frame; the
    // weights past its children are ignored, its children past the weights get 0.
    void SetEntryWeights(BlendEntry& entry, std::span<const float> weights);

    // Advances every entry by clipDt times its speed (a tree entry's normalized time by that over its
    // weighted cycle, wrapped by its Loop or held at 1) and the fade by fadeDt; once the fade is
    // complete only the current clip is left.
    void AdvanceBlendStack(BlendStack& stack, float clipDt, float fadeDt);

    // The clip playing now, or nullptr for an empty stack.
    [[nodiscard]] BlendEntry*       GetCurrentEntry(BlendStack& stack);
    [[nodiscard]] const BlendEntry* GetCurrentEntry(const BlendStack& stack);

    // Whether older clips are still fading out.
    [[nodiscard]] bool IsFading(const BlendStack& stack);

    // An entry's weight in the blend now; the weights of a stack's entries sum to 1.
    [[nodiscard]] float GetEffectiveWeight(const BlendStack& stack, uint32_t entry);

    // The blended local pose: each entry sampled with its own loop and folded in by weight, a tree
    // entry's children each by its weight times their own. A clip index out of range is skipped; an empty stack gives the bind pose.
    // scratch and outLocalPose are caller-owned, so once sized evaluation allocates nothing.
    void EvaluateBlendStack(const BlendStack& stack, const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
                            std::vector<JointTransform>& scratch, std::vector<JointTransform>& outLocalPose);
}
