#pragma once
#include "soloud.h"
#include "soloud_wav.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class EngineSound {
public:
    static EngineSound& Instance();

    bool Initialize();
    void Shutdown();

    // 데이터 로드: SoLoud::Wav 객체를 생성하여 반환
    std::shared_ptr<SoLoud::Wav> LoadFromFile(const std::string& path);
    std::shared_ptr<SoLoud::Wav> LoadFromMemory(const uint8_t* data, size_t size);

    // 재생: SoLoud는 재생 시 handle(unsigned int)을 반환함
    unsigned int Play(std::shared_ptr<SoLoud::Wav> sound, bool loop = false, float volume = 1.0f);
    void Stop(unsigned int handle);
    void SetVolume(unsigned int handle, float volume);

private:
    EngineSound() = default;
    SoLoud::Soloud m_soloud; // 실제 엔진 객체
};

extern std::vector<std::shared_ptr<SoLoud::Wav>> g_soundTable;