#include "soundsystem.h"
#include "resourcehub.h"
#include "luabind.h"

SoundSystem::SoundSystem() {
}
SoundSystem::~SoundSystem() {
    m_soloud.deinit();
}

void SoundSystem::Init() {
    m_soloud.init();
}

void SoundSystem::BindToLua(LuaBindContext& ctx) {
    LuaNamespaceBinder(ctx)
        .func("play", [this](int soundId, sol::optional<float> volume, sol::optional<float> pan) {
            SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
            if (!wav) return (unsigned int)0;

            int handle = m_soloud.play(*wav);
            if (volume) m_soloud.setVolume(handle, volume.value());
            if (pan) m_soloud.setPan(handle, pan.value());
            return (unsigned int)handle;
        })
        .names({"soundId", "volume", "pan"})
        .returns("integer")
        .desc("Play a sound once and return its handle")

        .func("loop", [this](int soundId, sol::optional<float> volume) {
            SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
            if (!wav) return (unsigned int)0;

            int handle = m_soloud.play(*wav);
            m_soloud.setLooping(handle, true);
            if (volume) m_soloud.setVolume(handle, volume.value());
            return (unsigned int)handle;
        })
        .names({"soundId", "volume"})
        .returns("integer")
        .desc("Play a sound in loop and return its handle")

        .func("stop", [this](sol::optional<unsigned int> handle) {
            if (handle) m_soloud.stop(handle.value());
            else m_soloud.stopAll();
        })
        .names({"handle"})
        .desc("Stop a sound by handle, or stop all sounds if no handle given")

        .func("volume", [this](unsigned int handle, float vol) {
            m_soloud.setVolume(handle, vol);
        })
        .names({"handle", "vol"})
        .desc("Set volume for a playing sound (0.0 - 1.0)")

        .func("pause", [this](unsigned int handle, bool paused) {
            m_soloud.setPause(handle, paused);
        })
        .names({"handle", "paused"})
        .desc("Pause or resume a playing sound")

        .func("masterVolume", [this](float vol) {
            m_soloud.setGlobalVolume(vol);
        })
        .names({"vol"})
        .desc("Set global master volume (0.0 - 1.0)");
}