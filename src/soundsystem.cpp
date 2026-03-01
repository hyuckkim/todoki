#include "soundsystem.h"
#include "resourcehub.h"
#include "includesol.h"

SoundSystem::SoundSystem() {
}
SoundSystem::~SoundSystem() {
    m_soloud.deinit();
}

void SoundSystem::Init() {
    m_soloud.init();
}

void SoundSystem::BindToLua(sol::state& lua, const char* name) {
    sol::table s = lua.create_named_table(name);

    // 재생 (res.sound로 로드한 ID 사용)
    s["play"] = [this](int soundId, sol::optional<float> volume, sol::optional<float> pan) {
        SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
        if (!wav) return (unsigned int)0;

        int handle = m_soloud.play(*wav);
        if (volume) m_soloud.setVolume(handle, volume.value());
        if (pan) m_soloud.setPan(handle, pan.value());
        return (unsigned int)handle;
        };

    // 루프 재생
    s["loop"] = [this](int soundId, sol::optional<float> volume) {
        SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
        if (!wav) return (unsigned int)0;

        int handle = m_soloud.play(*wav);
        m_soloud.setLooping(handle, true);
        if (volume) m_soloud.setVolume(handle, volume.value());
        return (unsigned int)handle;
        };

    // 정지/전체 정지
    s["stop"] = [this](sol::optional<unsigned int> handle) {
        if (handle) m_soloud.stop(handle.value());
        else m_soloud.stopAll();
        };

    // 볼륨 조절 (0.0 ~ 1.0)
    s["volume"] = [this](unsigned int handle, float vol) {
        m_soloud.setVolume(handle, vol);
        };

    // 일시정지
    s["pause"] = [this](unsigned int handle, bool paused) {
        m_soloud.setPause(handle, paused);
        };

    // 마스터 볼륨
    s["masterVolume"] = [this](float vol) {
        m_soloud.setGlobalVolume(vol);
        };
}