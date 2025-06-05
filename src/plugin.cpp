#include "plugin.h"
#include "schemareader.h"

MMSPlugin g_ThisPlugin;

PLUGIN_EXPOSE( MMSPlugin, g_ThisPlugin );
bool MMSPlugin::Load( PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late )
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_ANY( GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION );
	GET_V_IFACE_ANY( GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION );

	// Required to get the IMetamodListener events
	g_SMAPI->AddListener( this, this );

	META_CONVAR_REGISTER( FCVAR_RELEASE | FCVAR_GAMEDLL );

	return true;
}

CON_COMMAND( dump_schema, "Dumps schema to kv3/json file" )
{
	if(args.ArgC() > 1)
	{
		if(std::strcmp( args.Arg( 1 ), "help" ) == 0)
		{
			META_CONPRINTF( "Usage: dump_schema [flags]\n" );
			META_CONPRINTF( "Flags:\n" );

			for(int i = 0; i < ARRAYSIZE( SchemaReader::s_FlagsMap ); i++)
			{
				auto &flag = SchemaReader::s_FlagsMap[i];

				if(flag.m_Description)
					META_CONPRINTF( "\t- %s: %s\n", flag.m_Name, flag.m_Description );
				else
					META_CONPRINTF( "\t- %s\n", flag.m_Name );
			}

			return;
		}
	}

	SchemaReader sr;
	sr.ReadSchema( SchemaReader::ParseDumpFlags( args.ArgS() ) );
	
	sr.WriteToOutDir();
}
