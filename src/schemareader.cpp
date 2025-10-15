#include "schemareader.h"
#include "schema_metadata.h"
#include "pulse_metadata.h"

#include "plugin.h"

#include <fstream>
#include <filesystem>
#include <ctime>

CSchemaSystem *SchemaReader::SchemaSystem()
{
	static CSchemaSystem *s_SchemaSystem = nullptr;

	if(!s_SchemaSystem)
	{
		if(!g_pSchemaSystem || !g_pSchemaSystem->SchemaSystemIsReady())
		{
			META_CONPRINTF( "Early schema system usage!\n" );
			return nullptr;
		}

		s_SchemaSystem = (CSchemaSystem *)g_pSchemaSystem;
	}

	return s_SchemaSystem;
}

KeyValues3 *SchemaReader::FindDefEntry( CSchemaType *type )
{
	int idx = FindTypeMapEntry( type );
	if(idx == -1)
		return nullptr;

	return FindDefEntry( idx );
}

int SchemaReader::FindTypeMapEntry( CSchemaType *type ) const
{
	auto iter = m_TypeMap.find( type );
	if(iter == m_TypeMap.end())
		return -1;

	return iter->second;
}

std::string SchemaReader::SplitTemplatedName( CSchemaType *type ) const
{
	std::string name = type->m_sTypeName.Get();

	if(IsSplittingAtomicNames())
	{
		auto pos = name.find( '<' );
		if(pos != std::string::npos)
			name.resize( pos );
	}

	return name;
}

void SchemaReader::RecordGameInfo()
{
	std::ifstream inp( std::filesystem::path( g_SMAPI->GetBaseDir() ) / "steam.inf" );

	if(inp.is_open())
	{
		auto game_info = GetRoot()->FindOrCreateMember( "game_info" );

		std::string key, value;
		do
		{
			if(!std::getline( inp, key, '=' ))
				break;
			if(!std::getline( inp, value, '\n' ))
				break;

			game_info->SetMemberString( key.c_str(), value.c_str() );
		} while(inp.good());
	}
	else
	{
		META_CONPRINTF( "Failed to open steam.inf, no game_info would be available in the dump file.\n" );
	}
}

void SchemaReader::RecordDumperInfo()
{
	auto dumper_info = GetRoot()->FindOrCreateMember( "dumper_info" );

	dumper_info->SetMemberString( "version", g_ThisPlugin.GetVersion() );

	{
		std::time_t time = std::time( nullptr );
		std::tm *gtm = std::gmtime( &time );

		char buf[128];

		// Formatted for ISO 8601 UTC time
		std::strftime( buf, sizeof( buf ), "%Y-%m-%dT%H:%M:%SZ", gtm );
		dumper_info->SetMemberString( "dump_date", buf );
	}

	dumper_info->SetMemberInt( "dump_format_version", DUMPER_FILE_FORMAT_VERSION );
}

void SchemaReader::RecordDumpFlags()
{
	auto dump_flags = GetRoot()->FindOrCreateMember( "dump_flags" );
	dump_flags->SetToEmptyArray();

	for(int i = 0; i < ARRAYSIZE( s_FlagsMap ); i++)
	{
		if(s_FlagsMap[i].m_OutName && (s_Flags & s_FlagsMap[i].m_Flag) != 0)
		{
			dump_flags->ArrayAddElementToTail()->SetString( s_FlagsMap[i].m_OutName );
		}
	}
}

uint32 SchemaReader::ParseDumpFlags( const char *flags )
{
	uint32 result = 0;
	const char *split_chars[] = { " ", ",", ";" };
	CSplitString split( flags, split_chars, ARRAYSIZE( split_chars ), false );

	for(int i = split.Count() - 1; i >= 0; i--)
	{
		for(int k = 0; k < ARRAYSIZE( s_FlagsMap ); k++)
		{
			if(s_FlagsMap[k].m_Name && std::strcmp( split[i], s_FlagsMap[k].m_Name ) == 0)
			{
				split.Remove( i );
				result |= s_FlagsMap[k].m_Flag;
				break;
			}
		}
	}

	for(int i = 0; i < split.Count(); i++)
	{
		META_CONPRINTF( "Failed to parse flag (%s), ignoring...\n", split[i] );
	}

	// Fallback to using json as default file format if none was provided
	if((result & SR_DUMP_AS_JSON) == 0 && (result & SR_DUMP_AS_KV3) == 0)
		result |= SR_DUMP_AS_JSON;
	
	return result;
}

void SchemaReader::ReadSchema( uint32 flags )
{
	META_CONPRINTF( "Reading schema...\n" );

	RecordGameInfo();
	RecordDumperInfo();

	s_Flags = flags;
	RecordDumpFlags();

	ReadBuiltins();
	ReadDeclClasses();
	ReadDeclEnums();
	ReadAtomics();
	ReadPulseBindings();
}

void SchemaReader::SetOutDir( const std::filesystem::path &out_dir )
{
	m_OutPath = std::filesystem::path( g_SMAPI->GetBaseDir() ) / "addons" / PLUGIN_NAME / out_dir;
	std::filesystem::create_directories( m_OutPath.parent_path() );
}

void SchemaReader::ValidateOutDir()
{
	// Fallback to using dumps subdir in case none was provided
	if(m_OutPath.empty())
		SetOutDir( "dumps/" );
}

void SchemaReader::ReadBuiltins()
{
	META_CONPRINTF( "Reading builtins...\n" );

	auto gts = SchemaSystem()->GlobalTypeScope();

	for(int i = SCHEMA_BUILTIN_TYPE_VOID; i < SCHEMA_BUILTIN_TYPE_COUNT; i++)
	{
		CreateDefEntry( &gts->m_BuiltinTypes[i] );
	}
}

void SchemaReader::ReadDeclClasses()
{
	META_CONPRINTF( "Reading classes...\n" );

	auto gts = SchemaSystem()->GlobalTypeScope();

	FOR_EACH_MAP( gts->m_DeclaredClasses.m_Map, iter )
	{
		ReadDeclClass( gts->m_DeclaredClasses.m_Map.Element( iter ) );
	}

	for(int i = 0; i < SchemaSystem()->m_TypeScopes.GetNumStrings(); i++)
	{
		auto ts = SchemaSystem()->m_TypeScopes[i];

		FOR_EACH_MAP( ts->m_DeclaredClasses.m_Map, iter )
		{
			ReadDeclClass( ts->m_DeclaredClasses.m_Map.Element( iter ) );
		}
	}
}

void SchemaReader::ReadDeclEnums()
{
	META_CONPRINTF( "Reading enums...\n" );

	auto gts = SchemaSystem()->GlobalTypeScope();

	FOR_EACH_MAP( gts->m_DeclaredEnums.m_Map, iter )
	{
		ReadDeclEnum( gts->m_DeclaredEnums.m_Map.Element( iter ) );
	}

	for(int i = 0; i < SchemaSystem()->m_TypeScopes.GetNumStrings(); i++)
	{
		auto ts = SchemaSystem()->m_TypeScopes[i];

		FOR_EACH_MAP( ts->m_DeclaredEnums.m_Map, iter )
		{
			ReadDeclEnum( ts->m_DeclaredEnums.m_Map.Element( iter ) );
		}
	}
}

void SchemaReader::ReadAtomics()
{
	if(!IsDumpingAtomics())
		return;

	META_CONPRINTF( "Reading atomics...\n" );

	auto gts = SchemaSystem()->GlobalTypeScope();

	FOR_EACH_MAP( gts->m_AtomicInfos.m_Map, iter )
	{
		ReadAtomicInfo( gts->m_AtomicInfos.m_Map.Element( iter ).Get() );
	}

	for(int i = 0; i < SchemaSystem()->m_TypeScopes.GetNumStrings(); i++)
	{
		auto ts = SchemaSystem()->m_TypeScopes[i];

		FOR_EACH_MAP( ts->m_AtomicInfos.m_Map, iter )
		{
			ReadAtomicInfo( ts->m_AtomicInfos.m_Map.Element( iter ).Get() );
		}
	}
}

void SchemaReader::ReadMemberSchemaType( KeyValues3 *root, CSchemaType *type, bool append_subtype )
{
	if(append_subtype)
		root = root->FindOrCreateMember( "subtype" );

	switch(type->m_eTypeCategory)
	{
		case SCHEMA_TYPE_BUILTIN:
		case SCHEMA_TYPE_DECLARED_CLASS:
		case SCHEMA_TYPE_DECLARED_ENUM:
		{
			root->SetMemberString( "type", "ref" );

			if(type->IsA<CSchemaType_Builtin>())
				root->SetMemberInt( "ref_idx", FindTypeMapEntry( type ) );
			else if(type->IsA<CSchemaType_DeclaredClass>())
				root->SetMemberInt( "ref_idx", ReadDeclClass( type->ReinterpretAs<CSchemaType_DeclaredClass>() ) );
			else
				root->SetMemberInt( "ref_idx", ReadDeclEnum( type->ReinterpretAs<CSchemaType_DeclaredEnum>() ) );

			break;
		}
		case SCHEMA_TYPE_POINTER:
		{
			auto ptr = type->ReinterpretAs<CSchemaType_Ptr>();

			root->SetMemberString( "type", "ptr" );
			ReadMemberSchemaType( root, ptr->GetInnerType().Get() );

			break;
		}

		case SCHEMA_TYPE_ATOMIC:
		{
			root->SetMemberString( "type", "atomic" );
			root->SetMemberString( "name", SplitTemplatedName( type ).c_str() );

			int size;
			uint8 alignment;
			type->GetSizeAndAlignment( size, alignment );

			root->SetMemberInt( "size", size );
			root->SetMemberUInt8( "alignment", alignment );

			switch(type->m_eAtomicCategory)
			{
				case SCHEMA_ATOMIC_T:
				{
					auto atomic = type->ReinterpretAs<CSchemaType_Atomic_T>();
					auto templ = root->FindOrCreateMember( "template" );

					ReadMemberSchemaType( templ->ArrayAddElementToTail(), atomic->m_pTemplateType, false );

					break;
				}

				case SCHEMA_ATOMIC_TT:
				{
					auto atomic = type->ReinterpretAs<CSchemaType_Atomic_TT>();
					auto templ = root->FindOrCreateMember( "template" );

					ReadMemberSchemaType( templ->ArrayAddElementToTail(), atomic->m_pTemplateType, false );
					ReadMemberSchemaType( templ->ArrayAddElementToTail(), atomic->m_pTemplateType2, false );

					break;
				}

				case SCHEMA_ATOMIC_COLLECTION_OF_T:
				{
					auto atomic = type->ReinterpretAs<CSchemaType_Atomic_CollectionOfT>();
					auto templ = root->FindOrCreateMember( "template" );

					ReadMemberSchemaType( templ->ArrayAddElementToTail(), atomic->m_pTemplateType, false );
					if(atomic->m_nFixedBufferCount > 0)
					{
						auto ii = templ->ArrayAddElementToTail();

						ii->SetMemberString( "type", "literal" );
						ii->SetMemberInt64( "value", atomic->m_nFixedBufferCount );
					}

					break;
				}

				case SCHEMA_ATOMIC_I:
				{
					auto atomic = type->ReinterpretAs<CSchemaType_Atomic_I>();
					auto templ = root->FindOrCreateMember( "template" );

					auto ii = templ->ArrayAddElementToTail();
					ii->SetMemberString( "type", "literal" );
					ii->SetMemberInt( "value", atomic->m_nInteger );

					break;
				}
			}

			break;
		}

		case SCHEMA_TYPE_BITFIELD:
		{
			auto bitfield = type->ReinterpretAs<CSchemaType_Bitfield>();

			root->SetMemberString( "type", "bitfield" );
			root->SetMemberInt64( "count", bitfield->m_nBitfieldCount );

			break;
		}

		case SCHEMA_TYPE_FIXED_ARRAY:
		{
			auto fixed_array = type->ReinterpretAs<CSchemaType_FixedArray>();

			root->SetMemberString( "type", "fixed_array" );
			root->SetMemberInt64( "element_size", fixed_array->m_nElementSize );
			root->SetMemberInt64( "count", fixed_array->m_nElementCount );
			ReadMemberSchemaType( root, fixed_array->GetInnerType().Get() );

			break;
		}
	}
}

int SchemaReader::ReadDeclClass( CSchemaType_DeclaredClass *type )
{
	auto [def, idx] = CreateDefEntry( type );

	if(!def)
		return idx;
	
	auto traits = def->FindOrCreateMember( "traits" );
	auto ci = type->m_pClassInfo;

	LinkChildParentScopeDecls( traits, type, idx );
	ReadFlags( traits, type );

	if(ci)
	{
		// Ugly hack to prevent stringifying corrupted kv3 getter in that class,
		// otherwise it'll crash when attempted to be retrieved
		if(std::strcmp( ci->m_pszName, "CastSphereSATParams_t" ) != 0)
			ReadMetaTags( traits, ci->m_pStaticMetadata, ci->m_nStaticMetadataCount );

		if(ci->m_nBaseClassCount > 0)
		{
			traits->SetMemberUShort( "multi_depth", ci->m_nMultipleInheritanceDepth );
			traits->SetMemberUShort( "single_depth", ci->m_nSingleInheritanceDepth );

			auto baseclasses = traits->FindOrCreateMember( "baseclasses" );
			baseclasses->SetArrayElementCount( ci->m_nBaseClassCount );

			for(int i = 0; i < ci->m_nBaseClassCount; i++)
			{
				auto &base_ci = ci->m_pBaseClasses[i];
				auto baseclass = baseclasses->GetArrayElement( i );

				baseclass->SetMemberUInt( "offset", base_ci.m_nOffset );
				baseclass->SetMemberInt( "ref_idx", ReadDeclClass( base_ci.m_pClass->m_pDeclaredClass ) );
			}
		}
	}

	ApplyNetVarOverrides( type );

	auto members = traits->FindOrCreateMember( "members" );

	if(ci)
	{
		members->SetArrayElementCount( ci->m_nFieldCount );

		for(int i = 0; i < ci->m_nFieldCount; i++)
		{
			auto field = ci->m_pFields[i];
			auto member = members->GetArrayElement( i );

			member->SetMemberString( "name", field.m_pszName );
			member->SetMemberInt( "offset", field.m_nSingleInheritanceOffset );
			auto member_traits = member->FindOrCreateMember( "traits" );

			ReadMetaTags( member_traits, field.m_pStaticMetadata, field.m_nStaticMetadataCount );
			ReadMemberSchemaType( member_traits, field.m_pType );
		}
	}
	else
	{
		members->SetArrayElementCount( 0 );
	}

	return idx;
}

int SchemaReader::ReadDeclEnum( CSchemaType_DeclaredEnum *type )
{
	auto [def, idx] = CreateDefEntry( type );

	if(!def)
		return idx;

	auto traits = def->FindOrCreateMember( "traits" );
	auto ci = type->m_pEnumInfo;

	LinkChildParentScopeDecls( traits, type, idx );
	ReadFlags( traits, type );

	if(ci)
	{
		ReadMetaTags( traits, ci->m_pStaticMetadata, ci->m_nStaticMetadataCount );

		auto fields = traits->FindOrCreateMember( "fields" );
		fields->SetArrayElementCount( ci->m_nEnumeratorCount );
		for(int i = 0; i < ci->m_nEnumeratorCount; i++)
		{
			auto enumf = ci->m_pEnumerators[i];
			auto field = fields->GetArrayElement( i );

			field->SetMemberString( "name", enumf.m_pszName );
			field->SetMemberInt64( "value", enumf.m_nValue );

			ReadMetaTags( field, enumf.m_pStaticMetadata, enumf.m_nStaticMetadataCount, true );
		}
	}
	else
	{
		traits->FindOrCreateMember( "fields" )->SetArrayElementCount( 0 );
	}

	return idx;
}

void SchemaReader::ReadAtomicInfo( SchemaAtomicTypeInfo_t *info )
{
	auto def = GetAtomicDefs()->ArrayAddElementToTail();

	def->SetMemberString( "name", info->m_pszName );
	def->SetMemberInt( "token", info->m_nAtomicID );

	ReadMetaTags( def, info->m_pStaticMetadata, info->m_nStaticMetadataCount, true );
}

void SchemaReader::ReadMetaTags( KeyValues3 *root, SchemaMetadataEntryData_t *data, int count, bool append_traits )
{
	if(count <= 0 || !IsDumpingMetaTags())
		return;
	
	if(append_traits)
		root = root->FindOrCreateMember( "traits" );

	auto metatags = root->FindOrCreateMember( "metatags" );
	metatags->SetArrayElementCount( count );

	for(int i = 0; i < count; i++)
	{
		auto meta = data[i];
		auto metatag = metatags->GetArrayElement( i );

		metatag->SetMemberString( "name", meta.m_pszName );

		std::string metavalue = SchemaMetadataToString::Eval( &meta );
		if(!metavalue.empty())
			metatag->SetMemberString( "value", metavalue.c_str() );
	}
}

void SchemaReader::ReadFlags( KeyValues3 *root, CSchemaType *type )
{
	if(auto class_decl = type->ReinterpretAs<CSchemaType_DeclaredClass>())
	{
		if(!class_decl->m_pClassInfo)
			return;

		auto class_flags = class_decl->m_pClassInfo->m_nFlags1;

		if(class_flags == 0)
			return;

		auto flags = root->FindOrCreateMember( "flags" );
		static std::pair<uint32, const char *> s_FlagMap[] = {
			{ SCHEMA_CF1_HAS_VIRTUAL_MEMBERS, "has_virtual_members" },
			{ SCHEMA_CF1_IS_ABSTRACT, "is_abstract" },
			{ SCHEMA_CF1_HAS_TRIVIAL_CONSTRUCTOR, "has_trivial_constructor" },
			{ SCHEMA_CF1_HAS_TRIVIAL_DESTRUCTOR, "has_trivial_destructor" },
			{ SCHEMA_CF1_LIMITED_METADATA, "limited_metadata" },
			{ SCHEMA_CF1_INHERITANCE_DEPTH_CALCULATED, "inheritance_depth_calculated" },
			{ SCHEMA_CF1_MODULE_LOCAL_TYPE_SCOPE, "local_type_scope" },
			{ SCHEMA_CF1_GLOBAL_TYPE_SCOPE, "global_type_scope" },
			{ SCHEMA_CF1_CONSTRUCT_ALLOWED, "construct_allowed" },
			{ SCHEMA_CF1_CONSTRUCT_DISALLOWED, "construct_disallowed" },
			{ SCHEMA_CF1_INFO_TAG_MNetworkAssumeNotNetworkable, "MNetworkAssumeNotNetworkable" },
			{ SCHEMA_CF1_INFO_TAG_MNetworkNoBase, "MNetworkNoBase" },
			{ SCHEMA_CF1_INFO_TAG_MIgnoreTypeScopeMetaChecks, "MIgnoreTypeScopeMetaChecks" },
			{ SCHEMA_CF1_INFO_TAG_MDisableDataDescValidation, "MDisableDataDescValidation" },
			{ SCHEMA_CF1_INFO_TAG_MClassHasEntityLimitedDataDesc, "MClassHasEntityLimitedDataDesc" },
			{ SCHEMA_CF1_INFO_TAG_MClassHasCustomAlignedNewDelete, "MClassHasCustomAlignedNewDelete" },
			{ SCHEMA_CF1_UNK016, "unk016" },
			{ SCHEMA_CF1_INFO_TAG_MConstructibleClassBase, "MConstructibleClassBase" },
			{ SCHEMA_CF1_INFO_TAG_MHasKV3TransferPolymorphicClassname, "MHasKV3TransferPolymorphicClassname" }
		};

		for(int i = 0; i < ARRAYSIZE( s_FlagMap ); i++)
		{
			if((class_flags & s_FlagMap[i].first) != 0)
			{
				flags->ArrayAddElementToTail()->SetString( s_FlagMap[i].second );
				class_flags &= ~s_FlagMap[i].first;
			}
		}

		char buf[64];
		for(int i = 0; i < sizeof( class_flags ) * 8; i++)
		{
			if((class_flags & i) != 0)
			{
				std::snprintf( buf, sizeof( buf ), "UNKNOWN_BIT_%d", i );
				flags->ArrayAddElementToTail()->SetString( buf );

				if(IsVerboseLogging())
				{
					META_CONPRINTF( "Found unknown flag bit %d for class (%s)\n", i, type->m_sTypeName.Get() );
				}
			}
		}
	}
	else if(auto enum_decl = type->ReinterpretAs<CSchemaType_DeclaredEnum>())
	{
		if(!enum_decl->m_pEnumInfo)
			return;

		auto enum_flags = enum_decl->m_pEnumInfo->m_nFlags;

		if(enum_flags == 0)
			return;

		auto flags = root->FindOrCreateMember( "flags" );
		static std::pair<uint32, const char *> s_FlagMap[] = {
			{ SCHEMA_EF_IS_REGISTERED, "is_registered" },
			{ SCHEMA_EF_MODULE_LOCAL_TYPE_SCOPE, "local_type_scope" },
			{ SCHEMA_EF_GLOBAL_TYPE_SCOPE, "global_type_scope" }
		};

		for(int i = 0; i < ARRAYSIZE( s_FlagMap ); i++)
		{
			if((enum_flags & s_FlagMap[i].first) != 0)
			{
				flags->ArrayAddElementToTail()->SetString( s_FlagMap[i].second );
				enum_flags &= ~s_FlagMap[i].first;
			}
		}

		char buf[64];
		for(int i = 0; i < sizeof( enum_flags ) * 8; i++)
		{
			if((enum_flags & i) != 0)
			{
				std::snprintf( buf, sizeof( buf ), "UNKNOWN_BIT_%d", i );
				flags->ArrayAddElementToTail()->SetString( buf );

				if(IsVerboseLogging())
				{
					META_CONPRINTF( "Found unknown flag bit %d for enum (%s)\n", i, type->m_sTypeName.Get() );
				}
			}
		}
	}
}

template<typename METATAG>
inline void SchemaReader::ReadPulseDomains( KeyValues3 *root, std::map<std::string, KeyValues3 *> &domains )
{
	CUtlVector<const CSchemaClassInfo *> classes;
	SchemaSystem()->FindClassesByMeta( METATAG::Tag(), SCHEMA_ITER_MULTI_PARENT, &classes );

	for(int i = 0; i < classes.Count(); i++)
	{
		auto ci = classes[i];

		if(auto metatag = SchemaMetadataIterator( ci->m_pStaticMetadata, ci->m_nStaticMetadataCount ).FindTag( METATAG::Tag() ))
		{
			if(auto meta_binding = METATAG::From( metatag ))
			{
				if(auto binding = meta_binding->Value())
				{
					auto iter = domains.find( binding->m_Name.Get() );
					KeyValues3 *domain;

					if(iter == domains.end())
					{
						domain = domains.emplace( binding->m_Name.Get(), root->ArrayAddElementToTail() ).first->second;

						domain->SetMemberString( "name", binding->m_Name );
					}
					else
						domain = iter->second;
					
					bool created = false;
					auto cpp_scopes = domain->FindOrCreateMember( "cpp_scopes", &created );

					if(created)
						cpp_scopes->SetToEmptyArray();

					KeyValues3 *functions = nullptr;
					for(int k = 0; k < cpp_scopes->GetArrayElementCount(); k++)
					{
						auto scope = cpp_scopes->GetArrayElement( k );
						
						if(std::strcmp( scope->GetMemberString( "name" ), ci->m_pszName ) == 0)
						{
							functions = scope->FindOrCreateMember( "functions" );
							break;
						}
					}

					if(!functions)
					{
						auto scope = cpp_scopes->ArrayAddElementToTail();
						scope->SetMemberString( "name", ci->m_pszName );
						functions = scope->FindOrCreateMember( "functions" );
						functions->SetToEmptyArray();
					}

					ReadPulseDomianFunctions( functions, binding->m_Functions, binding->m_FunctionCount );
					ReadPulseDomianFunctions( functions, binding->m_EventFunctions, binding->m_EventFunctionCount );
				}
			}
		}
	}
}

template <typename DOMAIN_FUNCTION>
void SchemaReader::ReadPulseDomianFunctions( KeyValues3 *root, DOMAIN_FUNCTION *functions, int count )
{
	for(int i = 0; i < count; i++)
	{
		auto func = root->ArrayAddElementToTail();
		auto &func_binding = functions[i];

		func->SetMemberString( "name", func_binding.m_Name.Get() );
		func->SetMemberString( "library_name", func_binding.m_LibraryName.Get() );
		func->SetMemberString( "description", func_binding.m_Description.Get() );

		if constexpr (std::is_same_v<DOMAIN_FUNCTION, CPulseLibraryEventFunction>)
			func->SetMemberString( "type", "event" );
		else
			func->SetMemberString( "type", "plain" );

		auto params = func->FindOrCreateMember( "params" );
		params->SetToEmptyArray();

		if(func_binding.m_Params)
		{
			auto binding_params = func_binding.m_Params();
			for(int i = 0; i < func_binding.m_ParamCount; i++)
			{
				auto &binding_param = binding_params[i];
				auto param = params->ArrayAddElementToTail();

				param->SetMemberString( "name", binding_param.m_Name.GetString() );
				param->SetMemberString( "type", binding_param.m_TypeDesc.ToString().c_str() );

				if(binding_param.HasDefaultValue())
					param->SetMemberString( "default_value", binding_param.DefaultValueToString().c_str() );

				ReadMetaTags( param, binding_param.m_StaticMetadata, binding_param.m_StaticMetadataCount, true );
			}
		}

		auto rets = func->FindOrCreateMember( "rets" );
		rets->SetToEmptyArray();

		if(func_binding.m_Rets)
		{
			auto binding_rets = func_binding.m_Rets();
			for(int i = 0; i < func_binding.m_RetCount; i++)
			{
				auto &binding_ret = binding_rets[i];
				auto ret = rets->ArrayAddElementToTail();

				ret->SetMemberString( "name", binding_ret.m_Name.GetString() );
				ret->SetMemberString( "type", binding_ret.m_TypeDesc.ToString().c_str() );

				if(binding_ret.HasDefaultValue())
					ret->SetMemberString( "default_value", binding_ret.DefaultValueToString().c_str() );

				ReadMetaTags( ret, binding_ret.m_StaticMetadata, binding_ret.m_StaticMetadataCount, true );
			}
		}

		ReadMetaTags( func, func_binding.m_StaticMetadata, func_binding.m_StaticMetadataCount, true );
	}
}

void SchemaReader::ReadPulseDomainsInfo( KeyValues3 *root, std::map<std::string, KeyValues3 *> &domains )
{
	CUtlVector<const CSchemaClassInfo *> classes;
	SchemaSystem()->FindClassesByMeta( MPulseInstanceDomainInfo::Tag(), SCHEMA_ITER_MULTI_PARENT, &classes );

	for(int i = 0; i < classes.Count(); i++)
	{
		auto ci = classes[i];

		if(auto metatag = SchemaMetadataIterator( ci->m_pStaticMetadata, ci->m_nStaticMetadataCount ).FindTag( MPulseInstanceDomainInfo::Tag() ))
		{
			if(auto meta_binding = MPulseInstanceDomainInfo::From( metatag ))
			{
				if(auto domain_info = meta_binding->Value())
				{
					auto iter = domains.find( domain_info->m_Name.Get() );
					KeyValues3 *domain;
					
					if(iter != domains.end())
					{
						domain = iter->second;

						domain->SetMemberString( "description", domain_info->m_Description.Get() );
						domain->SetMemberString( "friendly_name", domain_info->m_FriendlyName.Get() );
						domain->SetMemberString( "cursor", domain_info->m_CursorName.Get() );
					}
				}
			}
		}
	}
}

void SchemaReader::ReadPulseBindings()
{
	if(!IsDumpingPulseBindings())
		return;

	META_CONPRINTF( "Reading pulse_bindings...\n" );

	auto pulse_bindings = GetRoot()->FindOrCreateMember( "pulse_bindings" );
	pulse_bindings->SetArrayElementCount( 0 );

	std::map<std::string, KeyValues3 *> domains;
	ReadPulseDomains<MPulseLibraryBindings>( pulse_bindings, domains );
	ReadPulseDomains<MPulseCellMethodBindings>( pulse_bindings, domains );

	ReadPulseDomainsInfo( pulse_bindings, domains );
}

CSchemaType_DeclaredClass *SchemaReader::FindSchemaTypeInTypeScopes( const char *name )
{
	auto ci = SchemaSystem()->FindClassByScopedName( name ).Get();

	if(ci)
		return ci->m_pDeclaredClass;
	
	for(int i = 0; i < SchemaSystem()->m_TypeScopes.GetNumStrings(); i++)
	{
		auto ts = SchemaSystem()->m_TypeScopes[i];
		ci = ts->FindDeclaredClass( name ).Get();
		
		if(ci)
			return ci->m_pDeclaredClass;
	}

	return nullptr;
}

bool SchemaReader::ApplyNetVarOverrides( CSchemaType_DeclaredClass *type )
{
	if(!IsApplyingNetVarOverrides())
		return true;
	
	auto ci = type->m_pClassInfo;

	if(!ci || ci->m_nBaseClassCount <= 0)
		return true;

	for(auto &meta : SchemaMetadataIterator( ci->m_pStaticMetadata, ci->m_nStaticMetadataCount ))
	{
		if(auto var_override = MNetworkVarTypeOverride::From( &meta ))
		{
			auto var_ci = FindSchemaTypeInTypeScopes( var_override->Value().m_TypeName );

			if(!var_ci)
			{
				if(IsVerboseLogging())
				{
					META_CONPRINTF( "Failed to find type override (%s) for class (%s)\n", var_override->Value().m_TypeName, ci->m_pszName );
				}

				continue;
			}

			auto idx = ReadDeclClass( var_ci->ReinterpretAs<CSchemaType_DeclaredClass>() );

			if(idx == -1)
			{
				if(IsVerboseLogging())
				{
					META_CONPRINTF( "Failed to find type override (%s) for class (%s) not initialized\n", var_override->Value().m_TypeName, ci->m_pszName );
				}

				continue;
			}

			ApplyNetVarOverrides( type, var_override->Value().m_FieldName, idx );
		}
	}

	return true;
}

bool SchemaReader::ApplyNetVarOverrides( CSchemaType_DeclaredClass *root, const char *field_to_overwrite, int type_override_idx )
{
	auto ci = root->m_pClassInfo;

	if(!ci)
		return true;

	for(int i = 0; i < ci->m_nFieldCount; i++)
	{
		auto &field = ci->m_pFields[i];

		if(std::strcmp( field.m_pszName, field_to_overwrite ) == 0)
		{
			auto def = FindDefEntry( root );

			if(!def)
			{
				if(IsVerboseLogging())
				{
					META_CONPRINTF( "Failed to apply netvar override (%s) due to type (%s) being not yet initialized.\n", field_to_overwrite, ci->m_pszName );
				}

				return true;
			}

			auto traits = def->FindMember( "traits" );

			if(!traits)
				return true;
			
			auto members = traits->FindMember( "members" );

			if(!members)
				return true;

			for(int k = 0; k < members->GetArrayElementCount(); k++)
			{
				auto member = members->GetArrayElement( k );

				if(std::strcmp( member->GetMemberString( "name" ), field_to_overwrite ) == 0)
				{
					auto mem_traits = member->FindMember( "traits" );
					if(!mem_traits)
						break;
					
					auto subtype = mem_traits->FindMember( "subtype" );
					do
					{
						if(std::strcmp( subtype->GetMemberString( "type" ), "ref" ) == 0)
						{
							subtype->SetMemberInt( "ref_idx", type_override_idx );
							return true;
						}

						subtype = subtype->FindMember( "subtype" );

					} while(subtype);

					return true;
				}
			}

			if(IsVerboseLogging())
			{
				META_CONPRINTF( "Failed to apply netvar override (%s) due to missing member field in kv.\n", field_to_overwrite );
			}

			return true;
		}
	}

	for(int i = 0; i < ci->m_nBaseClassCount; i++)
	{
		auto &baseclass = ci->m_pBaseClasses[i];

		if(ApplyNetVarOverrides( baseclass.m_pClass->m_pDeclaredClass, field_to_overwrite, type_override_idx ))
			return true;
	}

	return true;
}

void SchemaReader::LinkChildParentScopeDecls( KeyValues3 *child_traits, CSchemaType *child, int child_idx )
{
	if(IsIgnoringParentScopes())
		return;

	// Account for classes/structs defined within other classes/structs
	CSplitString class_scoping( child->m_sTypeName.Get(), "::" );
	if(class_scoping.Count() > 1)
	{
		auto parent_type = FindSchemaTypeInTypeScopes( class_scoping[class_scoping.Count() - 2] );

		if(!parent_type)
		{
			if(IsVerboseLogging())
				META_CONPRINTF( "Failed to find parent scope class for \"%s\".\n", child->m_sTypeName.Get() );

			// Let child class to know that parent decl is unavailable
			child_traits->FindOrCreateMember( "parent_class_idx" )->SetInt( -1 );
			return;
		}

		auto parent_idx = ReadDeclClass( parent_type );
		auto parent_def = FindDefEntry( parent_idx );
		auto parent_traits = parent_def->FindOrCreateMember( "traits" );

		// Add a ref of child class decl to parent decl
		auto child_class_decls = parent_traits->FindOrCreateMember( "child_class_idx" );
		child_class_decls->ArrayAddElementToTail()->SetInt( child_idx );

		// Add a ref of parent class decl to child decl
		child_traits->FindOrCreateMember( "parent_class_idx" )->SetInt( parent_idx );
	}
}

bool SchemaReader::WriteToOutDir()
{
	bool success = WriteToKV3();
	success |= WriteToJSON();

	return success;
}

bool SchemaReader::WriteToKV3()
{
	if(!IsDumpingToKV3())
		return false;

	CUtlString err, out;
	SaveKV3Text_ToString( g_KV3Encoding_Text, GetRoot(), &err, &out );

	if(!err.IsEmpty())
	{
		META_CONPRINTF( "Failed to save kv3 to file! Reason: \"%s\"\n", err.Get() );
		return false;
	}

	auto t = std::time( nullptr );
	auto tm = *std::localtime( &t );
	std::ostringstream ss;

	ss << std::put_time( &tm, "%d%m%y" ) << ".kv3";

	return WriteToFile( ss.str(), out.Get() );
}

bool SchemaReader::WriteToJSON()
{
	if(!IsDumpingToJSON())
		return false;

	CUtlString err, out;
	SaveKV3AsJSON( GetRoot(), &err, &out );

	if(!err.IsEmpty())
	{
		META_CONPRINTF( "Failed to save kv3 as json! Reason: \"%s\"\n", err.Get() );
		return false;
	}

	auto t = std::time( nullptr );
	auto tm = *std::localtime( &t );
	std::ostringstream ss;

	ss << std::put_time( &tm, "%d%m%y" ) << ".json";

	return WriteToFile( ss.str(), out.Get() );
}

bool SchemaReader::WriteToFile( const std::string &filename, const char *content )
{
	ValidateOutDir();

	auto file_path = m_OutPath / filename;
	std::ofstream fs( file_path );

	fs << content;

	fs.close();

	META_CONPRINTF( "Wrote file output to %s\n", file_path.string().c_str() );

	return true;
}
