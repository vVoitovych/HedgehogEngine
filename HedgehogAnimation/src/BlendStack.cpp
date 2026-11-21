#include "HedgehogAnimation/api/BlendStack.hpp"

#include "HedgehogAnimation/api/Pose.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogAnimation
{
    namespace
    {
        // How far the current clip has faded in, 0 to 1.
        float FadeProgress(const BlendStack& stack)
        {
            if (stack.Count <= 1 || stack.FadeDuration <= 0.0f)
                return 1.0f;
            return std::clamp(stack.FadeElapsed / stack.FadeDuration, 0.0f, 1.0f);
        }

        // Keeps only the current clip.
        void CollapseToCurrent(BlendStack& stack)
        {
            if (stack.Count == 0)
                return;
            stack.Entries[0]        = stack.Entries[stack.Count - 1];
            stack.Entries[0].Weight = 1.0f;
            stack.Count             = 1;
            stack.FadeDuration      = 0.0f;
            stack.FadeElapsed       = 0.0f;
        }

        // A tree entry's cycle in seconds, the weighted mean of its children's.
        float GetTreeCycle(const BlendEntry& entry)
        {
            float total = 0.0f;
            float cycle = 0.0f;
            for (uint32_t i = 0; i < entry.ChildCount; ++i)
            {
                const BlendEntryChild& child = entry.Children[i];
                total += child.Weight;
                cycle += child.Weight * child.Duration / (child.Speed > 0.0f ? child.Speed : 1.0f);
            }
            return total > 0.0f ? cycle / total : 0.0f;
        }

        // Samples one clip at time and folds it into the pose by weight.
        void Fold(const Skeleton& skeleton, const AnimationClip& clip, float time, bool loop, float weight,
                  float& accumulated, std::vector<JointTransform>& scratch, std::vector<JointTransform>& pose)
        {
            if (accumulated <= 0.0f)
            {
                SamplePose(skeleton, clip, time, loop, pose);
                accumulated = weight;
                return;
            }
            SamplePose(skeleton, clip, time, loop, scratch);
            accumulated += weight;
            BlendPoses(pose, scratch, weight / accumulated, pose);
        }

        void PushEntry(BlendStack& stack, const BlendEntry& entry, float fade)
        {
            if (fade <= 0.0f || stack.Count == 0)
            {
                ClearBlendStack(stack);
                stack.Entries[0] = entry;
                stack.Count      = 1;
                return;
            }

            // Every entry's present weight becomes its share of what fades out.
            std::array<float, MAX_BLEND_ENTRIES> weights{};
            for (uint32_t i = 0; i < stack.Count; ++i)
                weights[i] = GetEffectiveWeight(stack, i);
            for (uint32_t i = 0; i < stack.Count; ++i)
                stack.Entries[i].Weight = weights[i];

            if (stack.Count == MAX_BLEND_ENTRIES)
            {
                // Full: the oldest goes, and the rest share its weight.
                const float dropped = stack.Entries[0].Weight;
                for (uint32_t i = 1; i < stack.Count; ++i)
                    stack.Entries[i - 1] = stack.Entries[i];
                --stack.Count;
                const float remaining = 1.0f - dropped;
                for (uint32_t i = 0; i < stack.Count; ++i)
                    stack.Entries[i].Weight = remaining > 0.0f ? stack.Entries[i].Weight / remaining
                                                               : 1.0f / static_cast<float>(stack.Count);
            }

            stack.Entries[stack.Count++] = entry;
            stack.FadeDuration           = fade;
            stack.FadeElapsed            = 0.0f;
        }
    }

    void ClearBlendStack(BlendStack& stack)
    {
        stack = BlendStack{};
    }

    void PushClip(BlendStack& stack, uint32_t clip, float speed, bool loop, float fade)
    {
        PushEntry(stack, BlendEntry{ clip, 0.0f, speed, loop, 1.0f }, fade);
    }

    void PushBlendTree(BlendStack& stack, std::span<const BlendEntryChild> children, float speed, bool loop, float fade)
    {
        BlendEntry entry{ 0, 0.0f, speed, loop, 1.0f };
        entry.ChildCount = static_cast<uint32_t>(std::min<size_t>(children.size(), MAX_BLEND_TREE_CHILDREN));
        std::copy_n(children.begin(), entry.ChildCount, entry.Children.begin());
        PushEntry(stack, entry, fade);
    }

    void SetEntryWeights(BlendEntry& entry, std::span<const float> weights)
    {
        for (uint32_t i = 0; i < entry.ChildCount; ++i)
            entry.Children[i].Weight = i < weights.size() ? weights[i] : 0.0f;
    }

    void AdvanceBlendStack(BlendStack& stack, float clipDt, float fadeDt)
    {
        for (uint32_t i = 0; i < stack.Count; ++i)
        {
            BlendEntry& entry = stack.Entries[i];
            if (entry.ChildCount == 0)
            {
                entry.Time += clipDt * entry.Speed;
                continue;
            }
            const float cycle = GetTreeCycle(entry);
            if (cycle > 0.0f)
                entry.Time += clipDt * entry.Speed / cycle;
            entry.Time = entry.Loop ? entry.Time - std::floor(entry.Time) : std::clamp(entry.Time, 0.0f, 1.0f);
        }
        if (stack.Count <= 1)
            return;
        stack.FadeElapsed += fadeDt;
        if (stack.FadeElapsed >= stack.FadeDuration)
            CollapseToCurrent(stack);
    }

    BlendEntry* GetCurrentEntry(BlendStack& stack)
    {
        return stack.Count > 0 ? &stack.Entries[stack.Count - 1] : nullptr;
    }

    const BlendEntry* GetCurrentEntry(const BlendStack& stack)
    {
        return stack.Count > 0 ? &stack.Entries[stack.Count - 1] : nullptr;
    }

    bool IsFading(const BlendStack& stack)
    {
        return stack.Count > 1;
    }

    float GetEffectiveWeight(const BlendStack& stack, uint32_t entry)
    {
        if (entry >= stack.Count)
            return 0.0f;
        const float progress = FadeProgress(stack);
        if (entry == stack.Count - 1)
            return progress;
        return stack.Entries[entry].Weight * (1.0f - progress);
    }

    void EvaluateBlendStack(const BlendStack& stack, const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
                            std::vector<JointTransform>& scratch, std::vector<JointTransform>& outLocalPose)
    {
        // Folded oldest first: each entry blends in by its share of the weight so far.
        float accumulated = 0.0f;
        for (uint32_t i = 0; i < stack.Count; ++i)
        {
            const BlendEntry& entry  = stack.Entries[i];
            const float       weight = GetEffectiveWeight(stack, i);
            if (weight <= 0.0f)
                continue;
            if (entry.ChildCount == 0)
            {
                if (entry.Clip < clips.size())
                    Fold(skeleton, clips[entry.Clip], entry.Time, entry.Loop, weight, accumulated, scratch, outLocalPose);
                continue;
            }
            // A tree's children fold in one by one, each by its share of the entry's weight.
            for (uint32_t c = 0; c < entry.ChildCount; ++c)
            {
                const BlendEntryChild& child = entry.Children[c];
                if (child.Weight > 0.0f && child.Clip < clips.size())
                    Fold(skeleton, clips[child.Clip], entry.Time * child.Duration, entry.Loop, weight * child.Weight,
                         accumulated, scratch, outLocalPose);
            }
        }
        if (accumulated <= 0.0f)
            outLocalPose.assign(skeleton.BindPose.begin(), skeleton.BindPose.end());
    }
}
