#include "ParticleEditorWindow.hpp"

#include "ParticleEditing.hpp"

#include "Panels/AssetDragDrop.hpp"
#include "Panels/MaterialSlots.hpp"
#include "Widgets/ParticleCurveWidgets.hpp"
#include "Widgets/PropertyFields.hpp"

#include "DialogueWindows/api/ParticleEffectDialogue.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>

namespace Editor
{
    namespace
    {
        using HedgehogEngine::ParticleEmitter;

        constexpr float  LIST_WIDTH        = 220.0f;
        constexpr size_t MAX_LISTED_ROWS   = 4;
        constexpr ImVec4 PROBLEM_COLOR     = { 1.0f, 0.45f, 0.4f, 1.0f };
        constexpr const char* UNSAVED_POPUP   = "Unsaved particle effect changes";
        constexpr const char* UNTITLED        = "untitled";
        constexpr const char* DIALOGUE_FOLDER = "assets://";

        constexpr std::array<const char*, 4> SHAPES     = { "Point", "Sphere", "Cone", "Box" };
        constexpr std::array<const char*, 2> BLENDS     = { "Alpha", "Additive" };
        constexpr std::array<const char*, 2> SPACES     = { "Local", "World" };
        constexpr std::array<const char*, 3> COLLISIONS = { "None", "HeightPlane", "DepthBuffer" };

        template <typename Enum, size_t N>
        bool EnumCombo(const char* id, Enum& value, const std::array<const char*, N>& names)
        {
            int index = static_cast<int>(value);
            if (!ImGui::Combo(id, &index, names.data(), static_cast<int>(N)))
                return false;
            value = static_cast<Enum>(index);
            return true;
        }

        bool RangeField(const char* label, HedgehogEngine::FloatRange& range, float speed)
        {
            PropertyLabel(label);
            float values[2] = { range.Min, range.Max };
            if (!ImGui::DragFloat2((std::string("##") + label).c_str(), values, speed))
                return false;
            range = { values[0], values[1] };
            return true;
        }

        bool FloatField(const char* label, float& value, float speed, float min = 0.0f, float max = 0.0f)
        {
            PropertyLabel(label);
            return ImGui::DragFloat((std::string("##") + label).c_str(), &value, speed, min, max);
        }

        bool UIntField(const char* label, uint32_t& value)
        {
            PropertyLabel(label);
            return ImGui::InputScalar((std::string("##") + label).c_str(), ImGuiDataType_U32, &value);
        }

    }

    void ParticleEditorWindow::Draw(const FS::FileSystemManager& fileSystem, HedgehogEngine::ParticleEffectContainer& effects)
    {
        m_Effects = &effects;
        if (!Open)
            return;
        if (!m_LoadPath.empty())
        {
            Load(m_LoadPath, fileSystem);
            m_LoadPath.clear();
        }

        std::string title = "Particle Effect";
        if (IsUntitled())
            title += std::string(" - ") + UNTITLED;
        else if (!m_Path.empty())
            title += " - " + std::filesystem::path(m_Path).filename().string();
        title += IsDirty() ? "*" : "";
        title += "###ParticleEffectEditor";

        bool keepOpen = true;
        ImGui::SetNextWindowSize({ 760.0f, 640.0f }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &keepOpen))
        {
            DrawToolbar(fileSystem);
            if (m_HasEffect)
            {
                const float problemsHeight = m_Problems.empty() ? 0.0f
                    : ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(std::min(m_Problems.size(), MAX_LISTED_ROWS) + 1)
                      + ImGui::GetStyle().WindowPadding.y * 2.0f;
                ImGui::BeginChild("ParticleEmitters", ImVec2(LIST_WIDTH, -problemsHeight), ImGuiChildFlags_Borders);
                DrawEmitterList();
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("ParticleEmitterFields", ImVec2(0.0f, -problemsHeight), ImGuiChildFlags_Borders);
                if (m_Selected < m_Edited.Emitters.size())
                {
                    bool changed = DrawEmitter(m_Edited.Emitters[m_Selected]);
                    changed      = DrawSubEmitters(m_Selected) || changed;
                    if (changed)
                        ApplyEdit();
                }
                ImGui::EndChild();
                if (!m_Problems.empty())
                {
                    ImGui::BeginChild("ParticleProblems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                    DrawProblems();
                    ImGui::EndChild();
                }
            }

            if (IsDirty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
                Save(fileSystem);
            DrawUnsavedChangesPopup(fileSystem);
        }
        ImGui::End();

        // A valid edit shows in a preview or Play at once, through the container, once nothing is
        // being dragged or typed (each one starts the effect again).
        if (m_HasEffect && !m_Path.empty() && m_EditedText != m_PreviewText && m_Problems.empty() && !ImGui::IsAnyItemActive())
        {
            effects.SetOverride(m_Path, m_Edited);
            m_PreviewText = m_EditedText;
            m_PreviewPath = m_Path;
        }

        if (!keepOpen)
        {
            if (IsDirty())
                m_Pending = PendingAction::Close;
            else
                Open = false;
        }
    }

    void ParticleEditorWindow::OpenEffect(const std::string& virtualPath)
    {
        Open = true;
        RequestOpen(virtualPath);
    }

    // ── File lifetime ────────────────────────────────────────────────────────

    void ParticleEditorWindow::DrawToolbar(const FS::FileSystemManager& fileSystem)
    {
        if (ImGui::Button("New"))
            RequestNew();
        ImGui::SameLine();
        if (ImGui::Button("Open..."))
            OpenFile(fileSystem);
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsDirty() && !IsUntitled());
        if (ImGui::Button("Save"))
            Save(fileSystem);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_HasEffect);
        if (ImGui::Button("Save As..."))
            SaveAs(fileSystem);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(IsUntitled() ? UNTITLED : m_Path.empty() ? "(open a particle effect)" : m_Path.c_str());
        if (!m_Status.empty())
            ImGui::TextUnformatted(m_Status.c_str());
    }

    void ParticleEditorWindow::DrawUnsavedChangesPopup(const FS::FileSystemManager& fileSystem)
    {
        if (m_Pending != PendingAction::None && !ImGui::IsPopupOpen(UNSAVED_POPUP))
            ImGui::OpenPopup(UNSAVED_POPUP);
        if (!ImGui::BeginPopupModal(UNSAVED_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::Text("'%s' has unsaved changes.", IsUntitled() ? UNTITLED : m_Path.c_str());
        bool proceed = false;
        if (ImGui::Button("Save"))
        {
            Save(fileSystem);
            proceed = !IsDirty(); // a failed save keeps the question open
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            // Read the file again, so reopening the window shows what it holds; an untitled effect is dropped.
            m_EditedText = m_SavedText;
            m_LoadPath   = m_Path;
            ClearPreview();
            m_HasEffect  = !m_Path.empty();
            proceed      = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }

        if (proceed)
        {
            if (m_Pending == PendingAction::OpenEffect)
                m_LoadPath = m_PendingPath;
            else if (m_Pending == PendingAction::NewEffect)
                NewEffect();
            else
                Open = false;
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void ParticleEditorWindow::RequestOpen(const std::string& virtualPath)
    {
        if (m_HasEffect && FS::MakeAssetKey(virtualPath) == FS::MakeAssetKey(m_Path))
            return;
        if (IsDirty())
        {
            m_Pending     = PendingAction::OpenEffect;
            m_PendingPath = virtualPath;
            return;
        }
        m_LoadPath = virtualPath;
    }

    void ParticleEditorWindow::RequestNew()
    {
        if (IsDirty())
            m_Pending = PendingAction::NewEffect;
        else
            NewEffect();
    }

    void ParticleEditorWindow::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const std::string                path = FS::ToAssetVirtualPath(virtualPath);
        const std::optional<std::string> text = fileSystem.ReadTextFile(path);
        if (!text)
        {
            m_Status = path + " cannot be read.";
            return;
        }
        HedgehogEngine::ParticleEffectReadResult parsed = HedgehogEngine::ParseParticleEffect(*text);
        if (!parsed.Effect)
        {
            m_Status = path + " cannot be opened: " + parsed.Error;
            return;
        }
        ClearPreview();
        m_Edited    = std::move(*parsed.Effect);
        m_Path      = path;
        m_File      = fileSystem.ResolvePhysical(path).value_or(std::filesystem::path{});
        m_HasEffect = true;
        m_Selected  = 0;
        m_Status.clear();
        ApplyEdit();
        m_SavedText   = m_EditedText;
        m_PreviewText = m_EditedText;
    }

    void ParticleEditorWindow::NewEffect()
    {
        ClearPreview();
        m_Edited = HedgehogEngine::MakeDefaultParticleEffect();
        m_Path.clear();
        m_File.clear();
        m_HasEffect = true;
        m_Selected  = 0;
        m_Status.clear();
        ApplyEdit();
        m_SavedText = m_EditedText;
    }

    void ParticleEditorWindow::OpenFile(const FS::FileSystemManager& fileSystem)
    {
        const std::optional<std::filesystem::path> folder = fileSystem.ResolvePhysical(DIALOGUE_FOLDER);
        const std::string start  = folder ? std::filesystem::path(*folder).make_preferred().string() + "\\" : std::string();
        const char*       picked = DialogueWindows::ParticleEffectOpenDialogue(start.c_str());
        if (!picked)
            return;
        const std::optional<std::string> virtualPath = fileSystem.ToVirtualPath(picked);
        if (!virtualPath)
        {
            m_Status = std::string(picked) + " is not under a mounted folder, so no particle system can use it.";
            return;
        }
        RequestOpen(*virtualPath);
    }

    void ParticleEditorWindow::Save(const FS::FileSystemManager& fileSystem)
    {
        if (m_Path.empty())
            SaveAs(fileSystem);
        else if (Write(m_Path, fileSystem))
            m_SavedText = m_EditedText;
    }

    void ParticleEditorWindow::ClearPreview()
    {
        if (m_Effects && !m_PreviewPath.empty())
            m_Effects->ClearOverride(m_PreviewPath);
        m_PreviewPath.clear();
        m_PreviewText = m_EditedText;
    }

    void ParticleEditorWindow::SaveAs(const FS::FileSystemManager& fileSystem)
    {
        std::string start = m_File.string();
        if (start.empty())
        {
            const std::optional<std::filesystem::path> folder = fileSystem.ResolvePhysical(DIALOGUE_FOLDER);
            start = folder ? (*folder / "Untitled.particle").make_preferred().string() : "Untitled.particle";
        }
        const char* picked = DialogueWindows::ParticleEffectSaveDialogue(start.c_str());
        if (!picked)
            return;
        std::filesystem::path file(picked);
        if (file.extension() != HedgehogEngine::PARTICLE_EFFECT_EXTENSION)
            file += HedgehogEngine::PARTICLE_EFFECT_EXTENSION;
        const std::optional<std::string> virtualPath = fileSystem.ToVirtualPath(file);
        if (!virtualPath)
        {
            m_Status = file.string() + " is not under a mounted folder, so no particle system could use it.";
            return;
        }
        if (!Write(*virtualPath, fileSystem))
            return;
        m_Path      = *virtualPath;
        m_File      = file;
        m_SavedText = m_EditedText;
    }

    bool ParticleEditorWindow::Write(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        if (!fileSystem.WriteTextFile(virtualPath, m_EditedText))
        {
            m_Status = "Save failed: " + virtualPath + " cannot be written.";
            return false;
        }
        m_Status = "Saved " + virtualPath + ".";
        // The file now holds the edit; the container reads it again.
        ClearPreview();
        if (!m_Problems.empty())
            m_Status += " It has " + std::to_string(m_Problems.size()) + " problem(s), so it does not load until they are fixed.";
        return true;
    }

    void ParticleEditorWindow::ApplyEdit()
    {
        m_EditedText = HedgehogEngine::WriteParticleEffect(m_Edited);
        m_Problems   = HedgehogEngine::ValidateParticleEffect(m_Edited);
        m_Selected   = std::min<uint32_t>(m_Selected, m_Edited.Emitters.empty() ? 0 : static_cast<uint32_t>(m_Edited.Emitters.size() - 1));
        m_NameBuffer = m_Selected < m_Edited.Emitters.size() ? m_Edited.Emitters[m_Selected].Name : std::string();
    }

    bool ParticleEditorWindow::HasProblems(const ParticleEmitter& emitter) const
    {
        const std::string prefix = "Emitter '" + emitter.Name + "':";
        return std::any_of(m_Problems.begin(), m_Problems.end(), [&](const std::string& p) { return p.starts_with(prefix); });
    }

    // ── Form ─────────────────────────────────────────────────────────────────

    void ParticleEditorWindow::DrawEmitterList()
    {
        for (uint32_t i = 0; i < m_Edited.Emitters.size(); ++i)
        {
            const bool problems = HasProblems(m_Edited.Emitters[i]);
            if (problems)
                ImGui::PushStyleColor(ImGuiCol_Text, PROBLEM_COLOR);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(m_Edited.Emitters[i].Name.c_str(), i == m_Selected))
            {
                m_Selected   = i;
                m_NameBuffer = m_Edited.Emitters[i].Name;
            }
            ImGui::PopID();
            if (problems)
                ImGui::PopStyleColor();
        }

        ImGui::Separator();
        const uint32_t count = static_cast<uint32_t>(m_Edited.Emitters.size());
        bool           edited = false;
        if (ImGui::Button("Add"))
        {
            m_Selected = AddEmitter(m_Edited);
            edited     = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected >= count);
        if (ImGui::Button("Duplicate"))
        {
            m_Selected = DuplicateEmitter(m_Edited, m_Selected);
            edited     = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            DeleteEmitter(m_Edited, m_Selected);
            edited = true;
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(m_Selected == 0 || m_Selected >= count);
        if (ImGui::ArrowButton("##Up", ImGuiDir_Up))
        {
            MoveEmitter(m_Edited, m_Selected, m_Selected - 1);
            --m_Selected;
            edited = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected + 1 >= count);
        if (ImGui::ArrowButton("##Down", ImGuiDir_Down))
        {
            MoveEmitter(m_Edited, m_Selected, m_Selected + 1);
            ++m_Selected;
            edited = true;
        }
        ImGui::EndDisabled();
        if (edited)
            ApplyEdit();
    }

    bool ParticleEditorWindow::DrawEmitter(ParticleEmitter& emitter)
    {
        bool changed = false;
        if (!BeginPropertyTable("##ParticleEmitter"))
            return false;

        // A name is taken as typed once no other emitter has it.
        PropertyLabel("Name");
        const bool nameTaken = m_NameBuffer != emitter.Name;
        if (nameTaken)
            ImGui::PushStyleColor(ImGuiCol_Text, PROBLEM_COLOR);
        char name[128] = {};
        m_NameBuffer.copy(name, sizeof(name) - 1);
        if (ImGui::InputText("##Name", name, sizeof(name)))
        {
            m_NameBuffer = name;
            if (RenameEmitter(m_Edited, m_Selected, m_NameBuffer))
                changed = true;
        }
        if (nameTaken)
        {
            ImGui::PopStyleColor();
            ImGui::SetItemTooltip("Empty, or another emitter's name: not applied.");
        }

        changed |= UIntField("Max particles", emitter.MaxParticles);
        changed |= FloatField("Rate", emitter.Rate, 0.5f, 0.0f, 100000.0f);
        changed |= FloatField("Duration", emitter.Duration, 0.05f, 0.0f, 3600.0f);
        PropertyLabel("Loop");
        changed |= ImGui::Checkbox("##Loop", &emitter.Loop);

        PropertyLabel("Shape");
        changed |= EnumCombo("##Shape", emitter.Shape, SHAPES);
        changed |= FloatField("Radius", emitter.Radius, 0.01f, 0.0f, 1000.0f);
        changed |= FloatField("Angle", emitter.Angle, 0.5f, 0.0f, 180.0f);
        PropertyLabel("Box size");
        changed |= ImGui::DragFloat3("##BoxSize", &emitter.BoxSize[0], 0.05f, 0.0f, 1000.0f);
        PropertyLabel("From surface");
        changed |= ImGui::Checkbox("##EmitFromSurface", &emitter.EmitFromSurface);

        changed |= RangeField("Lifetime", emitter.Lifetime, 0.01f);
        changed |= RangeField("Start speed", emitter.StartSpeed, 0.05f);
        changed |= RangeField("Start size", emitter.StartSize, 0.01f);
        changed |= RangeField("Start rotation", emitter.StartRotation, 1.0f);
        PropertyLabel("Start colour");
        changed |= ImGui::ColorEdit4("##StartColor", &emitter.StartColor[0], ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        PropertyLabel("Gravity");
        changed |= ImGui::DragFloat3("##Gravity", &emitter.Gravity[0], 0.05f);
        changed |= FloatField("Drag", emitter.Drag, 0.01f, 0.0f, 100.0f);

        PropertyLabel("Size over life");
        changed |= ParticleCurveEditor("##SizeOverLife", emitter.SizeOverLife);
        PropertyLabel("Speed over life");
        changed |= ParticleCurveEditor("##SpeedOverLife", emitter.SpeedOverLife);
        PropertyLabel("Colour over life");
        changed |= ParticleGradientEditor("##ColorOverLife", emitter.ColorOverLife);

        // The texture under assets://, taking a Project panel texture dropped on it.
        PropertyLabel("Texture");
        char texture[260] = {};
        emitter.Texture.copy(texture, sizeof(texture) - 1);
        if (ImGui::InputText("##Texture", texture, sizeof(texture)))
        {
            emitter.Texture = texture;
            changed         = true;
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const std::optional<ContentOpenRequest> dropped = AcceptAssetDrop({ ContentType::Texture }))
                if (const std::optional<std::string> path = ToMaterialTexturePath(dropped->VirtualPath))
                {
                    emitter.Texture = *path;
                    changed         = true;
                }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetItemTooltip("Drop a texture from the Project panel; empty for a soft round dot.");
        PropertyLabel("Sheet tiles");
        uint32_t tiles[2] = { emitter.SheetTilesX, emitter.SheetTilesY };
        if (ImGui::InputScalarN("##SheetTiles", ImGuiDataType_U32, tiles, 2))
        {
            emitter.SheetTilesX = tiles[0];
            emitter.SheetTilesY = tiles[1];
            changed             = true;
        }
        changed |= FloatField("Sheet FPS", emitter.SheetFps, 0.5f, 0.0f, 240.0f);
        PropertyLabel("Blend");
        changed |= EnumCombo("##Blend", emitter.Blend, BLENDS);
        PropertyLabel("Space");
        changed |= EnumCombo("##Space", emitter.SimulationSpace, SPACES);
        changed |= FloatField("Soft distance", emitter.SoftDistance, 0.01f, 0.0f, 100.0f);

        PropertyLabel("Collision");
        changed |= EnumCombo("##Collision", emitter.Collision.Mode, COLLISIONS);
        if (emitter.Collision.Mode != HedgehogEngine::ParticleCollisionMode::None)
        {
            if (emitter.Collision.Mode == HedgehogEngine::ParticleCollisionMode::HeightPlane)
                changed |= FloatField("Plane height", emitter.Collision.PlaneHeight, 0.05f);
            changed |= FloatField("Bounce", emitter.Collision.Bounce, 0.01f, 0.0f, 1.0f);
            changed |= FloatField("Lifetime loss", emitter.Collision.LifetimeLoss, 0.01f, 0.0f, 1.0f);
        }
        EndPropertyTable();

        // Bursts: when, how many, how many times (0 forever) and how far apart.
        ImGui::SeparatorText("Bursts");
        for (size_t b = 0; b < emitter.Bursts.size(); ++b)
        {
            HedgehogEngine::ParticleBurst& burst = emitter.Bursts[b];
            ImGui::PushID(static_cast<int>(b));
            ImGui::SetNextItemWidth(70.0f);
            changed |= ImGui::DragFloat("Time", &burst.Time, 0.05f, 0.0f, 3600.0f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            changed |= ImGui::InputScalar("Count", ImGuiDataType_U32, &burst.Count);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            changed |= ImGui::InputScalar("Cycles", ImGuiDataType_U32, &burst.Cycles);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            changed |= ImGui::DragFloat("Interval", &burst.Interval, 0.05f, 0.0f, 3600.0f);
            ImGui::SameLine();
            const bool remove = ImGui::SmallButton("Delete");
            ImGui::PopID();
            if (remove)
            {
                emitter.Bursts.erase(emitter.Bursts.begin() + static_cast<std::ptrdiff_t>(b));
                return true;
            }
        }
        if (ImGui::SmallButton("Add burst"))
        {
            emitter.Bursts.emplace_back();
            changed = true;
        }
        return changed;
    }

    bool ParticleEditorWindow::DrawSubEmitters(uint32_t index)
    {
        // Count particles of another emitter born where each of this one's particles dies.
        ImGui::SeparatorText("Sub-emitters");
        std::vector<HedgehogEngine::ParticleSubEmitter>& subs = m_Edited.Emitters[index].SubEmitters;
        for (size_t s = 0; s < subs.size(); ++s)
        {
            ImGui::PushID(static_cast<int>(s));
            const uint32_t child = subs[s].Emitter;
            ImGui::TextUnformatted(child < m_Edited.Emitters.size() ? m_Edited.Emitters[child].Name.c_str() : "(missing)");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            const bool counted = ImGui::InputScalar("per death", ImGuiDataType_U32, &subs[s].Count);
            ImGui::SameLine();
            const bool remove = ImGui::SmallButton("Delete");
            ImGui::PopID();
            if (remove)
            {
                subs.erase(subs.begin() + static_cast<std::ptrdiff_t>(s));
                return true;
            }
            if (counted)
                return true;
        }
        bool added = false;
        if (ImGui::BeginCombo("##AddSubEmitter", "Add sub-emitter"))
        {
            for (uint32_t child = 0; child < m_Edited.Emitters.size(); ++child)
            {
                // Not one that spawns this emitter already, which would come back to it.
                if (SpawnsEmitter(m_Edited, child, index))
                    continue;
                if (ImGui::Selectable(m_Edited.Emitters[child].Name.c_str()))
                    added = AddSubEmitter(m_Edited, index, child);
            }
            ImGui::EndCombo();
        }
        return added;
    }

    void ParticleEditorWindow::DrawProblems()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, PROBLEM_COLOR);
        for (const std::string& problem : m_Problems)
            ImGui::TextUnformatted(problem.c_str());
        ImGui::PopStyleColor();
    }
}
