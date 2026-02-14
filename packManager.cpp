#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <fstream>

struct PackEntry {
    uint32_t offset;
    uint32_t size;
};

class PackManager {
private:
    std::ifstream m_packFile;
    std::map<std::string, PackEntry> m_index;
    const uint8_t XOR_KEY = 0x42; // Python 빌더와 동일한 키

public:
    static PackManager& Instance() { static PackManager instance; return instance; }

    bool Init(const std::string& path, const std::string& virtualRoot = "assets/") {
        m_packFile.open(path, std::ios::binary);
        if (!m_packFile) return false;

        uint32_t fileCount = 0;
        m_packFile.read((char*)&fileCount, sizeof(fileCount));

        for (uint32_t i = 0; i < fileCount; ++i) {
            char name[260] = { 0 };
            uint32_t offset, size;
            m_packFile.read(name, 260);
            m_packFile.read((char*)&offset, sizeof(offset));
            m_packFile.read((char*)&size, sizeof(size));

            // [핵심] 파일명 앞에 가상 루트 폴더를 붙여서 인덱싱
            std::string finalPath = virtualRoot + name;
            m_index[finalPath] = { offset, size };
        }
        return true;
    }

    std::vector<uint8_t> GetFileData(const std::string& path) {
        auto it = m_index.find(path);
        if (it == m_index.end()) return {};

        std::vector<uint8_t> buffer(it->second.size);
        m_packFile.seekg(it->second.offset);
        m_packFile.read((char*)buffer.data(), it->second.size);

        // XOR 복호화
        for (auto& b : buffer) b ^= XOR_KEY;

        return buffer;
    }
};