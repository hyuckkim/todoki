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

void SoundSystem::BindToLua(LuaBindContext& ctx) {
    LuaNamespaceBinder binder(ctx);

    // 재생 (res.sound로 로드한 ID 사용)
    binder.func("play", [this](int soundId, std::optional<float> volume, std::optional<float> pan) {
        SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
        if (!wav) return (unsigned int)0;

        int handle = m_soloud.play(*wav);
        if (volume) m_soloud.setVolume(handle, volume.value());
        if (pan) m_soloud.setPan(handle, pan.value());
        return (unsigned int)handle;
    }).names({"soundId", "volume", "pan"});

    // 루프 재생
    binder.func("loop", [this](int soundId, std::optional<float> volume) {
        SoLoud::Wav* wav = ResourceHub::Instance().GetSound(soundId);
        if (!wav) return (unsigned int)0;

        int handle = m_soloud.play(*wav);
        m_soloud.setLooping(handle, true);
        if (volume) m_soloud.setVolume(handle, volume.value());
        return (unsigned int)handle;
    }).names({"soundId", "volume"});

    // 정지/전체 정지
    binder.func("stop", [this](std::optional<unsigned int> handle) {
        if (handle) m_soloud.stop(handle.value());
        else m_soloud.stopAll();
    }).names({"handle"});

    // 볼륨 조절 (0.0 ~ 1.0)
    binder.func("volume", [this](unsigned int handle, float vol) {
        m_soloud.setVolume(handle, vol);
    }).names({"handle", "vol"});

    // 일시정지
    binder.func("pause", [this](unsigned int handle, bool paused) {
        m_soloud.setPause(handle, paused);
    }).names({"handle", "paused"});

    // 마스터 볼륨
    binder.func("masterVolume", [this](float vol) {
        m_soloud.setGlobalVolume(vol);
    }).names({"vol"});
}