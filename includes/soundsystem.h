#include "includesol.h"
#include "luabind.h"
#include <soloud.h>
#include <soloud_wav.h>
#include <vector>

class SoundSystem {
public:
	SoundSystem();
	~SoundSystem();
	void Init();
	void BindToLua(LuaBindContext& ctx);
private:
	SoLoud::Soloud m_soloud;
};