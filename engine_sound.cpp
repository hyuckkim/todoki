#include "engine_sound.h"

EngineSound& EngineSound::Instance() {
    static EngineSound instance;
    return instance;
}

bool EngineSound::Initialize() {
    m_soloud.init();
    return true;
}

void EngineSound::Shutdown() {
    m_soloud.deinit();
}

std::shared_ptr<SoLoud::Wav> EngineSound::LoadFromFile(const std::string& path) {
    auto wav = std::make_shared<SoLoud::Wav>();
    if (wav->load(path.c_str()) == SoLoud::SO_NO_ERROR) {
        return wav;
    }
    return nullptr;
}

std::shared_ptr<SoLoud::Wav> EngineSound::LoadFromMemory(const uint8_t* data, size_t size) {
    auto wav = std::make_shared<SoLoud::Wav>();
    // SoLoud는 메모리 로딩 시 데이터 복사 여부를 결정할 수 있음
    if (wav->loadMem(data, (unsigned int)size, true, true) == SoLoud::SO_NO_ERROR) {
        return wav;
    }
    return nullptr;
}

unsigned int EngineSound::Play(std::shared_ptr<SoLoud::Wav> sound, bool loop, float volume) {
    if (!sound) return 0;

    unsigned int handle = m_soloud.play(*sound);
    m_soloud.setLooping(handle, loop);
    m_soloud.setVolume(handle, volume);
    return handle;
}

void EngineSound::Stop(unsigned int handle) {
    m_soloud.stop(handle);
}

void EngineSound::SetVolume(unsigned int handle, float volume) {
    m_soloud.setVolume(handle, volume);
}