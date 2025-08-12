#pragma once

#include "schemasystem/schemasystem.h"
#include "schemasystem/schematypes.h"

#include "keyvalues3.h"

#include <map>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>

#define DUMPER_FILE_FORMAT_VERSION 1

template <typename T>
constexpr const char *SchemaTypeToString() = delete;

template <> constexpr const char *SchemaTypeToString<CSchemaType_Builtin>()			{ return "builtin"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Ptr>()				{ return "ptr"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Atomic>()			{ return "atomic"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Atomic_T>()		{ return "atomic"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Atomic_CollectionOfT>() { return "atomic"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Atomic_TT>()		{ return "atomic"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_Atomic_I>()		{ return "atomic"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_DeclaredClass>()	{ return "class"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_DeclaredEnum>()	{ return "enum"; }
template <> constexpr const char *SchemaTypeToString<CSchemaType_FixedArray>()		{ return "array"; }
// template <> constexpr const char *SchemaTypeToString<CSchemaType_Bitfield>()		{ return "bitfield"; }

class SchemaReader
{
public:
	SchemaReader() : m_KV3Context( false ) {}

	void ReadSchema( uint32 flags = SR_NONE );

	// Outdir is relative to plugin folder
	void SetOutDir( const std::filesystem::path &out_dir );
	const std::filesystem::path &GetOutDir() const { return m_OutPath; }

	bool WriteToOutDir();
	bool WriteToKV3();
	bool WriteToJSON();

	static uint32 ParseDumpFlags( const char *flags );

	static bool IsVerboseLogging() { return (s_Flags & SR_VERBOSE_LOGGING) != 0; }
	static bool IsDumpingToJSON() { return (s_Flags & SR_DUMP_AS_JSON) != 0; }
	static bool IsDumpingToKV3() { return (s_Flags & SR_DUMP_AS_KV3) != 0; }
	static bool IsDumpingMetaTags() { return (s_Flags & SR_DUMP_METATAGS) != 0; }
	static bool IsDumpingAtomics() { return (s_Flags & SR_DUMP_ATOMICS) != 0; }
	static bool IsDumpingPulseBindings() { return (s_Flags & SR_DUMP_PULSE_BINDINGS) != 0; }
	static bool IsSplittingAtomicNames() { return (s_Flags & SR_SPLIT_ATOMIC_NAMES) != 0; }
	static bool IsIgnoringParentScopes() { return (s_Flags & SR_IGNORE_PARENT_SCOPE) != 0; }
	static bool IsApplyingNetVarOverrides() { return (s_Flags & SR_APPLY_NETVAR_OVERRIDES) != 0; }

private:
	static CSchemaSystem *SchemaSystem();

	void ValidateOutDir();

	void ReadBuiltins();
	void ReadDeclClasses();
	void ReadDeclEnums();
	void ReadAtomics();
	void ReadMemberSchemaType( KeyValues3 *root, CSchemaType *type, bool append_subtype = true );
	int ReadDeclClass( CSchemaType_DeclaredClass *type );
	int ReadDeclEnum( CSchemaType_DeclaredEnum *type );
	void ReadAtomicInfo( SchemaAtomicTypeInfo_t *info );
	void ReadMetaTags( KeyValues3 *root, SchemaMetadataEntryData_t *data, int count, bool append_traits = false );
	void ReadFlags( KeyValues3 *root, CSchemaType *type );
	void ReadPulseBindings();

	template <typename METATAG>
	void ReadPulseDomains( KeyValues3 *root, std::map<std::string, KeyValues3 *> &domains );
	template <typename DOMAIN_FUNCTION>
	void ReadPulseDomianFunctions( KeyValues3 *root, DOMAIN_FUNCTION *functions, int count );
	void ReadPulseDomainsInfo( KeyValues3 *root, std::map<std::string, KeyValues3 *> &domains );

	bool ApplyNetVarOverrides( CSchemaType_DeclaredClass *type );
	bool ApplyNetVarOverrides( CSchemaType_DeclaredClass *root, const char *field_to_overwrite, int type_override_idx );
	void LinkChildParentScopeDecls( KeyValues3 *child_traits, CSchemaType *child, int child_idx );

	bool WriteToFile( const std::string &filename, const char *content );

	CSchemaType_DeclaredClass *FindSchemaTypeInTypeScopes( const char *name );

	KeyValues3 *GetRoot() { return m_KV3Context.Root(); }
	KeyValues3 *GetDefs() { return GetRoot()->FindOrCreateMember( "defs" ); }
	KeyValues3 *GetAtomicDefs() { return GetRoot()->FindOrCreateMember( "atomics" ); }

	void RecordGameInfo();
	void RecordDumperInfo();
	void RecordDumpFlags();

	// Returns nullptr if entry already exists
	template <typename T>
	std::pair<KeyValues3 *, int> CreateDefEntry( T *type );
	
	KeyValues3 *FindDefEntry( int idx ) { return GetDefs()->GetArrayElement( idx ); }
	KeyValues3 *FindDefEntry( CSchemaType *type );

	int FindTypeMapEntry( CSchemaType *type ) const;

	std::string SplitTemplatedName( CSchemaType *type ) const;

private:
	CKeyValues3Context m_KV3Context;

	struct KeyLess
	{
		bool operator()( const CSchemaType *lhs, const CSchemaType *rhs ) const
		{
			return std::strcmp( lhs->m_sTypeName, rhs->m_sTypeName ) < 0;
		}
	};

	std::map<CSchemaType *, int, KeyLess> m_TypeMap;
	std::filesystem::path m_OutPath;

	inline static uint32 s_Flags = 0;

public:
	enum 
	{
		SR_NONE = 0,
		SR_VERBOSE_LOGGING	= (1 << 0),

		SR_DUMP_AS_JSON		= (1 << 1),
		SR_DUMP_AS_KV3		= (1 << 2),

		// Dump metatags
		SR_DUMP_METATAGS	= (1 << 3),

		// Dump atomic info
		SR_DUMP_ATOMICS		= (1 << 4),

		// Dump pulse bindings
		SR_DUMP_PULSE_BINDINGS = (1 << 5),

		// Splits templated atomic names and leaves only base name leaving templated stuff
		SR_SPLIT_ATOMIC_NAMES = (1 << 6),

		// Ignores parent scope decls and removes inlined structs/classes
		// converting them from A::B to A__B
		SR_IGNORE_PARENT_SCOPE = (1 << 7),

		// Applies netvar overrides to types (MNetworkVarTypeOverride metatags)
		SR_APPLY_NETVAR_OVERRIDES = (1 << 8)
	};

	struct DumpFlags_t
	{
		uint32 m_Flag;
		const char *m_Name;
		const char *m_OutName;
		const char *m_Description;
	};

	static inline DumpFlags_t s_FlagsMap[] = {
		{ SR_VERBOSE_LOGGING, "verbose", nullptr, "Verbose output" },
		{ SR_DUMP_AS_JSON, "as_json", nullptr, "Dumps to a json file (Default)" },
		{ SR_DUMP_AS_KV3, "as_kv3", nullptr, "Dumps to a kv3 file" },
		{ SR_DUMP_METATAGS, "metatags", "has_metatags", "Dump metatags" },
		{ SR_DUMP_ATOMICS, "atomics", "has_atomics", "Dump atomics" },
		{ SR_DUMP_PULSE_BINDINGS, "pulse_bindings", "has_pulse_bindings", "Dump pulse bindings" },
		{ SR_SPLIT_ATOMIC_NAMES, "split_atomics", "atomic_names_split", "Splits templated atomic names and leaves only base name leaving templated stuff" },
		{ SR_IGNORE_PARENT_SCOPE, "ignore_parents", "no_parent_scope", "Ignores parent scope decls and removes inlined structs/classes converting them from A::B to A__B" },
		{ SR_APPLY_NETVAR_OVERRIDES, "apply_netvar_overrides", "netvars_overriden", "Applies netvar overrides to types (MNetworkVarTypeOverride metatags)" },

		// Supplementary definitions
		{ SR_DUMP_METATAGS | SR_DUMP_ATOMICS | SR_DUMP_PULSE_BINDINGS, "all", nullptr, "Dumps everything" },
		{ SR_SPLIT_ATOMIC_NAMES | SR_IGNORE_PARENT_SCOPE | SR_APPLY_NETVAR_OVERRIDES, "for_cpp", nullptr, "Use optimal flags for cpp generation later" },
	};
};

// Short convenience helper method to replace all occurrences in a string with other
inline std::string ReplaceString( std::string subject, const std::string &search, const std::string &replace )
{
	size_t pos = 0;
	while((pos = subject.find( search, pos )) != std::string::npos)
	{
		subject.replace( pos, search.length(), replace );
		pos += replace.length();
	}
	return subject;
}

template <typename T>
inline std::pair<KeyValues3 *, int> SchemaReader::CreateDefEntry( T *type )
{
	int map_type_idx = FindTypeMapEntry( type );
	if(map_type_idx != -1)
		return std::make_pair( nullptr, map_type_idx );

	map_type_idx = GetDefs()->GetArrayElementCount();
	m_TypeMap[type] = map_type_idx;
	auto def = GetDefs()->ArrayAddElementToTail();

	def->SetMemberString( "type", SchemaTypeToString<T>() );

	if(IsIgnoringParentScopes())
		def->SetMemberString( "name", ReplaceString( type->m_sTypeName.Get(), "::", "__" ).c_str() );
	else
		def->SetMemberString( "name", type->m_sTypeName.Get() );

	def->SetMemberString( "scope", type->m_pTypeScope->GetScopeName() );

	if(auto decl_class = type->template ReinterpretAs<CSchemaType_DeclaredClass>())
		def->SetMemberString( "project", decl_class->m_pClassInfo->m_pszProjectName );

	int size;
	uint8 alignment;
	type->GetSizeAndAlignment( size, alignment );

	def->SetMemberInt( "size", size );
	def->SetMemberInt( "alignment", alignment );

	return std::make_pair( def, map_type_idx );
}
