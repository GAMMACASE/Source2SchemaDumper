#pragma once

#include "plugin.h"
#include "schemareader.h"
#include "schemasystem/schematypes.h"

#include <string>
#include <sstream>
#include <map>

#include "string_view"
using namespace std::string_view_literals;

struct SchemaMetadataIterator
{
	SchemaMetadataIterator( SchemaMetadataEntryData_t *data, int count )
		: m_Data( data ), m_Count( count )
	{}

	SchemaMetadataEntryData_t *FindTag( const char *metatag )
	{
		for(auto &metadata : *this)
		{
			if(std::strcmp( metadata.m_pszName, metatag ) == 0)
			{
				return &metadata;
			}
		}

		return nullptr;
	}

	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type        = SchemaMetadataEntryData_t;
		using pointer           = SchemaMetadataEntryData_t *;
		using reference         = SchemaMetadataEntryData_t &;

		Iterator( pointer ptr ) : m_Ptr( ptr ) {}

		reference operator*() const { return *m_Ptr; }
		pointer operator->() { return m_Ptr; }

		Iterator &operator++() { m_Ptr++; return *this; }
		Iterator operator++( int ) { Iterator tmp = *this; ++(*this); return tmp; }

		friend bool operator== ( const Iterator &a, const Iterator &b ) { return a.m_Ptr == b.m_Ptr; };
		friend bool operator!= ( const Iterator &a, const Iterator &b ) { return a.m_Ptr != b.m_Ptr; };

		pointer m_Ptr;
	};
	
	Iterator begin() { return Iterator( &m_Data[0] ); }
	Iterator end() { return Iterator( &m_Data[m_Count] ); }

	SchemaMetadataEntryData_t *m_Data;
	int m_Count;
};

struct CSchemaNetworkVarName
{
	const char *m_FieldName;
	const char *m_TypeName;
};

struct CSchemaNetworkOverride
{
	const char *m_TypeName;
	const char *m_FieldName;
};

struct CSchemaPropertyEditClassAsString
{
	using FnToString = void (*)(void *, void *, CUtlString &);
	using FnFromString = int (*)(void *, void *, CUtlString &);
	
	FnToString m_ToString;
	FnFromString m_FromString;
};

struct CSchemaSendProxyRecipientsFilter
{
	using FnRecipientFilter = void (*)(void *, void *, int64 &);

	void *m_Unk001;
	FnRecipientFilter m_Filter;
	const char *m_FilterName;
	int m_FilterIndex;
};

#if SOURCE_ENGINE == SE_CS2 || SOURCE_ENGINE == SE_DOTA
struct ClassKV3Defaults
{
	KeyValues3 *m_Defaults;
	KeyValues3 *m_unk001;
};
#endif

class KeyValues3;
class IGapTypeQueryRegistrationForScope;

using FnPropertyAttrChangeCb = int64 (*)(void *);
using FnPropertyAttrStateCb = int64 (*)(void *);
using FnPropertyAttrExtraInfo = CUtlString (*)(CUtlString &, void *);
using FnPropertyElementName = void (*)(void *, CUtlString &);
using FnParticleCustomFieldDefaultValue = bool (*)(KeyValues3 *, KeyValues3 *);
using FnLeafSuggestionProvider = void (*)(void *);
#if SOURCE_ENGINE == SE_CS2 || SOURCE_ENGINE == SE_DOTA
using FnGetKV3Defaults = ClassKV3Defaults *(*)();
#else
using FnGetKV3Defaults = KeyValues3 *(*)();
#endif

template <typename T>
struct SchemaMetadataField : public SchemaMetadataEntryData_t
{
	T *operator->() { return reinterpret_cast<T *>(m_pData); }
	const T &Value() const { return *reinterpret_cast<T *>(m_pData); }
	const char *Name() const { return m_pszName; }
	std::string ToString() const { return ""; };
};

template<> inline std::string SchemaMetadataField<const char *>::ToString() const { return Value(); }
template<> inline std::string SchemaMetadataField<const char [8]>::ToString() const { return std::string( Value(), 8 ); }
template<> inline std::string SchemaMetadataField<int>::ToString() const { return std::to_string( Value() ); }
template<> inline std::string SchemaMetadataField<float>::ToString() const { return std::to_string( Value() ); }

template<> inline std::string SchemaMetadataField<empty_t>::ToString() const
{
	if(m_pData != nullptr && SchemaReader::IsVerboseLogging())
	{
		META_CONPRINTF( "Non nullptr data field found for metadata tag \"%s\"\n", m_pszName );
	}

	return "";
}

template<> inline std::string SchemaMetadataField<CSchemaNetworkVarName>::ToString() const
{
	auto &value = Value();

	std::ostringstream ss;
	if(value.m_TypeName != nullptr)
		ss << value.m_TypeName << " " << value.m_FieldName;
	else
		ss << value.m_FieldName;

	return ss.str();
}

template<> inline std::string SchemaMetadataField<CSchemaNetworkOverride>::ToString() const
{
	auto &value = Value();

	std::ostringstream ss;
	if(value.m_TypeName != nullptr)
		ss << value.m_TypeName << "::" << value.m_FieldName;
	else
		ss << value.m_FieldName;

	return ss.str();
}

template<> inline std::string SchemaMetadataField<FnGetKV3Defaults>::ToString() const
{
#if SOURCE_ENGINE == SE_CS2
	if(!Value() || !Value()())
		return "";

	KeyValues3 *kv = Value()()->m_Defaults;
	if(!kv)
		return "";

	CUtlString err, buf;
	std::ostringstream ss;
	if(!SaveKV3Text_NoHeader( kv, &err, &buf ))
	{
		ss << "!!FAILED TO PARSE (" << err.Get() << ")!!";
		return ss.str();
	}

	return buf.Get();
#else
	return "!!NOT SUPPORTED!!";
#endif
}

class SchemaMetadataToString
{
public:
	using FnSchemaMetadataToString = std::string( * )(SchemaMetadataEntryData_t *meta);
	using MetadataMapType = std::map<std::string_view, FnSchemaMetadataToString>;

	SchemaMetadataToString( std::string_view metatag, FnSchemaMetadataToString cb )
	{
		MetadataMap().emplace( metatag, cb );
	}

	static std::string Eval( SchemaMetadataEntryData_t *meta )
	{
		auto iter = MetadataMap().find( meta->m_pszName );
		if(iter == MetadataMap().end())
		{
			if(SchemaReader::IsVerboseLogging())
			{
				META_CONPRINTF( "Unknown metadata tag found \"%s\", can't stringify!\n", meta->m_pszName );
			}

			return "!!UNKNOWN!!";
		}

		return iter->second( meta );
	}

	template <typename T>
	static std::string FnToString( SchemaMetadataEntryData_t *meta )
	{
		return reinterpret_cast<SchemaMetadataField<T> *>(meta)->ToString();
	}

private:
	static MetadataMapType &MetadataMap()
	{
		static MetadataMapType s_MetadataTagsMap;
		return s_MetadataTagsMap;
	}
};

#define METADATA_TAG( name, type ) struct name : public SchemaMetadataField<type> {\
	static name *From( SchemaMetadataEntryData_t *other )\
	{ return std::strcmp( other->m_pszName, #name ) == 0 ? reinterpret_cast<name *>(other) : nullptr; }\
	static const char *Tag() { return s_ThisClassName; }\
	static inline const char *s_ThisClassName = #name;\
};\
SchemaMetadataToString name##_decl( #name##sv, SchemaMetadataToString::FnToString<type> );

METADATA_TAG( MKV3TransferName, const char * );
METADATA_TAG( MFieldVerificationName, const char * );
METADATA_TAG( MVectorIsSometimesCoordinate, const char * );
METADATA_TAG( MEntitySubclassScopeFile, const char * );
METADATA_TAG( MScriptDescription, const char * );
METADATA_TAG( MAlternateSemanticName, const char * );

METADATA_TAG( MNetworkSerializeAs, const char * );
METADATA_TAG( MNetworkEncoder, const char * );
METADATA_TAG( MNetworkChangeCallback, const char * );
METADATA_TAG( MNetworkUserGroup, const char * );
METADATA_TAG( MNetworkAlias, const char * );
METADATA_TAG( MNetworkTypeAlias, const char * );
METADATA_TAG( MNetworkSerializer, const char * );
METADATA_TAG( MNetworkExcludeByName, const char * );
METADATA_TAG( MNetworkExcludeByUserGroup, const char * );
METADATA_TAG( MNetworkIncludeByName, const char * );
METADATA_TAG( MNetworkIncludeByUserGroup, const char * );
METADATA_TAG( MNetworkUserGroupProxy, const char * );
METADATA_TAG( MNetworkReplayCompatField, const char * );
METADATA_TAG( MNetworkVarEmbeddedFieldOffsetDelta, int );
METADATA_TAG( MNetworkBitCount, int );
METADATA_TAG( MNetworkPriority, int );
METADATA_TAG( MNetworkEncodeFlags, int );
METADATA_TAG( MNetworkMinValue, float );
METADATA_TAG( MNetworkMaxValue, float );
METADATA_TAG( MNetworkVarNames, CSchemaNetworkVarName );
METADATA_TAG( MNetworkOverride, CSchemaNetworkOverride );
METADATA_TAG( MNetworkVarTypeOverride, CSchemaNetworkVarName );
METADATA_TAG( MNetworkSendProxyRecipientsFilter, CSchemaSendProxyRecipientsFilter );
METADATA_TAG( MNetworkVarsAtomic, empty_t );
METADATA_TAG( MNetworkEnable, empty_t );
METADATA_TAG( MNetworkDisable, empty_t );
METADATA_TAG( MNetworkPolymorphic, empty_t );
METADATA_TAG( MNetworkOutOfPVSUpdates, int * );
METADATA_TAG( MNetworkChangeAccessorFieldPathIndex, empty_t );

METADATA_TAG( MResourceTypeForInfoType, const char[8] );
METADATA_TAG( MGapTypeQueriesForScopeSingleton, IGapTypeQueryRegistrationForScope * );
METADATA_TAG( MGetKV3ClassDefaults, FnGetKV3Defaults );

METADATA_TAG( MPropertyFriendlyName, const char * );
METADATA_TAG( MPropertyDescription, const char * );
METADATA_TAG( MPropertyAttributeRange, const char * );
METADATA_TAG( MPropertyStartGroup, const char * );
METADATA_TAG( MPropertyAttributeChoiceName, const char * );
METADATA_TAG( MPropertyGroupName, const char * );
METADATA_TAG( MPropertyAttributeEditor, const char * );
METADATA_TAG( MPropertySuppressExpr, const char * );
METADATA_TAG( MPropertyArrayElementNameKey, const char * );
METADATA_TAG( MPropertyCustomFGDType, const char * );
METADATA_TAG( MPropertySuppressBaseClassField, const char * );
METADATA_TAG( MPropertyAttributeSuggestionName, const char * );
METADATA_TAG( MPropertyProvidesEditContextString, const char * );
METADATA_TAG( MPropertyEditContextOverrideKey, const char * );
METADATA_TAG( MPropertyCustomEditor, const char * );
METADATA_TAG( MPropertyEditClassAsString, CSchemaPropertyEditClassAsString );
METADATA_TAG( MPropertyAttrChangeCallback, FnPropertyAttrChangeCb );
METADATA_TAG( MPropertyAttrStateCallback, FnPropertyAttrStateCb );
METADATA_TAG( MPropertyAttrExtraInfoFn, FnPropertyAttrExtraInfo );
METADATA_TAG( MPropertyElementNameFn, FnPropertyElementName );
METADATA_TAG( MPropertyLeafSuggestionProviderFn, FnLeafSuggestionProvider );
METADATA_TAG( MPropertySuppressEnumerator, empty_t );
METADATA_TAG( MPropertyFlattenIntoParentRow, empty_t );
METADATA_TAG( MPropertyAutoRebuildOnChange, empty_t );
METADATA_TAG( MPropertyPolymorphicClass, empty_t );
METADATA_TAG( MPropertyAutoExpandSelf, empty_t );
METADATA_TAG( MPropertyColorPlusAlpha, empty_t );
METADATA_TAG( MPropertySuppressField, empty_t );
METADATA_TAG( MPropertyHideField, empty_t );
METADATA_TAG( MPropertyReadOnly, empty_t );
METADATA_TAG( MPropertySortPriority, int );

METADATA_TAG( MVDataRoot, empty_t );
METADATA_TAG( MVDataSingleton, empty_t );
METADATA_TAG( MVDataPromoteField, empty_t );
METADATA_TAG( MVDataAnonymousNode, empty_t );
METADATA_TAG( MVDataNodeType, int );
METADATA_TAG( MVDataOverlayType, int );
METADATA_TAG( MVDataFileExtension, const char * );
METADATA_TAG( MVDataPreviewWidget, const char * );
METADATA_TAG( MVDataAssociatedFile, const char * );
METADATA_TAG( MVDataOutlinerIconExpr, const char * );
METADATA_TAG( MVDataUniqueMonotonicInt, const char * );
METADATA_TAG( MVDataUseLinkedEntityClasses, empty_t );

METADATA_TAG( MIsBoxedIntegerType, empty_t );
METADATA_TAG( MIsBoxedFloatType, empty_t );

METADATA_TAG( MModelGameData, empty_t );
METADATA_TAG( MGapNotNull, empty_t );
METADATA_TAG( MCustomFGDMetadata, const char * );
METADATA_TAG( MFgdFromSchemaEditablePolymorphicThisClass, empty_t );
METADATA_TAG( MFgdFromSchemaCompletelySkipField, empty_t );
METADATA_TAG( MFgdHelper, const char * );
METADATA_TAG( MEntityAllowsPortraitWorldSpawn, empty_t );
METADATA_TAG( MVectorIsCoordinate, empty_t );
METADATA_TAG( MEnumeratorIsNotAFlag, empty_t );
METADATA_TAG( MEnumFlagsWithOverlappingBits, empty_t );
METADATA_TAG( MAtomicTransfersAsPlainString, empty_t );
METADATA_TAG( MAtomicTransfersAsMap, empty_t );
METADATA_TAG( MIsStringAndTokenType, empty_t );

METADATA_TAG( MObsoleteParticleFunction, empty_t );
METADATA_TAG( MClassIsParticleModel, empty_t );
METADATA_TAG( MClassIsParticleVec, empty_t );
METADATA_TAG( MClassIsParticleFloat, empty_t );
METADATA_TAG( MClassIsParticleTransform, empty_t );
METADATA_TAG( MParticleCustomFieldDefaultValue, FnParticleCustomFieldDefaultValue );
METADATA_TAG( MParticleRequireDefaultArrayEntry, empty_t );
METADATA_TAG( MParticleAdvancedField, empty_t );
METADATA_TAG( MParticleHelpField, empty_t );
METADATA_TAG( MParticleInputOptional, empty_t );
METADATA_TAG( MParticleReplacementOp, const char * );
METADATA_TAG( MParticleMinVersion, int );
METADATA_TAG( MParticleMaxVersion, int );
METADATA_TAG( MParticleDomainTag, const char * );

METADATA_TAG( M_LEGACY_OptInToSchemaPropertyDomain, empty_t );
