from enum import Enum
from typing import Union

from generator_scripts.common import align_value

alignment_overrides = {
	'EngineLoopState_t': 8
}

class SubTypeTypes(str, Enum):
	NONE = 'none'
	REF = 'ref'
	FIXED_ARRAY = 'fixed_array'
	ATOMIC = 'atomic'
	POINTER = 'ptr'
	BITFIELD = 'bitfield'
	LITERAL = 'literal'

	@classmethod
	def has_value(cls, value):
		return value in cls._value2member_map_ 

class ObjectTypes(str, Enum):
	NONE = 'none'
	CLASS = 'class'
	ENUM = 'enum'
	BUILTIN = 'builtin'

	@classmethod
	def has_value(cls, value):
		return value in cls._value2member_map_ 

class MetaTag:
	name = ''
	value = None

	def __init__(self):
		pass

	def has_value(self):
		return self.value is not None

	@staticmethod
	def parse_from(raw_tag):
		meta_tag = MetaTag()
		meta_tag.name = raw_tag['name']
		meta_tag.value = raw_tag.get('value', None)
		return meta_tag
	
	@staticmethod
	def parse_from_list(raw_tags):
		meta_tags = []
		for raw_tag in raw_tags:
			meta_tags.append(MetaTag.parse_from(raw_tag))
		return meta_tags

class EnumObjectField:
	name = ''
	value = 0
	traits: 'ObjectTraits' = None

	def __init__(self):
		pass

	def get_metadata(self):
		return self.traits.metatags or []

	@staticmethod
	def parse_from(raw_field):
		enum_field = EnumObjectField()
		enum_field.name = raw_field['name']
		enum_field.value = raw_field['value']
		enum_field.traits = ObjectTraits.parse_from(None, enum_field, raw_field.get('traits', {}))
		return enum_field
	
	@staticmethod
	def parse_from_list(raw_fields):
		enum_fields = []
		for raw_field in raw_fields:
			enum_fields.append(EnumObjectField.parse_from(raw_field))
		return enum_fields

class SubTypeRef:
	ref: 'ObjectDefinition' = None

	def __init__(self, ref = None):
		self.ref = ref

	def get_deps(self, needs_definition = True):
		return [(self, needs_definition)]
	
	def get_size(self):
		return self.ref.size

	def get_alignment(self):
		return self.ref.calc_alignment()

	def as_str(self, var_name = None):
		return f'{self.ref.name}{" " + var_name if var_name is not None else ""}'

	@property
	def type(self) -> SubTypeTypes:
		return SubTypeTypes.REF

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_ref):
		subtype_ref = SubTypeRef()
		subtype_ref.ref = obj_list.get_def_at(raw_ref['ref_idx'])
		return subtype_ref

class SubTypeFixedArray:
	subtype: 'SubType' = None
	count = 0
	element_size = 0

	def __init__(self):
		pass

	def get_deps(self, needs_definition = True):
		return self.subtype.get_deps(needs_definition)

	def get_size(self):
		return self.count * self.element_size

	def get_alignment(self):
		return self.subtype.get_alignment()

	def as_str(self, var_name = None):
		return f'{self.subtype.as_str(var_name)}[{self.count}]'

	@property
	def type(self) -> SubTypeTypes:
		return SubTypeTypes.FIXED_ARRAY

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_fixed_array):
		subtype_fixed_array = SubTypeFixedArray()
		subtype_fixed_array.subtype = parse_subtype(obj_list, raw_fixed_array['subtype'])
		subtype_fixed_array.count = raw_fixed_array['count']
		subtype_fixed_array.element_size = raw_fixed_array['element_size']
		return subtype_fixed_array

class AtomicTemplate:
	name = ''
	template_type = ''
	subtype: 'SubType' = None

	def __init__(self):
		pass

	def get_deps(self, needs_definition = True):
		return self.subtype.get_deps(needs_definition)

	def as_str(self, var_name = None):
		return self.subtype.as_str(var_name)

class AtomicTemplateList:
	template_args: list[AtomicTemplate] = None

	def __init__(self):
		self.template_args = []

	def __iter__(self):
		return iter(x.subtype for x in self.template_args)
	
	def __len__(self):
		return len(self.template_args)

	def __getitem__(self, key):
		return self.template_args[key]

	def get_template_decl(self):
		return ', '.join([f'{t.template_type} {t.name}' for t in self.template_args])

	def get_deps(self, needs_definition = True):
		deps = []
		for t in self.template_args:
			deps.extend(t.get_deps(needs_definition))
		return deps

	def as_str(self, var_name = None):
		return ', '.join([t.as_str(var_name) for t in self.template_args])

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_template_list):
		template_list = AtomicTemplateList()

		for i, template_arg in enumerate(raw_template_list):
			template = AtomicTemplate()
			template.name = chr(ord('A') + i)
			template.subtype = parse_subtype(obj_list, template_arg)
			template.template_type = 'size_t' if template.subtype.type == SubTypeTypes.LITERAL else 'typename'
			template_list.template_args.append(template)

		return template_list

class SubTypeAtomic:
	name = ''
	size = 0
	alignment = 0
	template_list: AtomicTemplateList | None = None

	def __init__(self):
		self.template_list = None

	def is_templated(self):
		return self.template_list is not None

	def get_template_decl(self):
		result = ''
		if self.is_templated():
			result = f'<{self.template_list.get_template_decl()}>'
		return result

	def get_deps(self, needs_definition = True):
		if not self.is_templated():
			return [(self, needs_definition)]

		deps = self.template_list.get_deps(False)
		deps.append((self, needs_definition))
		return deps

	def get_size(self):
		return self.size

	def get_alignment(self):
		return self.alignment

	def as_str(self, var_name = None):
		result = self.name

		if self.is_templated():
			result += '<' + self.template_list.as_str() + '>'

		return f'{result}{" " + var_name if var_name is not None else ""}'

	@property
	def type(self) -> SubTypeTypes:
		return SubTypeTypes.ATOMIC

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_atomic):
		subtype_atomic = SubTypeAtomic()
		subtype_atomic.name = raw_atomic['name'].split('<')[0]
		subtype_atomic.size = raw_atomic['size']
		subtype_atomic.alignment = raw_atomic['alignment']

		if 'template' in raw_atomic:
			subtype_atomic.template_list = AtomicTemplateList.parse_from(obj_list, raw_atomic['template'])
		
		return subtype_atomic

class SubTypePointer:
	subtype: 'SubType' = None

	def __init__(self):
		pass

	def get_deps(self, needs_definition = True):
		return self.subtype.get_deps(False)

	def get_size(self):
		return 8

	def get_alignment(self):
		return 8

	def as_str(self, var_name = None):
		return f'{self.subtype.as_str()}*{" " + var_name if var_name is not None else ""}'

	@property
	def type(self) -> SubTypeTypes:
		return SubTypeTypes.POINTER

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_pointer):
		subtype_pointer = SubTypePointer()
		subtype_pointer.subtype = parse_subtype(obj_list, raw_pointer['subtype'])
		return subtype_pointer
	
class SubTypeBitfield:
	bits_count = 0

	def __init__(self):
		pass

	def get_deps(self, needs_definition = True):
		return []

	def get_size(self):
		return int((self.bits_count + 7) / 8)

	def get_alignment(self):
		return 0

	def as_str(self, var_name = None):
		return f'int8{" " + var_name if var_name is not None else ""} : {self.bits_count}'

	@property
	def type(self):
		return SubTypeTypes.BITFIELD

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_bitfield):
		subtype_bitfield = SubTypeBitfield()
		subtype_bitfield.bits_count = raw_bitfield['count']
		return subtype_bitfield

class SubTypeLiteral:
	value = 0

	def __init__(self):
		pass

	def get_deps(self, needs_definition = True):
		return []

	def get_size(self):
		return 0
	
	def get_alignment(self):
		return 0

	def as_str(self, var_name = None):
		return f'{self.value}'

	@property
	def type(self) -> SubTypeTypes:
		return SubTypeTypes.LITERAL

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_literal):
		subtype_literal = SubTypeLiteral()
		subtype_literal.value = raw_literal['value']
		return subtype_literal

SubType = Union[SubTypeRef, SubTypeFixedArray, SubTypeAtomic, SubTypePointer, SubTypeBitfield, SubTypeLiteral]
def parse_subtype(obj_list: 'ObjectList', raw_subtype) -> SubType:
	match raw_subtype['type']:
		case SubTypeTypes.REF:
			return SubTypeRef.parse_from(obj_list, raw_subtype)
		case SubTypeTypes.FIXED_ARRAY:
			return SubTypeFixedArray.parse_from(obj_list, raw_subtype)
		case SubTypeTypes.ATOMIC:
			return SubTypeAtomic.parse_from(obj_list, raw_subtype)
		case SubTypeTypes.POINTER:
			return SubTypePointer.parse_from(obj_list, raw_subtype)
		case SubTypeTypes.BITFIELD:
			return SubTypeBitfield.parse_from(obj_list, raw_subtype)
		case SubTypeTypes.LITERAL:
			return SubTypeLiteral.parse_from(obj_list, raw_subtype)
		case _:
			raise Exception(f'Unknown subtype type: {type}')

class ClassObjectMember:
	name = ''
	offset = 0
	traits: 'ObjectTraits' = None

	def __init__(self):
		pass

	def get_type(self):
		return self.traits.subtype

	def get_metadata(self):
		return self.traits.metatags or []

	def has_flag(self, flag):
		return self.traits.has_flag(flag)

	@staticmethod
	def parse_from(obj_list: 'ObjectList', raw_member):
		class_member = ClassObjectMember()
		class_member.name = raw_member['name']
		class_member.offset = raw_member['offset']
		class_member.traits = ObjectTraits.parse_from(obj_list, class_member, raw_member.get('traits', {}))

		if class_member.traits.subtype is None:
			raise Exception(f'Class member {class_member.name} has no subtype')

		return class_member
	
	@staticmethod
	def parse_from_list(obj_list: 'ObjectList', raw_members):
		class_members = []
		for raw_member in raw_members:
			class_members.append(ClassObjectMember.parse_from(obj_list, raw_member))
		return class_members

class ObjectTraits:
	flags: list[str] | None = None
	fields: list[EnumObjectField] | None = None
	members: list[ClassObjectMember] | None = None
	metatags: list[MetaTag] | None = None
	subtype: SubType | None = None
	child_classes: list['ObjectDefinition'] | None = None
	parent_class: 'ObjectDefinition' = None
	multi_depth = None
	single_depth = None
	baseclasses: list[tuple['ObjectDefinition', int]] = None

	def __init__(self):
		pass

	def has_flag(self, flag):
		if self.flags is None:
			return False
		
		return flag in self.flags

	@staticmethod
	def parse_from(obj_list: 'ObjectList', root_obj, raw_traits):
		obj_traits = ObjectTraits()
		obj_traits.flags = raw_traits.get('flags', None)

		if 'metatags' in raw_traits:
			obj_traits.metatags = MetaTag.parse_from_list(raw_traits['metatags'])

		if 'fields' in raw_traits:
			obj_traits.fields = EnumObjectField.parse_from_list(raw_traits['fields'])
		
		if 'members' in raw_traits:
			obj_traits.members = ClassObjectMember.parse_from_list(obj_list, raw_traits['members'])

		if 'subtype' in raw_traits:
			obj_traits.subtype = parse_subtype(obj_list, raw_traits['subtype'])

		if 'child_class_idx' in raw_traits:
			obj_traits.child_classes = []
			for child_class_idx in raw_traits['child_class_idx']:
				obj_traits.child_classes.append(obj_list.get_def_at(child_class_idx))

		if 'parent_class_idx' in raw_traits:
			if raw_traits['parent_class_idx'] != -1:
				obj_traits.parent_class = obj_list.get_def_at(raw_traits['parent_class_idx'])
			else:
				# Special case for parent classes that are not present in schema, generate a synthetic parent class to match our needs
				obj_traits.parent_class = obj_list.get_def_at_name_or_synth(root_obj.name.split('::')[-2])
				if obj_traits.parent_class.is_synthetic():
					if obj_traits.parent_class.traits is None:
						obj_traits.parent_class.traits = ObjectTraits()
						obj_traits.parent_class.traits.child_classes = [ root_obj ]
					else:
						obj_traits.parent_class.traits.child_classes.append(root_obj)

		if 'multi_depth' in raw_traits:
			obj_traits.multi_depth = raw_traits['multi_depth']

		if 'single_depth' in raw_traits:
			obj_traits.single_depth = raw_traits['single_depth']

		if 'baseclasses' in raw_traits:
			obj_traits.baseclasses = []
			for baseclass in raw_traits['baseclasses']:
				obj_traits.baseclasses.append((obj_list.get_def_at(baseclass['ref_idx']), baseclass['offset']))
		return obj_traits

class ObjectDefinition:
	name = ''
	scope = ''
	project = ''
	type: ObjectTypes = ObjectTypes.NONE
	size = 0
	alignment = 0
	calculated_alignment = -1
	packed_alignment = -1
	traits: ObjectTraits = None
	synthetic = False

	def __init__(self):
		pass
	
	def is_synthetic(self):
		return self.synthetic

	def is_parent_scope_def(self):
		return '::' not in self.name

	def get_scoped_name(self):
		return self.name.split('::')[-1]

	def get_parent_scope_name(self):
		return self.name.split('::')[-2]

	def get_parent_scope_def(self):
		if self.traits is None:
			return None
		
		return self.traits.parent_class

	def get_packed_alignment(self):
		return self.packed_alignment

	def has_flag(self, flag):
		if self.traits is None:
			return False

		return self.traits.has_flag(flag)

	def calc_alignment(self):
		global alignment_overrides
		if self.get_packed_alignment() != -1:
			return -1

		if self.is_synthetic():
			return self.alignment

		if self.alignment != 255:
			return self.alignment
		
		if self.calculated_alignment != -1:
			return self.calculated_alignment

		if self.name in alignment_overrides:
			return alignment_overrides[self.name]

		parent_alignment = self.calc_parent_alignment()
		has_virtuals = self.has_flag('has_virtual_members')

		class_alignment = 8 if has_virtuals and parent_alignment < 8 else parent_alignment

		for member in self.get_members():
			member_alignment = member.get_type().get_alignment()
			if member_alignment > class_alignment and member_alignment != 255:
				class_alignment = member_alignment
		
		self.calculated_alignment = class_alignment

		if align_value(self.size, class_alignment) != self.size:
			while class_alignment > 1:
				class_alignment = int(class_alignment / 2)
				if align_value(self.size, class_alignment) == self.size:
					break
			self.packed_alignment = class_alignment

			return -1

		return self.calculated_alignment

	def calc_parent_alignment(self):
		parent_alignment = 1
		for (baseclass, offset) in self.get_baseclasses():
			if baseclass.calc_alignment() > parent_alignment:
				parent_alignment = baseclass.calc_alignment()
		return parent_alignment

	# Returns true if enum is considered a bitfield
	def is_bitfield(self):
		if self.type != ObjectTypes.ENUM:
			return False

		is_bitfield = True
		max_reached = 0
		for field in self.get_fields():
			max_reached = field.value

			if field.value <= 2:
				continue
			
			if field.value.bit_count() >= 2:
				is_bitfield = False
				return
			
			break

		# don't treat small enums as bitfields
		return is_bitfield and max_reached > 2

	# Will result in at infinite loop if there is a circular dependency
	def resolve_child_scope_order(self, child_scopes):
		for i, child_scope in enumerate(child_scopes):
			for k, child_scope2 in enumerate(child_scopes):
				if i == k:
					break

				if child_scope.name in [x.as_str() for [x, _] in child_scope2.get_member_deps()]:
					child_scopes[k], child_scopes[i] = child_scopes[i], child_scopes[k]
					return True
		return False

	def get_child_scopes(self):
		if self.traits is None or self.traits.child_classes is None:
			return []
		
		ordered_list = self.traits.child_classes.copy()
		counter = 0
		while self.resolve_child_scope_order(ordered_list):
			counter += 1
			if counter > 100:
				raise Exception('Failed to resolve child scope order, max num of iteration reached')

		return ordered_list
	
	def get_member_deps(self) -> list[SubType]:
		if self.type != ObjectTypes.CLASS:
			return []

		deps = []
		for member in self.get_members():
			deps.extend(member.get_type().get_deps())
		return deps

	def get_baseclasses(self):
		if self.type != ObjectTypes.CLASS:
			raise Exception(f'Object definition is not a class: {self.name}')
		return self.traits.baseclasses or []
	
	def get_members(self):
		if self.type != ObjectTypes.CLASS:
			raise Exception(f'Object definition is not a class: {self.name}')
		return self.traits.members or []
	
	def get_fields(self):
		if self.type != ObjectTypes.ENUM:
			raise Exception(f'Object definition is not an enum: {self.name}')
		return self.traits.fields or []

	def get_metadata(self):
		return self.traits.metatags or []

	@staticmethod
	def generate_synthetic_class(name):
		obj_def = ObjectDefinition()
		obj_def.name = name
		obj_def.type = ObjectTypes.CLASS
		obj_def.size = -1
		obj_def.alignment = -1
		obj_def.traits = None
		obj_def.synthetic = True
		return obj_def

	@staticmethod
	def parse_from(raw_def):
		obj_def = ObjectDefinition()
		obj_def.name = raw_def['name']
		obj_def.scope = raw_def.get('scope', '')
		obj_def.project = raw_def.get('project', '')

		if not ObjectTypes.has_value(raw_def['type']):
			raise Exception(f'Unknown object type: {raw_def["type"]}')

		obj_def.type = raw_def['type']
		obj_def.size = raw_def['size']
		obj_def.alignment = raw_def['alignment']

		# Don't initialize traits right away since it needs to
		# reference other object definitions
		obj_def.traits = None
		return obj_def
	
class ObjectList:
	defs: list[ObjectDefinition] = None
	synth_defs: list[ObjectDefinition] = None

	def __init__(self):
		self.defs = []
		self.synth_defs = []

	def get_def_at_name_or_synth(self, name):
		for defn in self.defs:
			if defn.name == name:
				return defn
		for defn in self.synth_defs:
			if defn.name == name:
				return defn
			
		self.synth_defs.append(ObjectDefinition.generate_synthetic_class(name))
		return self.synth_defs[-1]

	def get_def_at_name(self, name):
		for defn in self.defs:
			if defn.name == name:
				return defn
		return None
	
	def get_def_at(self, idx) -> ObjectDefinition:
		if idx < 0 or idx >= len(self.defs):
			raise IndexError(f'Index out of range: {idx}')
		return self.defs[idx]

	def get_def_idx(self, obj: ObjectDefinition):
		return self.defs.index(obj)

	@staticmethod
	def parse_from(raw_defs):
		obj_list = ObjectList()
		for raw_def in raw_defs:
			obj_list.defs.append(ObjectDefinition.parse_from(raw_def))

		# Do second pass to parse traits now that we have all object definitions parsed
		for raw_def, obj_def in zip(raw_defs, obj_list.defs):
			obj_def.traits = ObjectTraits.parse_from(obj_list, obj_def, raw_def.get('traits', {}))
		
		return obj_list

	def __iter__(self):
		return iter(self.defs)
	
	def __len__(self):
		return len(self.defs)

	def __getitem__(self, key):
		return self.defs[key]