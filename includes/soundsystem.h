#include "includesol.h"
#include <soloud.h>
#include <soloud_wav.h>
#include <vector>

struct LuaBindContext;

class SoundSystem {
public:
	SoundSystem();
	~SoundSystem();
	void Init();
	void BindToLua(LuaBindContext& ctx);
private:
	SoLoud::Soloud m_soloud;
};