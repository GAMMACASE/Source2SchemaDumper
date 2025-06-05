import os
from generator_scripts.common import ArgsFlags, locate_input_path, parse_args_as_flags, prepare_out_path
from generator_scripts.pulse_defs import DomainDefinition, DomainFunctionDefinition, DomainScopeDefinition
from generator_scripts.schema_file import SchemaFile, FileWriter
import argparse

args = None

class PulseBindingWriter(FileWriter):
	domain_indent = 0

	def __init__(self, stream, flags):
		super().__init__(stream, flags)
	
	def write_comment_check(self, comment):
		if self.has_flag(ArgsFlags.AddComments):
			return self.write_comment(comment)
		return self
	
	def write_domain_start(self, domain: DomainDefinition):

		if domain.description != '':
			self.write_comment_check(domain.description)

		if domain.friendly_name != '':
			self.write_comment_check(f'@friendly_name {domain.friendly_name}')

		if domain.cursor != '':
			self.write_comment_check(f'@cursor {domain.cursor}')

		self.write_il(f'domain {domain.name} {{').indent()

		return self
	
	def write_domain_end(self):
		return self.dedent().write_il('}')
	
	def write_scope_start(self, scope: DomainScopeDefinition):
		return self.write_il(f'scope {scope.name} {{').indent()

	def write_scope_end(self):
		return self.dedent().write_il('}')

	def write_function(self, func: DomainFunctionDefinition):
		if func.description != '':
			self.write_comment_check(func.description)
			self.write_comment_check('')

		if func.traits.metatags is not None:
			for metatag in func.traits.metatags:
				self.write_comment_check('@func_metatag {' + metatag.name + f'}} {metatag.value if metatag.has_value() else ""}')

		self.write_comment_check(f'@type {func.type}')
		self.write_comment_check(f'@libname "{func.library_name}"')
		self.write_comment_check(f'@friendly_name "{func.name}"')

		for param in func.params:
			if param.traits.metatags is not None:
				for metatag in param.traits.metatags:
					if metatag.name != 'MPulseArgDesc':
						self.write_comment_check('@param_metatag {' + metatag.name + f'}} {metatag.value if metatag.has_value() else ""}')

			self.write_comment_check('@param {' + param.type + f'}} {param.name} {param.get_description()}')

		for ret in func.rets:
			self.write_comment_check('@returns {' + ret.type + f'}} {ret.name} {ret.get_description()}')

		params = [x.name + (' = ' + x.get_default_value() if x.default_value != '' else '') for x in func.params]
		self.write_il(f'{func.cpp_name()}({", ".join(params)});')

		if self.has_flag(ArgsFlags.AddComments):
			self.newl()

		return self

class PulseContext:
	writer: PulseBindingWriter = None
	flags: ArgsFlags = ArgsFlags.Empty

	def __init__(self, writer, flags: ArgsFlags):
		self.writer = writer
		self.flags = flags

	def process_domain(self, domain: DomainDefinition):
		self.writer.write_domain_start(domain)

		for scope in domain.scopes:
			self.writer.write_scope_start(scope)

			for function in scope.functions:
				self.writer.write_function(function)

			self.writer.write_scope_end().newl()

		self.writer.write_domain_end().newl()

def print_stdout(text):
	global args

	if not args.silent:
		print(text)

def main():
	global args

	parser = argparse.ArgumentParser(
		description = "Generates pulse binding definitions for domains out of dumped schema data.",
		usage = "%(prog)s [options]"
	)

	parser.add_argument('-i', '--input', help = 'The path to the JSON schema file or a folder containing schema files, in which case the newest file would be processed. Default is ./dumps/ dir.', dest = 'schema_path', type = str, default = './dumps/')
	parser.add_argument('-o', '--output', help = 'The path to the output pulse bindings file or a directory. Default is ./generated/ dir.', type = str, dest = 'out_path', default = "./generated/")
	parser.add_argument('-s', '--silent', help = 'Disables stdout output.', action = 'store_true', dest = 'silent')
	parser.add_argument('-n', '--no-comments', help = 'Don\'t generate help comments for resulting domain definitions.', action = 'store_true', dest = 'no_comments')
	parser.add_argument('-g', '--generate-domains', help = 'A list of domain definitions to generate. Use "all" to generate all domains (Default).', nargs = '+', type = str, dest = 'generate_domains', default = 'all')

	args = parser.parse_args()
	flags = parse_args_as_flags(args, ArgsFlags.AddComments)

	args.schema_path = locate_input_path(args.schema_path)
	schema_file = SchemaFile(args.schema_path)

	args.out_path = prepare_out_path(args.out_path, 'pulse_bindings_' + os.path.basename(args.schema_path).replace('.json', '.txt'))

	if not schema_file.has_pulse_bindings():
		raise Exception('Schema file is missing pulse bindings!')

	total_generated = 0
	with PulseBindingWriter(open(args.out_path, 'w'), flags) as writer:
		context = PulseContext(writer, flags)

		if 'all' in args.generate_domains:
			print_stdout('Generating all domain definitions...')

			for domain in schema_file.pulse_bindings:
				total_generated += 1
				context.process_domain(domain)
		else:
			print_stdout(f'Generating ({", ".join(args.generate_domains)}) domain definitions...')
			
			for class_name in args.generate_domains:
				for domain in schema_file.pulse_bindings:
					if domain.name == class_name:
						total_generated += 1
						context.process_domain(domain)
						break
	
	print_stdout(f'Successfully generated pulse domains file at {os.path.abspath(args.out_path)} (Total domians processed: {total_generated})')

if __name__ == '__main__':
	main()
