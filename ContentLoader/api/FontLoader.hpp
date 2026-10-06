#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedFont.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <optional>
#include <string>

namespace ContentLoader
{
    // Bakes a TrueType font (under assets:// unless fileName names a mount) at pixelHeight (ascent to descent): every printable
    // ASCII and Latin-1 codepoint the font has (U+0020 to U+007E and U+00A0 to U+00FF), plus the
    // fallback glyph, packed without overlap into one R8 atlas with a pixel of padding, and the
    // kerning between every two of them. Returns std::nullopt (after logging) when the file cannot
    // be read or is not a font, or pixelHeight is not positive.
    CONTENT_LOADER_API std::optional<LoadedFont> LoadFont(const std::string& fileName, float pixelHeight,
                                                          const FS::FileSystemManager& fileSystem);
}
