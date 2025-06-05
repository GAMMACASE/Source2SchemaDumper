from enum import IntFlag
import os
import json
from generator_scripts.common import ArgsFlags
from generator_scripts.obj_defs import ObjectList
from generator_scripts.pulse_defs import DomainDefinition

class SchemaFile:
	path = ''
	schema = None
	defs: ObjectList | None = None
	pulse_bindings: list[DomainDefinition] | None = None

	def __init__(self, path):
		"""
		Initializes a new instance of the SchemaFile class.
		Args:
			path (str): The path to the JSON schema file.
		"""

		if not os.path.exists(path):
			raise Exception('Schema file path is invalid')

		self.path = path

		with open(path, 'r') as inp:
			try:
				self.schema = json.load(inp)
			except:
				raise Exception('Failed to parse JSON schema info')
		
		self.defs = ObjectList.parse_from(self.schema.get('defs', []))
		self.pulse_bindings = DomainDefinition.parse_from_list(self.schema.get('pulse_bindings', []))
		
	def get_flags(self):
		"""
		Returns:
			list: A list of dump flags.
		"""

		return self.schema.get('dump_flags', [])
	

	def has_metatags(self):
		"""
		Returns:
			bool: True if the schema file has metatags, False otherwise.
		"""

		return 'has_metatags' in self.get_flags()

	def has_atomics(self):
		"""
		Returns:
			bool: True if the schema file has atomic types, False otherwise.
		"""

		return 'has_atomics' in self.get_flags()
	
	def has_pulse_bindings(self):
		"""
		Returns:
			bool: True if the schema file has pulse bindings, False otherwise.
		"""

		return 'has_pulse_bindings' in self.get_flags()

class FileWriter:
	fd = None
	indent_level = 0
	flags: ArgsFlags = ArgsFlags.Empty

	def __init__(self, fd, flags = ArgsFlags.Empty):
		self.fd = fd
		self.indent_level = 0
		self.flags = flags
	
	def __enter__(self):
		return self

	def __exit__(self, exception_type, exception_value, exception_traceback):
		self.fd.close()

	def has_flag(self, flag):
		return flag in self.flags

	def format_indent(self, indent = -1):
		return '\t' * (self.indent_level if indent == -1 else indent)

	def format_multiline_text(self, text, newline_prefix):
		return newline_prefix + text.replace('\n', '\n' + newline_prefix)

	def dedent(self):
		self.indent_level -= 1
		return self

	def indent(self):
		self.indent_level += 1
		return self

	def write(self, text):
		self.fd.write(text)
		return self

	def write_indent(self):
		return self.write(self.format_indent())

	# Writes a new line
	def newl(self):
		return self.write('\n')

	# Writes an indented line
	def write_il(self, text, indent = -1):
		return self.write(self.format_multiline_text(text, self.format_indent(indent))).newl()

	def write_comment(self, text, indent = -1):
		return self.write(self.format_multiline_text(text, self.format_indent(indent) + '// ')).newl()

	def write_comment_check(self, comment):
		if self.has_flag(ArgsFlags.AddComments):
			return self.write_comment(comment)
		return self

class Helpers:
	@staticmethod
	def size_to_int(size, signed = False):
		match size:
			case 1:
				return 'uint8' if not signed else 'int8'
			case 2:
				return 'uint16' if not signed else 'int16'
			case 4:
				return 'uint32' if not signed else 'int32'
			case 8:
				return 'uint64' if not signed else 'int64'
			case _:
				raise Exception(f'Invalid size ({size}) to int was performed!')
