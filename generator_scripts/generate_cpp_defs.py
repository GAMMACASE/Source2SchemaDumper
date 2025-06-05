from enum import Enum
import os
from generate_cpp import CppContext, CppWriter
from generator_scripts.common import ArgsFlags, locate_input_path, parse_args_as_flags, prepare_out_path, get_hl2sdk_common_types
from generator_scripts.obj_defs import ClassObjectMember, EnumObjectField, ObjectDefinition, ObjectTypes, SubType, SubTypeAtomic, SubTypeTypes
from generator_scripts.schema_file import Helpers, SchemaFile
import argparse

args = None

class CppDefsWriter(CppWriter):
	def __init__(self, stream, flags = ArgsFlags.Empty):
		super().__init__(stream, flags)

	def class_declaration(self, class_obj: ObjectDefinition):
		return self

	def class_definition(self, class_obj: ObjectDefinition):
		self.class_entry_start(class_obj)
		self.write_il(f'DECLARE_SCHEMA_CLASS({class_obj.name})').newl()

		for member in class_obj.get_members():
			self.class_member_entry(member)

		self.class_entry_end()
		
		return self.newl()

	def class_static_asserts(self, class_obj: ObjectDefinition):
		return self

	def class_entry_start_str(self, name, alignment):
		return self.write_indent().write(f'class {name}')

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
		self.class_entry_start_str(class_obj.get_scoped_name(), class_obj.alignment)
		if len(class_obj.get_baseclasses()) > 0:
			self.write(" : public " + ", public ".join([x[0].name for x in class_obj.get_baseclasses()]))

		return self.newl().class_entry_block_start()

	def class_entry_end(self, class_obj: ObjectDefinition = None):
		return self.class_entry_block_end()

	def class_member_pad(self, offset, size):
		return self

	def class_member_entry(self, member: ClassObjectMember):
		if self.class_indent <= 0:
			raise Exception('Cannot write member entry outside of class')

		self.write_indent()

		match member.get_type().type:
			case SubTypeTypes.FIXED_ARRAY:
				# Get the actual array type here
				subtype = member.get_type().subtype
				while subtype.type == SubTypeTypes.FIXED_ARRAY:
					subtype = subtype.subtype

				self.write(f'SCHEMA_FIELD_POINTER({subtype.as_str()}, {member.name})')
			case SubTypeTypes.BITFIELD:
				print_stdout(f'Found bitfield member ({member.name}), which is unsupported, skipping...')
			case _:
				self.write(f'SCHEMA_FIELD({member.get_type().as_str()}, {member.name})')

		if self.has_flag(ArgsFlags.AddComments):
			self.write(f' // 0x{member.offset:X} ({member.offset})')

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
		return self

	def atomic_definition(self, atomic: SubTypeAtomic):
		return self

	def enum_static_asserts(self, enum_obj: ObjectDefinition):
		return self

def print_stdout(text):
	global args

	if not args.silent:
		print(text)

def main():
	global args

	parser = argparse.ArgumentParser(
		description = "Generates source2 schema definitions for C++ macro defines out of dumped schema data.",
		usage = "%(prog)s [options]"
	)

	parser.add_argument('-i', '--input', help = 'The path to the JSON schema file or a folder containing schema files, in which case the newest file would be processed. Default is ./dumps/ dir.', dest = 'schema_path', type = str, default = './dumps/')
	parser.add_argument('-o', '--output', help = 'The path to the output C++ file or a directory. Default is ./generated/ dir.', type = str, dest = 'out_path', default = "./generated/")
	parser.add_argument('-s', '--silent', help = 'Disables stdout output.', action = 'store_true', dest = 'silent')
	parser.add_argument('-c', '--comments', help = 'Generate help comments for resulting class/enum definitions.', action = 'store_true', dest = 'add_comments')
	parser.add_argument('-g', '--generate-classes', help = 'A list of class/enum definitions to generate.', required = True, nargs = '+', type = str, dest = 'generate_classes', default = None)

	args = parser.parse_args()
	flags = parse_args_as_flags(args)

	args.schema_path = locate_input_path(args.schema_path)
	schema_file = SchemaFile(args.schema_path)

	args.out_path = prepare_out_path(args.out_path, 'defs_' + os.path.basename(args.schema_path).replace('.json', '.h'))

	if 'no_parent_scope' not in schema_file.get_flags():
		print_stdout('!!! Schema dump was dumped with parent scope, which might not generate correct/full code.\n!!! Please dump shema without parent scope.')

	total_generated = 0
	with CppDefsWriter(open(args.out_path, 'w'), flags) as writer:
		context = CppContext(writer, flags)
		context.add_overrides(get_hl2sdk_common_types())

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