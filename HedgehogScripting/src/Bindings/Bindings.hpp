#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    class EngineContext;
    struct FixedStepClock;
}

// The engine types and functions the ScriptSystem gives its scripts, as globals.
namespace HedgehogScripting::Bindings
{
    struct ScriptEntity;

    // Vector3 (HM::Vector3) and Quat (HM::Quaternion) usertypes.
    void RegisterMath(sol::state& lua);

    // Log.info / Log.warn / Log.error, and print routed to Log.info, all through Logger with the
    // calling script's file and line as prefix.
    void RegisterLog(sol::state& lua);

    // Entity (ScriptEntity) and Transform (ScriptComponentRef<TransformComponent>) usertypes over
    // context's ECS; Transform writes publish TransformChangedEvent on its EventBus. Entity also
    // gets getX/hasX/addX for the Light, Camera, Mesh, Animator, AudioSource, UiRect, UiImage, UiText
    // and UiButton components. context must outlive lua.
    void RegisterEntity(sol::state& lua, HedgehogEngine::EngineContext& context);

    // Light, Camera and Mesh (ScriptComponentRef<T>) usertypes and the LightType,
    // CameraProjectionType and CameraTargetMode enum tables. Writes go straight to the ECS;
    // castShadows goes through LightSystem::SetShadowCasting and mesh.path through
    // MeshSystem::LoadMesh, as in the inspector. Animator (play, stop, isPlaying, getClipNames,
    // clip, speed, loop, time) goes through AnimationSystem. context must outlive lua.
    void RegisterComponents(sol::state& lua, HedgehogEngine::EngineContext& context);

    // The AudioSource usertype (ScriptComponentRef<AudioSourceComponent>: play, stop, isPlaying,
    // clip, volume, pitch, loop, spatial, playOnStart, minDistance, maxDistance) through the
    // engine's AudioSystem, and the Audio table (playOneShot, setMasterVolume, getMasterVolume)
    // over its AudioEngine. Clips load only from under assets:// through the context's file
    // system: another mount, a drive or a ".." is a script error. Outside Play mode play, stop,
    // playOneShot and setMasterVolume do nothing, and the first such call logs a warning.
    // context must outlive lua.
    void RegisterAudio(sol::state& lua, HedgehogEngine::EngineContext& context);

    // The Scene table (find, findAll, spawn, destroy) and entity:destroy(), over context's
    // SceneManager. Destroying only queues the entity in pendingDestroys; FlushDestroys deletes
    // them. Call after RegisterEntity. context and pendingDestroys must outlive lua.
    void RegisterScene(sol::state& lua, HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys);

    // Deletes the queued entities still alive through SceneManager::DeleteGameObject, which moves
    // their children up to the grandparent, and empties the queue. A scripted one gets OnDestroy
    // through the ScriptComponent removal callback.
    void FlushDestroys(HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys);

    // The Time table: deltaTime (the hook's dt: OnUpdate's scaled dt, or the fixed dt inside
    // OnFixedUpdate), fixedDeltaTime, time (the clock's scaled simulated time), frame (OnUpdate
    // calls since Play) and timeScale, the only writable field, clamped to [0, 100] and stored in
    // the clock. Every read sees the current values. All three must outlive lua.
    void RegisterTime(sol::state& lua, HedgehogEngine::FixedStepClock& clock, const float& deltaTime,
                      const uint64_t& frame);

    // An asset path as scripts write it (under assets://, prefix optional, either slash), as the path
    // under assets:// components store. Anything that could reach past assets:// is a script error
    // naming the asset (singular, e.g. "an audio clip"; plural, "audio clips"): another mount, a
    // drive, a rooted path or a ".." segment. An empty path is an error too.
    [[nodiscard]] std::string ToAssetPath(std::string path, const char* singular, const char* plural);

    // UiRect, UiImage, UiText and UiButton (ScriptComponentRef<T>) usertypes (RegisterEntity gives
    // Entity their getX/hasX/addX). Writes go straight to the ECS, so the next
    // extraction draws them. button:onClick(fn) subscribes fn to that button's clicks through
    // subscribeClick and returns the subscription's id (Events.unsubscribe takes it). context must
    // outlive lua.
    using ClickSubscriber = std::function<uint64_t(const ScriptEntity& button, sol::protected_function handler)>;
    void RegisterUi(sol::state& lua, HedgehogEngine::EngineContext& context, ClickSubscriber subscribeClick);

    // The Save table over context's SaveGameManager: write(slot) and load(slot) queue a save of
    // the world (and every registered section, the scripts' state included) or a load, done at the
    // end of the frame, and return whether it was queued (load: whether the slot exists);
    // list() returns { name, scene, timestamp, playTime, gameDataVersion, saveVersion } per slot,
    // by name; exists(slot) and delete(slot). A slot name other than 1 to 64 of A-Z, a-z, 0-9, '_'
    // and '-' is a script error. Outside Play mode write, load and delete do nothing and return
    // false, and the first such call logs a warning. context must outlive lua.
    void RegisterSave(sol::state& lua, HedgehogEngine::EngineContext& context);

    // The Input table over context's game actions (assets://Input/actions.yaml's Game map):
    // isDown, wasPressed, wasReleased, value and consume take an action's name (an unknown one, or
    // a non-string, is a script error); pointerPosition and pointerDelta return x and y in game-view
    // pixels, y down; isPointerInside. Consumed actions read as up; nothing is down outside Play.
    // Every call reads the current state. context must outlive lua.
    void RegisterInput(sol::state& lua, HedgehogEngine::EngineContext& context);
}
