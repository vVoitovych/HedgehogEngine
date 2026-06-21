// Implementation is inline in GuiReflection.hpp to avoid cross-DLL ImGui context issues.
// HedgehogEngine.dll has a separate GImGui instance that is never initialized;
// making these functions inline ensures they compile into the caller's binary
// (Editor.exe), which owns the live ImGui context.
