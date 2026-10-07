#include "doctest/doctest/doctest.h"

#include "Panels/EditorIcons.hpp"

#include <string>

using namespace Editor;

TEST_CASE("FindEditorIcon maps every icon's file name, without .png, back to the icon")
{
    for (size_t index = 0; index < EDITOR_ICON_COUNT; ++index)
    {
        const EditorIcon  icon = static_cast<EditorIcon>(index);
        const std::string file = GetEditorIconFile(icon);
        REQUIRE(file.ends_with(".png"));
        CHECK(FindEditorIcon(file.substr(0, file.size() - 4)) == icon);
    }
}

TEST_CASE("FindEditorIcon knows the icon names the engine's component types use")
{
    CHECK(FindEditorIcon("transform") == EditorIcon::Transform);
    CHECK(FindEditorIcon("mesh") == EditorIcon::Mesh);
    CHECK(FindEditorIcon("material") == EditorIcon::Material);
    CHECK(FindEditorIcon("light") == EditorIcon::Light);
    CHECK(FindEditorIcon("camera") == EditorIcon::Camera);
    CHECK(FindEditorIcon("animator") == EditorIcon::Animator);
    CHECK(FindEditorIcon("script") == EditorIcon::Script);
    CHECK(FindEditorIcon("ui_canvas") == EditorIcon::UiCanvas);
    CHECK(FindEditorIcon("ui_rect") == EditorIcon::UiRect);
    CHECK(FindEditorIcon("ui_image") == EditorIcon::UiImage);
    CHECK(FindEditorIcon("ui_text") == EditorIcon::UiText);
    CHECK(FindEditorIcon("ui_button") == EditorIcon::UiButton);
    CHECK(FindEditorIcon("audio_source") == EditorIcon::AudioSource);
    CHECK(FindEditorIcon("audio_listener") == EditorIcon::AudioListener);
    CHECK(FindEditorIcon("rigid_body") == EditorIcon::RigidBody);
    CHECK(FindEditorIcon("collider") == EditorIcon::Collider);
}

TEST_CASE("FindEditorIcon gives no icon for an empty, unknown or misspelt name")
{
    CHECK_FALSE(FindEditorIcon("").has_value());
    CHECK_FALSE(FindEditorIcon("rigidbody").has_value());
    CHECK_FALSE(FindEditorIcon("mesh.png").has_value());
    CHECK_FALSE(FindEditorIcon("Mesh").has_value());
    CHECK_FALSE(FindEditorIcon("mes").has_value());
}
