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

class PulseTraits:
	metatags: list[MetaTag] | None = None

	def __init__(self):
		pass

	@staticmethod
	def parse_from(raw_traits):
		pulse_traits = PulseTraits()

		if 'metatags' in raw_traits:
			pulse_traits.metatags = MetaTag.parse_from_list(raw_traits['metatags'])

		return pulse_traits

class DomainFunctionParam:
	name = ''
	type = ''
	default_value = ''
	traits: PulseTraits = None
	
	def __init__(self):
		pass

	def has_default_value(self):
		return self.default_value != ''

	def get_default_value(self):
		if self.type == 'string':
			return f'"{self.default_value}"'
		
		return self.default_value

	def get_description(self):
		if self.traits.metatags is None:
			return ''
		
		for meta_tag in self.traits.metatags:
			if meta_tag.name == 'MPulseArgDesc':
				return meta_tag.value
		
		return ''

	@staticmethod
	def parse_from(raw_param):
		param = DomainFunctionParam()
		param.name = raw_param['name']
		param.type = raw_param['type']
		param.default_value = raw_param.get('default_value', '')
		param.traits = PulseTraits.parse_from(raw_param.get('traits', {}))
		return param
	
	@staticmethod
	def parse_from_list(raw_params):
		params = []
		for raw_param in raw_params:
			params.append(DomainFunctionParam.parse_from(raw_param))
		return params

class DomainFunctionDefinition:
	name = ''
	library_name = ''
	description = ''
	type = ''
	params: list[DomainFunctionParam] = None
	rets: list[DomainFunctionParam] = None
	traits: PulseTraits = None

	def __init__(self):
		pass

	def cpp_name(self):
		return self.library_name.split('!')[-1].split('::')[-1]

	@staticmethod
	def parse_from(raw_function):
		function_def = DomainFunctionDefinition()
		function_def.name = raw_function['name']
		function_def.library_name = raw_function['library_name']
		function_def.description = raw_function['description']
		function_def.type = raw_function['type']

		function_def.params = DomainFunctionParam.parse_from_list(raw_function['params'])
		function_def.rets = DomainFunctionParam.parse_from_list(raw_function['rets'])

		function_def.traits = PulseTraits.parse_from(raw_function.get('traits', {}))
		return function_def

	@staticmethod
	def parse_from_list(raw_functions):
		function_defs = []
		for raw_function in raw_functions:
			function_defs.append(DomainFunctionDefinition.parse_from(raw_function))
		return function_defs

class DomainScopeDefinition:
	name = ''
	functions: list[DomainFunctionDefinition] = None

	def __init__(self):
		pass

	@staticmethod
	def parse_from(raw_scope):
		scope_def = DomainScopeDefinition()
		scope_def.name = raw_scope['name']
		scope_def.functions = DomainFunctionDefinition.parse_from_list(raw_scope['functions'])
		return scope_def

	@staticmethod
	def parse_from_list(raw_scopes):
		scope_defs = []
		for raw_scope in raw_scopes:
			scope_defs.append(DomainScopeDefinition.parse_from(raw_scope))
		return scope_defs

class DomainDefinition:
	name = ''
	description = ''
	friendly_name = ''
	cursor = ''
	scopes: list[DomainScopeDefinition] = None

	def __init__(self):
		pass

	@staticmethod
	def parse_from(raw_domain):
		domain_def = DomainDefinition()
		domain_def.name = raw_domain['name']
		domain_def.description = raw_domain.get('description', '')
		domain_def.friendly_name = raw_domain.get('friendly_name', '')
		domain_def.cursor = raw_domain.get('cursor', '')
		domain_def.scopes = DomainScopeDefinition.parse_from_list(raw_domain['cpp_scopes'])
		return domain_def
	
	@staticmethod
	def parse_from_list(raw_domains):
		domain_defs = []
		for raw_domain in raw_domains:
			domain_defs.append(DomainDefinition.parse_from(raw_domain))
		return domain_defs