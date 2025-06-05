#pragma once

#include "schema_metadata.h"

#include <sstream>

struct SchemaMetadataEntryData_t;

#if SOURCE_ENGINE == SE_CS2
enum PulseValueType_t : int32
{
	PVAL_INVALID = -1,
	PVAL_BOOL = 0,
	PVAL_INT = 1,
	PVAL_FLOAT = 2,
	PVAL_STRING = 3,
	PVAL_VEC3 = 4,
	PVAL_TRANSFORM = 5,
	PVAL_COLOR_RGB = 6,
	PVAL_EHANDLE = 7,
	PVAL_RESOURCE = 8,
	PVAL_SNDEVT_GUID = 9,
	PVAL_SNDEVT_NAME = 10,
	PVAL_ENTITY_NAME = 11,
	PVAL_OPAQUE_HANDLE = 12,
	PVAL_TYPESAFE_INT = 13,
	PVAL_CURSOR_FLOW = 14,
	PVAL_ANY = 15,
	PVAL_SCHEMA_ENUM = 16,
	PVAL_PANORAMA_PANEL_HANDLE = 17,
	PVAL_TEST_HANDLE = 18,
	PVAL_COUNT = 19,
};
#elif SOURCE_ENGINE != SE_CS2
enum PulseValueType_t : int32
{
	PVAL_INVALID = -1,
	PVAL_BOOL = 0,
	PVAL_INT = 1,
	PVAL_FLOAT = 2,
	PVAL_STRING = 3,
	PVAL_VEC3 = 4,
	PVAL_QANGLE = 5,
	PVAL_VEC3_WORLDSPACE = 6,
	PVAL_TRANSFORM = 7,
	PVAL_TRANSFORM_WORLDSPACE = 8,
	PVAL_COLOR_RGB = 9,
	PVAL_GAMETIME = 10,
	PVAL_EHANDLE = 11,
	PVAL_RESOURCE = 12,
	PVAL_SNDEVT_GUID = 13,
	PVAL_SNDEVT_NAME = 14,
	PVAL_ENTITY_NAME = 15,
	PVAL_OPAQUE_HANDLE = 16,
	PVAL_TYPESAFE_INT = 17,
	PVAL_CURSOR_FLOW = 18,
	PVAL_ANY = 19,
	PVAL_SCHEMA_ENUM = 20,
	PVAL_PANORAMA_PANEL_HANDLE = 21,
	PVAL_TEST_HANDLE = 22,
	PVAL_ARRAY = 23,
	PVAL_COUNT = 24,
};
#endif

struct PulseParamType
{
	bool IsValid() const { return m_Type != PVAL_INVALID; }

	std::string ToString() const
	{
		std::ostringstream ss;

		switch(m_Type)
		{
#if SOURCE_ENGINE != SE_CS2
			case PVAL_ARRAY:
			{
				if(m_SubType && m_SubType->IsValid())
				{
					ss << m_SubType->ToString();
				}
				else if(SchemaReader::IsVerboseLogging())
				{
					// META_CONPRINTF( "Failed to parse pulse array subtype!\n" );
					ss << "any";
				}

				ss << "[]"; break;
			}

			case PVAL_VEC3_WORLDSPACE: ss << "vec3_world"; break;
			case PVAL_TRANSFORM_WORLDSPACE:	ss << "transform_world"; break;
			case PVAL_GAMETIME:		ss << "game_time"; break;
#endif

			case PVAL_BOOL:			ss << "bool"; break;
			case PVAL_INT:			ss << "int"; break;
			case PVAL_FLOAT:		ss << "float"; break;
			case PVAL_STRING:		ss << "string"; break;
			case PVAL_VEC3:			ss << "vec3"; break;
#if SOURCE_ENGINE != SE_CS2
			case PVAL_QANGLE:		ss << "qangle"; break;
#endif
			case PVAL_TRANSFORM:	ss << "transform"; break;
			case PVAL_COLOR_RGB:	ss << "color"; break;
			case PVAL_EHANDLE:		ss << "ehandle<" << (m_LibraryClass ? m_LibraryClass : "" ) << ">"; break;
			case PVAL_RESOURCE:		ss << "resource<" << (m_LibraryClass ? m_LibraryClass : "") << ">"; break;
			case PVAL_SNDEVT_GUID:	ss << "sndevent_guid"; break;
			case PVAL_SNDEVT_NAME:	ss << "sndevent_name"; break;
			case PVAL_ENTITY_NAME:	ss << "entname"; break;
			case PVAL_OPAQUE_HANDLE:ss << (m_LibraryClass ? m_LibraryClass : "schema" ) << "*"; break;
			case PVAL_TYPESAFE_INT:	ss << (m_LibraryClass ? m_LibraryClass : "!int"); break;
			case PVAL_CURSOR_FLOW:	ss << "cursor"; break;
			case PVAL_ANY:			ss << "any"; break;
			case PVAL_SCHEMA_ENUM:	ss << "enum {" << m_LibraryClass << "}"; break;
			case PVAL_PANORAMA_PANEL_HANDLE: ss << "panorama_panel_handle"; break;
			case PVAL_TEST_HANDLE:	ss << "testhandle<" << (m_LibraryClass ? m_LibraryClass : "") << ">"; break;
			default:
			{
				if(SchemaReader::IsVerboseLogging())
				{
					META_CONPRINTF( "Found unknown pulse type (%d).\n", m_Type );
				}

				ss << "<unknown>";
				return ss.str();
			}
		}

		// This is mostly to catch any changes in the pulse system
		if(m_LibraryClass &&
			m_Type != PVAL_EHANDLE &&
			m_Type != PVAL_OPAQUE_HANDLE &&
			m_Type != PVAL_TYPESAFE_INT &&
			m_Type != PVAL_SCHEMA_ENUM &&
			m_Type != PVAL_TEST_HANDLE &&
			m_Type != PVAL_EHANDLE &&
			m_Type != PVAL_RESOURCE)
		{
			if(SchemaReader::IsVerboseLogging())
			{
				META_CONPRINTF( "Found valid library class (%s) field for type (%d).\n", m_LibraryClass, m_Type );
			}
		}

#if SOURCE_ENGINE == SE_CS2
		if(m_IsArray)
		{
			ss << "[]";
		}
#endif
		
		return ss.str();
	}

#if SOURCE_ENGINE == SE_CS2
	bool m_IsArray;
	PulseValueType_t m_Type;
	const char *m_LibraryClass;
#else
	PulseValueType_t m_Type;
	PulseParamType *m_SubType;
	const char *m_LibraryClass;
#endif
};

class CPulseFunctionParam
{
public:
	bool HasDefaultValue() const { return m_DefaultValueTypeDesc.IsValid(); }
	template <typename T>
	T &DefaultValue() const { return *(T *)&m_DefaultValue; }

	std::string TypeToString() const { return m_TypeDesc.ToString(); }
	std::string DefaultValueToString() const
	{
		if(!HasDefaultValue())
			return "";

		char buf[128];
		switch(m_DefaultValueTypeDesc.m_Type)
		{
			case PVAL_BOOL:			return std::to_string( DefaultValue<bool>() );
			case PVAL_INT:			return std::to_string( DefaultValue<int>() );
			case PVAL_FLOAT:		return std::to_string( DefaultValue<float>() );
			case PVAL_STRING:		return DefaultValue<CUtlString>().Get();
			case PVAL_SCHEMA_ENUM:	return std::to_string( DefaultValue<int64>() );
			case PVAL_ANY:			return std::to_string( *DefaultValue<int64 *>() );

			case PVAL_VEC3:
			{
				auto vec = DefaultValue<Vector *>();
				std::snprintf( buf, sizeof( buf ), "(%f %f %f)", vec->x, vec->y, vec->z );
				return buf;
			}
			
			case PVAL_COLOR_RGB:
			{
				auto clr = DefaultValue<Color>();
				std::snprintf( buf, sizeof( buf ), "(%d %d %d %d)", clr.r(), clr.g(), clr.b(), clr.a() );
				return buf;
			}

			default:
			{
				if(SchemaReader::IsVerboseLogging())
				{
					META_CONPRINTF( "Unable to stringify default value for type (%d).\n", m_DefaultValueTypeDesc.m_Type );
				}

				return "!!UNKNOWN!!";
			}
		}
	}

#if SOURCE_ENGINE == SE_CS2
public:
	CKV3MemberName m_Name;

	PulseParamType m_TypeDesc;

	void *m_DefaultValue;
	PulseParamType m_DefaultValueTypeDesc;

private:
	void *m_unk003;

public:
	uint16 m_StaticMetadataCount;
	SchemaMetadataEntryData_t *m_StaticMetadata;

#else
public:
	CKV3MemberName m_Name;

	PulseParamType m_TypeDesc;

private:
	void *m_unk003;

	void *m_DefaultValue;
	PulseParamType m_DefaultValueTypeDesc;

public:
	uint16 m_StaticMetadataCount;
	SchemaMetadataEntryData_t *m_StaticMetadata;

	void *m_ArrayPolymorphicFn;

private:
	void *m_unk005;
#endif
};

class CPulseDomainFunction
{
public:
	typedef CPulseFunctionParam *(*CPulseParamFn)();

	CUtlString m_LibraryName;
	CUtlString m_Name;
	CUtlString m_Description;

	int m_ParamCount;
	CPulseParamFn m_Params;

	int m_RetCount;
	CPulseParamFn m_Rets;

	int m_StaticMetadataCount;
	SchemaMetadataEntryData_t *m_StaticMetadata;
};

class CPulseLibraryFunction : public CPulseDomainFunction
{
public:
	uint64 m_Flags;
	bool m_IsLooped;
	void *m_EvalFn;
};

class CPulseLibraryEventFunction : public CPulseDomainFunction
{
public:
	uint64 m_Flags;
	uint8 m_unk101;
};

class CPulseLibraryBinding
{
public:
	CUtlString m_Name;

#if SOURCE_ENGINE == SE_DOTA || SOURCE_ENGINE == SE_DEADLOCK
	int m_Flags;

	int m_FunctionCount;
	CPulseLibraryFunction *m_Functions;

	int m_EventFunctionCount;
	CPulseLibraryEventFunction *m_EventFunctions;
#else

	CUtlString m_Description;

	int m_FunctionCount;
	CPulseLibraryFunction *m_Functions;
#endif
};

class CPulseDomainInfo
{
public:
	CUtlString m_Name;
	CUtlString m_FriendlyName;
	CUtlString m_Description;
	CUtlString m_CursorName;
};

class CPulseHookInfo
{
public:
	int m_FunctionCount;
	CPulseDomainFunction *m_Functions;
};

struct CPulseRequirementPass
{
	int m_Pass;
};

template<> inline std::string SchemaMetadataField<CPulseRequirementPass *>::ToString() const { return Value() ? std::to_string( Value()->m_Pass ) : "1"; }

// Intentionally skip these as these are dumped via SchemaReader::SR_DUMP_PULSE_BINDINGS
template<> inline std::string SchemaMetadataField<CPulseLibraryBinding *>::ToString() const { return "!!SKIPPED!!"; }
template<> inline std::string SchemaMetadataField<CPulseHookInfo *>::ToString() const { return "!!SKIPPED!!"; }

template<> inline std::string SchemaMetadataField<CPulseDomainInfo *>::ToString() const
{
	auto info = Value();

	if(!info)
		return "";

	KeyValues3 kv;

	kv.SetMemberString( "name", info->m_Name.Get() );
	kv.SetMemberString( "friendly_name", info->m_FriendlyName.Get() );
	kv.SetMemberString( "description", info->m_Description.Get() );
	kv.SetMemberString( "cursor", info->m_CursorName.Get() );

	CUtlString err, out;
	if(!SaveKV3Text_NoHeader( &kv, &err, &out ))
	{
		char buf[256];
		std::snprintf( buf, sizeof( buf ), "Failed to save: %s", err.Get() );
		return buf;
	}

	return out.Get();
}

METADATA_TAG( MPulseLibraryBindings, CPulseLibraryBinding * );
METADATA_TAG( MPulseCellMethodBindings, CPulseLibraryBinding * );
METADATA_TAG( MPulseInstanceDomainInfo, CPulseDomainInfo * );
METADATA_TAG( MPulseDomainHookInfo, CPulseHookInfo * );
METADATA_TAG( MPulseCellOutflowHookInfo, CPulseHookInfo * );

METADATA_TAG( MSourceTSDomain, empty_t );
METADATA_TAG( MCellForDomain, const char * );
METADATA_TAG( MPulseSignatureName, const char * );
METADATA_TAG( MPulseDocCustomAttr, const char * );
METADATA_TAG( MPulseRequirementPass, CPulseRequirementPass * );
METADATA_TAG( MPulseRequirementSummaryExpr, const char * );
METADATA_TAG( MPulseRequirementCommit, empty_t );
METADATA_TAG( MPulseRequirementCheck, empty_t );
METADATA_TAG( MPulseDomainOptInValueType, int );

#if SOURCE_ENGINE == SE_CS2
METADATA_TAG( MPulseProvideFeatureTag, const char * );
METADATA_TAG( MPulseDomainOptInFeatureTag, const char * );
#else
METADATA_TAG( MPulseProvideFeatureTag, int );
METADATA_TAG( MPulseDomainOptInFeatureTag, int );
#endif

METADATA_TAG( MPulseEditorIsControlFlowNode, empty_t );
METADATA_TAG( MPulseEditorHeaderIcon, const char * );
METADATA_TAG( MPulseEditorHeaderText, const char * );
METADATA_TAG( MPulseEditorHeaderExpr, const char * );
METADATA_TAG( MPulseEditorSubHeaderText, const char * );
METADATA_TAG( MPulseEditorCanvasItemPreset, const char * );
METADATA_TAG( MPulseEditorCanvasItemSpecKV3, const char * );
METADATA_TAG( MPulseExpressionAlias, const char * );
METADATA_TAG( MPulseCellWithCustomDocNode, empty_t );
METADATA_TAG( MPulseCellOutflow_IsDefault, empty_t );
METADATA_TAG( MPulseCellInflow_IsDefault, empty_t );
METADATA_TAG( MPulseCellInflow, const char * );
METADATA_TAG( MPulseAdvanced, empty_t );
METADATA_TAG( MPulseArgDesc, const char * );
METADATA_TAG( MPulseInternal_IsCursor, empty_t );
METADATA_TAG( MPulseCursorTerminates, empty_t );
METADATA_TAG( MPulseLegacyName, const char * ); 
METADATA_TAG( MPulseInstanceFunction, const char * );
METADATA_TAG( MPulseInstanceStep, const char * );
METADATA_TAG( MPulseLibraryFunction, const char * );
METADATA_TAG( MPulseLibraryStep, const char * );
METADATA_TAG( MPulseDomainHiddenInTool, empty_t );
METADATA_TAG( MPulseDomainOptInGameBlackboard, const char * );
METADATA_TAG( MPulseDomainIsGameBlackboard, empty_t );
METADATA_TAG( MPulseFunctionHiddenInTool, empty_t );
METADATA_TAG( MPulseFunctionDisableRequirements, empty_t );
METADATA_TAG( MPulsePolymorphicDependentReturn, const char * );
METADATA_TAG( MPulsePolymorphicDependentArg, const char * );
