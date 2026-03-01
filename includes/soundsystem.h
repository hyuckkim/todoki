#include "includesol.h"
#include <soloud.h>
#include <soloud_wav.h>
#include <vector>

class SoundSystem {
public:
	SoundSystem();
	~SoundSystem();
	void Init();
	void BindToLua(sol::state& lua, const char* name);
private:
	SoLoud::Soloud m_soloud;
};