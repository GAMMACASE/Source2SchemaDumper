from enum import IntFlag
import os
import glob

class ArgsFlags(IntFlag):
	Empty = 0,
	AddComments = (1 << 0),
	SupplyingSDK = (1 << 1),
	StaticAsserts = (1 << 2),
	Silent = (1 << 3)

def parse_args_as_flags(args, base_flags = ArgsFlags.Empty):
	additive_flag_map = {
		"add_comments": ArgsFlags.AddComments,
		"supply_hl2sdk": ArgsFlags.SupplyingSDK,
		"static_assert": ArgsFlags.StaticAsserts,
		"silent": ArgsFlags.Silent
	}

	negative_flag_map = {
		"no_comments": ArgsFlags.AddComments
	}

	flags = base_flags
	for key, flag in additive_flag_map.items():
		if getattr(args, key, False):
			flags |= flag

	for key, flag in negative_flag_map.items():
		if getattr(args, key, False):
			flags &= ~flag

	return flags

def get_hl2sdk_common_types():
	return [
		'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64', 'float32', 'float64',
		'CUtlVector', 'VectorAligned', 'Vector', 'Vector2D', 'Vector4D', 'matrix3x4_t', 'matrix3x4a_t',
		'QAngle', 'Quaternion', 'Color', 'CUtlString', 'CUtlStringToken', 'CUtlMap', 'CUtlOrderedMap',
		'RadianEuler', 'ThreeState_t', 'CBufferString', 'CBufferStringN', 'CUtlHashtable', 'CUtlLinkedList',
		'KeyValues', 'KeyValues3', 'SolidType_t', 'RenderMode_t', 'RenderFx_t', 'MoveType_t', 'MoveCollide_t', 'LifeState_t',
		'CEntityIdentity', 'CEntityInstance', 'WorldGroupId_t', 'CEntityIndex', 'CPlayerSlot', 'HitGroup_t',
		'EntComponentInfo_t', 'CVariantDefaultAllocator', 'CVariant', 'CVariantBase', 'CUtlSymbolLarge', 'CTransform', 'HSCRIPT',
		'fieldtype_t', 'CScriptComponent', 'CEntityComponent', 'CPhysSurfacePropertiesSoundNames', 'CPhysSurfacePropertiesPhysics',
		'CPhysSurfacePropertiesAudio', 'CPhysSurfaceProperties', 'CHitBox', 'CEntityHandle', 'ChangeAccessorFieldPathIndex_t',
		'CEntityComponentHelper', 'soundlevel_t', 'SoundFlags_t', 'RenderMultisampleType_t', 'EngineLoopState_t', 'GameTime_t',
		'CUtlSymbol', 'EntityIOTargetType_t', 'EntityDormancyType_t', 'CSplitScreenSlot', 'CHandle', 'CSmartPtr',
		'CUtlLeanVectorFixedGrowable', 'CUtlLeanVector', 'CUtlVectorFixedGrowable', 'EventClientOutput_t', 'CUtlDict',
		'ENetworkDisconnectionReason', 'CBitVec', 'Flags_t', 'CPhysSurfacePropertiesVehicle'
	]

def locate_input_path(input_path):
	if not os.path.exists(input_path):
		raise Exception('Schema file path is invalid or not found')

	if os.path.isdir(input_path):
		files = glob.glob(os.path.join(input_path, '*.json'))
		if len(files) == 0:
			raise Exception(f'No schema files found in folder {input_path}')

		return max(files, key=os.path.getctime)
	
	return input_path

def prepare_out_path(out_path, filename):
	if not os.path.exists(out_path):
		os.makedirs(out_path)

	if os.path.isdir(out_path):
		return os.path.join(out_path, filename)
	
	return out_path

def align_value(value, alignment):
	if alignment <= 0:
		return value

	return ( ( value + alignment - 1 ) & ~( alignment - 1 ) )
