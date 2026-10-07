#pragma once

namespace HP
{
    // Jolt's process-wide setup, counted: the first Acquire registers Jolt's default allocator, its
    // trace and assert hooks (to the Logger), its factory and its types; the last Release undoes
    // them. Thread-safe. Every PhysicsWorld holds one reference while it runs.
    void AcquireJoltRuntime();
    void ReleaseJoltRuntime();
}
