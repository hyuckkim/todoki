#include <soloud.h>
#include <soloud_wav.h>
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    SoLoud::Soloud soloud;

    if (soloud.init(SoLoud::Soloud::WASAPI) != SoLoud::SO_NO_ERROR)
    {
        std::cout << "Failed to init SoLoud\n";
        return 1;
    }

    SoLoud::Wav wav;
    if (wav.load("test.wav") != SoLoud::SO_NO_ERROR)
    {
        std::cout << "Failed to load wav\n";
        return 1;
    }

    soloud.play(wav);

    std::cout << "Playing...\n";

    while (soloud.getActiveVoiceCount() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    soloud.deinit();
    return 0;
}