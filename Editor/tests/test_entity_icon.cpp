#include "doctest/doctest/doctest.h"

#include "Panels/EntityIcon.hpp"

using namespace Editor;

namespace
{
    // An entity that has everything, so each check below can take one trait away at a time.
    [[nodiscard]] EntityTraits Everything()
    {
        return { true, true, true, true, true, true, true, true };
    }
}

TEST_CASE("ChooseEntityIcon takes the first trait in its order of precedence")
{
    EntityTraits traits = Everything();
    CHECK(ChooseEntityIcon(traits) == EditorIcon::Camera);
    traits.HasCamera = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::Light);
    traits.HasLight = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::UiCanvas);
    traits.HasUiCanvas = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::UiRect);
    traits.HasUiElement = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::AudioSource);
    traits.HasAudioSource = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::AudioListener);
    traits.HasAudioListener = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::Mesh);
    traits.HasMesh = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::Folder);
    traits.HasChildren = false;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::GameObject);
}

TEST_CASE("Each trait alone gives its own icon")
{
    const auto only = [](bool EntityTraits::* trait)
    {
        EntityTraits traits;
        traits.*trait = true;
        return ChooseEntityIcon(traits);
    };
    CHECK(only(&EntityTraits::HasCamera) == EditorIcon::Camera);
    CHECK(only(&EntityTraits::HasLight) == EditorIcon::Light);
    CHECK(only(&EntityTraits::HasUiCanvas) == EditorIcon::UiCanvas);
    CHECK(only(&EntityTraits::HasUiElement) == EditorIcon::UiRect);
    CHECK(only(&EntityTraits::HasAudioSource) == EditorIcon::AudioSource);
    CHECK(only(&EntityTraits::HasAudioListener) == EditorIcon::AudioListener);
    CHECK(only(&EntityTraits::HasMesh) == EditorIcon::Mesh);
    CHECK(only(&EntityTraits::HasChildren) == EditorIcon::Folder);
}

TEST_CASE("A mesh with children keeps the mesh icon, and an empty entity is a game object")
{
    EntityTraits traits;
    traits.HasMesh     = true;
    traits.HasChildren = true;
    CHECK(ChooseEntityIcon(traits) == EditorIcon::Mesh);
    CHECK(ChooseEntityIcon(EntityTraits{}) == EditorIcon::GameObject);
}
