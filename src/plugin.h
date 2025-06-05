#pragma once

#include "ISmmPlugin.h"
#include "version_gen.h"

PLUGIN_GLOBALVARS()

class MMSPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load( PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late );
	bool Unload( char *error, size_t maxlen ) { return true; }

public:
	const char *GetAuthor() { return PLUGIN_AUTHOR; }
	const char *GetName() { return PLUGIN_DISPLAY_NAME; }
	const char *GetDescription() { return PLUGIN_DESCRIPTION; }
	const char *GetURL() { return PLUGIN_URL; }
	const char *GetLicense() { return PLUGIN_LICENSE; }
	const char *GetVersion() { return PLUGIN_FULL_VERSION; }
	const char *GetDate() { return __DATE__; }
	const char *GetLogTag() { return PLUGIN_LOGTAG; }
};

extern MMSPlugin g_ThisPlugin;
