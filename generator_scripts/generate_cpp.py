from enum import Enum
import os
import re
from generator_scripts.common import Helpers, ArgsFlags, align_value, get_hl2sdk_common_types, get_std_common_types, locate_input_path, parse_args_as_flags, prepare_out_path
from generator_scripts.obj_defs import ClassObjectMember, EnumObjectField, ObjectDefinition, ObjectTypes, SubType, SubTypeAtomic, SubTypeTypes
from generator_scripts.schema_file import SchemaFile, FileWriter
import argparse

args = None

class CppWriter(FileWriter):
	class_indent = 0
	class_access_specifier = 'private'
	enum_indent = 0

	def __init__(self, stream, flags = ArgsFlags.Empty):
		super().__init__(stream, flags)
	
	def class_declaration(self, class_obj: ObjectDefinition):
		return self.class_entry_start_str(class_obj.name, class_obj.calc_alignment()).write(';').newl()

	def class_definition(self, class_obj: ObjectDefinition):
		current_offset = 0
		for (baseclass, offset) in class_obj.get_baseclasses():
			current_offset += align_value(baseclass.size, baseclass.calc_parent_alignment())
		
		has_virtuals = class_obj.has_flag('has_virtual_members')
		class_alignment = class_obj.calc_alignment()

		self.class_entry_start(class_obj)

		# Define all child classes first as they might be used as member types of this class
		if len(class_obj.get_child_scopes()) > 0:
			self.set_class_member_access_specifier('public')
			for child_scope in class_obj.get_child_scopes():
				match child_scope.type:
					case ObjectTypes.ENUM:
						self.enum_definition(child_scope).newl()
					case ObjectTypes.CLASS:
						self.class_definition(child_scope).newl()

		# Create vtable member if we have a vtable defined for this class
		if has_virtuals and current_offset == 0:
			self.set_class_member_access_specifier('private')
			self.write_il('void* __vftable;')
			current_offset += 8

		bitfield_accum = 0
		bitfield_size = 1
		bitfield_last_member = 0
		previous_member_alignment = 0

		for i, member in enumerate(class_obj.get_members()):
			member_size = member.get_type().get_size()
			member_alignment = member.get_type().get_alignment()

			# Bitfields don't have any offset set
			if member.get_type().type == SubTypeTypes.BITFIELD and member.offset == 0:
				# If it's a start of a bitfield sequence, find largest alignment needed for it
				if bitfield_accum == 0:
					for j in range(i, len(class_obj.get_members())):
						adv_member = class_obj.get_members()[j]
						if adv_member.get_type().type == SubTypeTypes.BITFIELD and adv_member.offset == 0:
							size_needed = int((adv_member.get_type().bits_count + 7) / 8)

							if size_needed > bitfield_size:
								bitfield_size = size_needed
						else:
							bitfield_last_member = j
							break
					for j in range(i, bitfield_last_member):
						class_obj.get_members()[j].get_type().expected_size = bitfield_size

				bitfield_accum += member.get_type().bits_count
			else:
				if bitfield_accum != 0:
					current_offset += int((bitfield_accum + 7) / 8)
					bitfield_size = 1
					bitfield_last_member = 0
					bitfield_accum = 0

				# Prevent unnecessary padding caused by alignment but ignore any packed classes
				if previous_member_alignment != 0 and member_alignment > previous_member_alignment and class_obj.get_packed_alignment() == -1:
					current_offset = align_value(current_offset, member_alignment)

				if current_offset != member.offset:
					if member.offset - current_offset < 0:
						self.write_comment_check(f'INVALID OFFSET: {member.offset}, {current_offset}, {previous_member_alignment}, {member_alignment}')
					self.class_member_pad(current_offset, member.offset - current_offset)

				current_offset = member.offset + member_size

			previous_member_alignment = member_alignment
			self.class_member_entry(member)

		if bitfield_accum != 0:
			current_offset += int((bitfield_accum + 7) / 8)
			bitfield_size = 1
			bitfield_last_member = 0
			bitfield_accum = 0

		if align_value(current_offset, class_alignment) < class_obj.size:
			self.class_member_pad(current_offset, class_obj.size - align_value(current_offset, class_alignment))

		self.class_entry_end(class_obj)
		
		# Declare usage for child scopes, otherwise these definitions would be omitted by idaclang for example
		for child_scope in class_obj.get_child_scopes():
			self.newl().write_il(f'{child_scope.name} {child_scope.name.replace("::", "__")};')

		self.newl().class_static_asserts(class_obj)
		
		return self

	def class_static_asserts(self, class_obj: ObjectDefinition):
		if self.has_flag(ArgsFlags.StaticAsserts) and not class_obj.is_synthetic():
			if align_value(class_obj.size, class_obj.calc_alignment()) != class_obj.size:
				self.write_il(f'static_assert(false, "Invalid alignment calculated ({class_obj.calc_alignment()}) doesn\'t match the size ({class_obj.size} vs {align_value(class_obj.size, class_obj.calc_alignment())})");')

			if class_obj.size > 0:
				self.write_il(f'static_assert(sizeof({class_obj.get_scoped_name()}) == {class_obj.size});')
			
			if class_obj.alignment != 255:
				self.write_il(f'static_assert({class_obj.alignment} == {class_obj.calc_alignment()}, "Inconsistent alignment calculated!");')
			for member in class_obj.get_members():
				if member.get_type().type != SubTypeTypes.BITFIELD:
					self.write_il(f'static_assert(offsetof({class_obj.get_scoped_name()}, {member.name}) == {member.offset});')

			self.newl()
		
		return self

	def class_entry_start_str(self, name, alignment):
		self.write_indent()
		self.write('class ')
		self.write(f"alignas({alignment}) " if alignment != 255 and alignment != -1 else "")
		self.write(f'{name}')

		return self

	def class_entry_block_start(self):
		self.class_indent += 1
		self.class_access_specifier = 'private'

		self.write_il('{').indent()

		return self

	def class_entry_block_end(self):
		if self.class_indent <= 0:
			raise Exception('Cannot write class entry end outside of class')

		self.class_indent -= 1
		self.class_access_specifier = ''

		return self.dedent().write_il('};')

	def class_entry_start(self, class_obj: ObjectDefinition):
		for metatag in class_obj.get_metadata():
			self.write_comment_check(f'{metatag.name}' + (' = ' + metatag.value if metatag.has_value() else ''))

		if not class_obj.is_synthetic():
			self.write_comment_check(f'Size: 0x{class_obj.size:X} ({class_obj.size})')
			self.write_comment_check(f'Alignment: {class_obj.alignment}')

			if class_obj.alignment == 255:
				self.write_comment_check(f'Calculated alignment: {class_obj.calc_alignment()}')
		else:
			self.write_comment_check(f'Synthetically created class, no size/alignment available')

		if class_obj.get_packed_alignment() != -1:
			self.write_il(f'#pragma pack(push, {class_obj.get_packed_alignment()})')

		self.class_entry_start_str(class_obj.get_scoped_name(), class_obj.calc_alignment())
		if len(class_obj.get_baseclasses()) > 0:
			self.write(" : public " + ", public ".join([x[0].name for x in class_obj.get_baseclasses()]))

		return self.newl().class_entry_block_start()

	def class_entry_end(self, class_obj: ObjectDefinition):
		self.class_entry_block_end()

		if class_obj.get_packed_alignment() != -1:
			self.write_il('#pragma pack(pop)')

		return self

	def class_member_pad(self, offset, size):
		if self.class_indent <= 0:
			raise Exception('Cannot write member pad outside of class')

		self.set_class_member_access_specifier('private')

		return self.write_il(f'uint8 pad_{offset:04X}[{size}];')

	def class_member_entry(self, member: ClassObjectMember):
		if self.class_indent <= 0:
			raise Exception('Cannot write member entry outside of class')

		self.set_class_member_access_specifier('public')

		for metatag in member.get_metadata():
			self.write_comment_check(f'{metatag.name}' + (' = ' + metatag.value if metatag.has_value() else ''))

		self.write_indent()
		self.write(f'{member.get_type().as_str(member.name)};')

		if self.has_flag(ArgsFlags.AddComments):
			self.write(f' // 0x{member.offset:X} ({member.offset}) (alignment: {member.get_type().get_alignment()})')

		return self.newl()

	def set_class_member_access_specifier(self, spec):
		if self.class_access_specifier != spec:
			self.class_access_specifier = spec
			self.class_member_access_specifier(spec)

	def class_member_access_specifier(self, spec):
		if self.class_indent <= 0:
			raise Exception('Cannot write member access specifier outside of class')

		return self.write_il(f'{spec}:', self.indent_level - 1)

	def atomic_declaration(self, atomic: SubTypeAtomic):
		if atomic.is_templated():
			self.write_il(f'template {atomic.get_template_decl()}')

		return self.class_entry_start_str(atomic.name, -1).write(';').newl().newl()

	def atomic_definition(self, atomic: SubTypeAtomic):
		if atomic.is_templated():
			self.write_il('template<>')

		self.class_entry_start_str(atomic.as_str(), atomic.alignment).newl()
		self.class_entry_block_start()
		self.class_member_pad(0, atomic.size)
		self.class_entry_block_end().newl()

		return self

	def enum_declaration(self, enum_obj: ObjectDefinition):
		is_signed = min(map(lambda x: x.value, enum_obj.get_fields())) < 0
		return self.write_il(f'enum class {enum_obj.name} : {Helpers.size_to_int(enum_obj.size, is_signed)};').newl().newl()

	def enum_definition(self, enum_obj: ObjectDefinition):
		self.enum_entry_start(enum_obj)

		for field in enum_obj.get_fields():
			self.enum_member_entry(field, enum_obj.is_bitfield())

		self.enum_entry_end()
		self.newl().enum_static_asserts(enum_obj)

		return self

	def enum_static_asserts(self, enum_obj: ObjectDefinition):
		if self.has_flag(ArgsFlags.StaticAsserts):
			for field in enum_obj.get_fields():
				self.write_il(f'static_assert({enum_obj.get_scoped_name()}::{field.name} == ({enum_obj.get_scoped_name()}){field.value});')

			is_signed = min(map(lambda x: x.value, enum_obj.get_fields())) < 0
			self.write_il(f'static_assert(sizeof({enum_obj.get_scoped_name()}) == sizeof({Helpers.size_to_int(enum_obj.size, is_signed)}));')
			self.newl()
		
		return self

	def enum_entry_start(self, enum_obj: ObjectDefinition):
		self.enum_indent += 1

		for metatag in enum_obj.get_metadata():
			self.write_comment_check(f'{metatag.name}' + (' = ' + metatag.value if metatag.has_value() else ''))

		is_signed = min(map(lambda x: x.value, enum_obj.get_fields())) < 0
		self.write_il(f'enum class {enum_obj.get_scoped_name()} : {Helpers.size_to_int(enum_obj.size, is_signed)}')
		self.write_il('{').indent()

		return self

	def enum_entry_end(self):
		if self.enum_indent <= 0:
			raise Exception('Cannot write enum entry end outside of enum')

		self.enum_indent -= 1
		return self.dedent().write_il('};')
	
	def enum_member_entry(self, enum_field: EnumObjectField, is_bitfield = False):
		if self.enum_indent <= 0:
			raise Exception('Cannot write enum member entry outside of enum')

		for metatag in enum_field.get_metadata():
			self.write_comment_check(f'{metatag.name}' + (' = ' + metatag.value if metatag.has_value() else ''))

		return self.write_il(f'{enum_field.name} = {enum_field.value},' + (f' // (1 << {enum_field.value.bit_length() - 1})' if is_bitfield and enum_field.value > 0 else ''))

class CppContext:
	writer: CppWriter = None
	defined_atomic_sizes = None
	processed = None
	status = None
	overrides = None
	flags: ArgsFlags = ArgsFlags.Empty

	class ObjectStatus(Enum):
		NONE = 0
		DECLARED = 1
		DEFINED = 2

	def __init__(self, writer, flags = ArgsFlags.Empty):
		self.writer = writer
		self.defined_atomic_sizes = dict()
		self.processed = dict()
		self.status = dict()
		self.overrides = []
		self.flags = flags

	def has_flag(self, flag):
		return flag in self.flags

	def is_declared(self, name):
		return name in self.status and (self.status[name] == self.ObjectStatus.DECLARED or self.status[name] == self.ObjectStatus.DEFINED)

	def is_defined(self, name):
		return name in self.status and self.status[name] == self.ObjectStatus.DEFINED

	def has_override(self, name):
		return name in self.overrides

	def add_overrides(self, names):
		self.overrides.extend(names)

	def declare_object(self, object: ObjectDefinition):
		if self.is_declared(object.name) or self.has_override(object.name):
			return

		# We can't really declared non parent scopes
		if not object.is_parent_scope_def():
			self.process_object(object.get_parent_scope_def())
			return

		match object.type:
			case ObjectTypes.ENUM:
				self.writer.enum_declaration(object)

			case ObjectTypes.CLASS:
				self.writer.class_declaration(object)

		self.status[object.name] = self.ObjectStatus.DECLARED

	def declare_atomic(self, atomic: SubTypeAtomic):
		if self.is_declared(atomic.as_str()) or self.has_override(atomic.name):
			return

		self.writer.atomic_declaration(atomic)

		self.status[atomic.as_str()] = self.ObjectStatus.DECLARED

	def define_atomic(self, atomic: SubTypeAtomic):
		# Very ugly hack to make CUtlHashtable work with hl2sdk
		if self.has_flag(ArgsFlags.SupplyingSDK):
			if atomic.name == 'CUtlHashtable':
				t1 = atomic.template_list[0]
				is_enum = t1.subtype.type == SubTypeTypes.REF and t1.subtype.ref.type == ObjectTypes.ENUM

				if not self.has_override(t1.as_str()) or is_enum:
					self.writer.write_il(f'template <> struct DefaultHashFunctor<{t1.as_str()}> : Mix32HashFunctor ' + '{};').newl()

		# For some reason there are atomics with the same traits (e.g. tamplate args) but with a different size & alignment
		# not sure the direct cause or if it's a bug on valve schema side, but currently that's only observerd in dota binaries for a class CDOTA_UnitInventory
		# and nowhere else, so we safeguard such cases in here wich should be sufficient if ever that pops up in different place
		if self.is_defined(atomic.as_str()) and self.defined_atomic_sizes[atomic.as_str()] != atomic.size:
			old_name = atomic.as_str()
			m = re.search(r'(\d+)$', atomic.name, flags = re.MULTILINE)
			if m:
				atomic.name = atomic.name[:-len(m.group(0))] + str(int(m.group(0)) + 1)
			else:
				atomic.name = atomic.name + '0'
			
			print_stdout(f'Found invalid sized atomic: {old_name} (new: {atomic.size}, defined: {self.defined_atomic_sizes[old_name]}), redefining as: {atomic.as_str()}')
			self.define_atomic(atomic)
			return

		if self.is_defined(atomic.as_str()) or self.has_override(atomic.name):
			return

		if not self.is_declared(atomic.name) and atomic.is_templated():
			self.writer.atomic_declaration(atomic)
			self.status[atomic.as_str()] = self.ObjectStatus.DECLARED

		self.writer.atomic_definition(atomic)

		self.defined_atomic_sizes[atomic.as_str()] = atomic.size
		self.status[atomic.as_str()] = self.ObjectStatus.DEFINED

	def define_object(self, object: ObjectDefinition):
		# Do static_asserts even if we have type defined by hl2sdk to catch any changes in them
		if self.has_override(object.name):
			match object.type:
				case ObjectTypes.ENUM:
					# Prevent ENetworkDisconnectionReason from here as we won't be having a correct define for it
					if object.name != 'ENetworkDisconnectionReason':
						self.writer.enum_static_asserts(object)
				case ObjectTypes.CLASS:
					self.writer.class_static_asserts(object)
			return

		if self.is_defined(object.name):
			return
		
		# Parent scope will handle our definition
		if not object.is_parent_scope_def():
			self.process_object(object.get_parent_scope_def())
			return

		match object.type:
			case ObjectTypes.ENUM:
				self.writer.enum_definition(object)
			case ObjectTypes.CLASS:
				self.writer.class_definition(object)

		self.status[object.name] = self.ObjectStatus.DEFINED

	def process_subtype(self, subtype: SubType, needs_definition):
		match subtype.type:
			case SubTypeTypes.REF:
				if not needs_definition:
					self.declare_object(subtype.ref)
				else:
					self.process_object(subtype.ref)
			case SubTypeTypes.ATOMIC:
				self.processed[subtype.as_str()] = True

				if needs_definition:
					self.define_atomic(subtype)
				else:
					self.declare_atomic(subtype)
			case _:
				raise Exception(f'Unknown subtype type: {subtype.type}')

	def process_class(self, class_obj: ObjectDefinition):
		for (baseclass, offset) in class_obj.get_baseclasses():
			self.process_object(baseclass)

		for child_scope in class_obj.get_child_scopes():
			self.process_object(child_scope)

		for (dep, needs_definition) in class_obj.get_member_deps():

			if self.has_flag(ArgsFlags.SupplyingSDK):
				# Ugly hack to make CUtlLeanVector work with hl2sdk
				if dep.type == 'atomic' and (dep.name.startswith('CUtlLeanVector') or dep.name.startswith('CUtlVectorFixedGrowable')):
					for (subdep, subneeds_def) in dep.get_deps():
						self.process_subtype(subdep, True)
				
				# Force define second argument of the hashtable as it expects it to be defined
				if dep.type == 'atomic' and dep.name.startswith('CUtlHashtable'):
					self.process_subtype(dep.template_list[1].subtype, True)

			self.process_subtype(dep, needs_definition)

		self.define_object(class_obj)

	def process_enum(self, enum_obj: ObjectDefinition):
		self.define_object(enum_obj)

	def process_object(self, object: ObjectDefinition):
		if object.name in self.processed:
			return

		self.processed[object.name] = True

		match object.type:
			case ObjectTypes.ENUM:
				self.process_enum(object)

			case ObjectTypes.CLASS:
				self.process_class(object)

def print_stdout(text):
	global args

	if not args.silent:
		print(text)

def main():
	global args

	parser = argparse.ArgumentParser(
		description = "Generates C++ source2 schema definitions out of dumped schema data.",
		usage = "%(prog)s [options]"
	)

	parser.add_argument('-i', '--input', help = 'The path to the JSON schema file or a folder containing schema files, in which case the newest file would be processed. Default is ./dumps/ dir.', dest = 'schema_path', type = str, default = './dumps/')
	parser.add_argument('-o', '--output', help = 'The path to the output C++ file or a directory. Default is ./generated/ dir.', type = str, dest = 'out_path', default = "./generated/")
	parser.add_argument('-s', '--silent', help = 'Disables stdout output.', action = 'store_true', dest = 'silent')
	parser.add_argument('-c', '--comments', help = 'Generate help comments for resulting class/enum definitions.', action = 'store_true', dest = 'add_comments')
	parser.add_argument('-g', '--generate-classes', help = 'A list of class/enum definitions to generate. Use "all" to generate all classes (Default).', nargs = '+', type = str, dest = 'generate_classes', default = 'all')
	parser.add_argument('-a', '--static-assert', help = 'Generate static assertions of resulting class/enum definitions to ensure their validity.', action = 'store_true', dest = 'static_assert')
	parser.add_argument('-d', '--supply-hl2sdk', help = 'Supplies hl2sdk class/enum definitions if applicable to the generated file.', action = 'store_true', dest = 'supply_hl2sdk')

	args = parser.parse_args()
	flags = parse_args_as_flags(args)

	args.schema_path = locate_input_path(args.schema_path)
	schema_file = SchemaFile(args.schema_path)

	args.out_path = prepare_out_path(args.out_path, os.path.basename(args.schema_path).replace('.json', '.h'))

	if not args.supply_hl2sdk and 'no_parent_scope' not in schema_file.get_flags():
		print_stdout('!!! Schema dump was dumped with parent scope, which might not generate correct code without supplying hl2sdk definitions.\n!!! Please either generate with hl2sdk defs (--supply-hl2sdk) or dump shema without parent scope.')

	total_generated = 0
	with CppWriter(open(args.out_path, 'w'), flags) as writer:
		context = CppContext(writer, flags)

		context.add_overrides(get_std_common_types())

		writer.write_il('#include "stdint.h"')
		# For std::pair usage
		writer.write_il('#include <utility>')

		if args.static_assert:
			writer.write_il('#include "stddef.h"')
		
		writer.newl()

		if args.supply_hl2sdk:
			writer.write_il('// Hack fix for proto defines as they result in an idaclang error!')
			writer.write_il('#define GOOGLE_PROTOBUF_INCLUDED_network_5fconnection_2eproto')
			writer.write_il('typedef int ENetworkDisconnectionReason;')

			writer.newl()

			writer.write_il('#include "platform.h"')
			writer.write_il('#include "eiface.h"')
			writer.write_il('#include "iserver.h"')
			writer.write_il('#include "inetchannel.h"')
			writer.write_il('#include "iloopmode.h"')
			writer.write_il('#include "interfaces/interfaces.h"')

			writer.newl()

			# Small hack to make hl2sdk defined structs not blame private fields in static_asserts
			if args.static_assert:
				writer.write_il('#define private public')
				writer.newl()

			writer.write_il('#include "const.h"')
			writer.write_il('#include "vector.h"')
			writer.write_il('#include "vector2d.h"')
			writer.write_il('#include "vector4d.h"')
			writer.write_il('#include "color.h"')
			writer.write_il('#include "variant.h"')
			writer.write_il('#include "bufferstring.h"')
			writer.write_il('#include "keyvalues.h"')
			writer.write_il('#include "keyvalues3.h"')
			writer.write_il('#include "transform.h"')
			writer.write_il('#include "igameevents.h"')
			writer.write_il('#include "smartptr.h"')
			writer.write_il('#include "gametrace.h"')
			writer.write_il('#include "playerslot.h"')
			writer.write_il('#include "datamap.h"')
			writer.write_il('#include "soundflags.h"')
			writer.write_il('#include "convar.h"')
			writer.write_il('#include "ehandle.h"')

			writer.newl()

			writer.write_il('#include "entity2/entityidentity.h"')
			writer.write_il('#include "entity2/entitysystem.h"')
			writer.write_il('#include "entity2/entityinstance.h"')
			writer.write_il('#include "entity2/entitykeyvalues.h"')
			writer.write_il('#include "entity2/entityclass.h"')
			writer.write_il('#include "entityhandle.h"')

			writer.newl()

			writer.write_il('#include "schemasystem/schemasystem.h"')
			writer.write_il('#include "schemasystem/schematypes.h"')

			writer.newl()

			writer.write_il('#include "networksystem/inetworkserializer.h"')
			writer.write_il('#include "networksystem/inetworkmessages.h"')
			writer.write_il('#include "networksystem/netmessage.h"')
			writer.write_il('#include "networksystem/iflattenedserializers.h"')
			writer.write_il('#include "networksystem/inetworksystem.h"')
			writer.write_il('#include "networksystem/iprotobufbinding.h"')

			writer.newl()

			writer.write_il('#include "memstack.h"')
			writer.write_il('#include "utlsymbol.h"')
			writer.write_il('#include "utlsymbollarge.h"')
			writer.write_il('#include "utlscratchmemory.h"')
			writer.write_il('#include "utlleanvector.h"')
			writer.write_il('#include "utldict.h"')
			writer.write_il('#include "bitbuf.h"')
			writer.write_il('#include "circularbuffer.h"')
			writer.write_il('#include "bufferstring.h"')
			writer.write_il('#include "utlhash.h"')
			writer.write_il('#include "utlhashdict.h"')
			writer.write_il('#include "utlhashtable.h"')
			writer.write_il('#include "utlmap.h"')
			writer.write_il('#include "utlstring.h"')
			writer.write_il('#include "utllinkedlist.h"')
			writer.write_il('#include "utlvector.h"')
			writer.write_il('#include "utltshash.h"')

			if args.static_assert:
				writer.newl()
				writer.write_il('#undef private')

			context.add_overrides(get_hl2sdk_common_types())
		else:
			writer.write_il('typedef int8_t int8;')
			writer.write_il('typedef uint8_t uint8;')
			writer.write_il('typedef int16_t int16;')
			writer.write_il('typedef uint16_t uint16;')
			writer.write_il('typedef int32_t int32;')
			writer.write_il('typedef uint32_t uint32;')
			writer.write_il('typedef int64_t int64;')
			writer.write_il('typedef uint64_t uint64;')
			writer.write_il('typedef float float32;')
			writer.write_il('typedef double float64;')
		
		writer.newl()

		if 'all' in args.generate_classes:
			print_stdout('Generating all class definitions...')

			for defn in schema_file.defs:
				total_generated += 1
				context.process_object(defn)
		else:
			print_stdout(f'Generating ({", ".join(args.generate_classes)}) class definitions...')
			
			for class_name in args.generate_classes:
				defn = schema_file.defs.get_def_at_name(class_name)

				if defn == None:
					print_stdout(f'Class definition ({class_name}) not found in schema file, failed to generate!')
					continue

				total_generated += 1
				context.process_object(defn)
	
	print_stdout(f'Successfully generated C++ file at {os.path.abspath(args.out_path)} (Total objects processed: {total_generated})')

if __name__ == '__main__':
	main()