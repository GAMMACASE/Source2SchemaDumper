# Source2 Schema Dumper

This [metamod-source](https://www.metamodsource.net/downloads.php?branch=dev) plugin provides capabilities to dump Source2 schema definitions from CS2, Deadlock and Dota2 in a json/kv3 format for later processing in various ways.

## Usage

To perform schema dump run ``dump_schema`` command when server has started and initialized (calling this too early on startup might be unavailable). By default running this command without any extra args would dump base level schema definitions.

This command supports the following arguments:
 * ``help``: Prints help message with up to date flags.
 * ``verbose``: Provides verbose output of the dump process, mostly useful for debugging.
 * ``as_json``: Dumps to a json file (Default).
 * ``as_kv3``: Dumps to a kv3 file.
 * ``metatags``: Dump metatags.
 * ``atomics``: Dump atomics.
 * ``pulse_bindings``: Dump pulse bindings.
 * ``split_atomics``: Splits templated atomic names and leaves only base name leaving templated stuff. (Makes ``CUtlVector<int>`` to be named as ``CUtlVector`` for example).
 * ``ignore_parents``: Ignores parent scope decls and removes inlined structs/classes converting them from A::B to A__B.
 * ``apply_netvar_overrides``: Applies netvar overrides to types (MNetworkVarTypeOverride metatags).
 * ``all``: Shorthand version of providing ``metatags``, ``atomics`` and ``pulse_bindings`` flags.
 * ``for_cpp``: Use optimal flags for cpp generation later, similar to providing ``split_atomics``, ``ignore_parents`` and ``apply_netvar_overrides`` flags.
> [!NOTE]
> Pulse bindings are heavily under development by valve, so these are expected to break with each engine update in the supported game list, and would require manual update to the code most likely!

Example usage:
 * ``dump_schema metatags pulse_bindings``: Would dump pulse_bindings and general schema information with metatags.
 * ``dump_schema all for_cpp``: Would provide best result for later cpp generation as well as dumps everything it can.

## Generator scripts

This plugin comes with a set of particular python generator scripts located in the root of a plugin folder (``addons/schemadump/``) that accept **JSON** schema dumps:

 * ``generate_cpp.py``: Generates fully cpp compatible definitions out of dumped schema. This script is mainly useful for later usage for **idaclang** or similar stuff, it also supports supplying [hl2sdk](https://github.com/alliedmodders/hl2sdk) definitions of common utl structs.
  
    It supports the following args:
   * ``-i`` ``--input``: The path to the **JSON** schema file or a folder containing schema files, in which case the newest file would be processed. Default is **./dumps/** dir.
   * ``-o`` ``--output``: The path to the output C++ file or a directory. Default is **./generated/** dir.
   * ``-s`` ``--silent``: Disables stdout output.
   * ``-c`` ``--comments``: Generate help comments for resulting class/enum definitions.
   * ``-g`` ``--generate-classes``: A list of class/enum definitions to generate. Use "all" to generate all classes (Default).
   * ``-a`` ``--static-assert``: Generate static assertions of resulting class/enum definitions to ensure their validity.
   * ``-d`` ``--supply-hl2sdk``: Supplies hl2sdk class/enum definitions if applicable to the generated file.
> [!NOTE]
> Schema dumps with parent scope, might not generate correct code without supplying [hl2sdk](https://github.com/alliedmodders/hl2sdk) definitions!
    
 * ``generate_cpp_defs.py``: Example script to generate [s2ze](https://github.com/Source2ZE/CS2Fixes) compatible class definitions out of dumped schema.

    It supports the following args:
   * ``-i`` ``--input``: The path to the **JSON** schema file or a folder containing schema files, in which case the newest file would be processed. Default is **./dumps/** dir.
   * ``-o`` ``--output``: The path to the output C++ file or a directory. Default is **./generated/** dir.
   * ``-s`` ``--silent``: Disables stdout output.
   * ``-c`` ``--comments``: Generate help comments for resulting class/enum definitions.
   * ``-g`` ``--generate-classes``: A list of class/enum definitions to generate. Doesn't default to "all" and expects you to provide classes to generate.
> [!NOTE]
> This script is mostly an example of how you can automate dumping schema to macro headers for plugins that use [s2ze](https://github.com/Source2ZE/CS2Fixes) alike schema systems.

 * ``generate_pulse_bindings.py``: Generates source2 pulse bindings. Mostly for reference use. Requires pulse bindings to be dumped first in resulting dump.

    It supports the following args:
   * ``-i`` ``--input``: Path to **JSON** schema file or a folder containing schema files, in which case the newest file would be processed. Default is **./dumps/** dir.
   * ``-o`` ``--output``: Path to output C++ file or a directory. Default is **./generated/** dir.
   * ``-s`` ``--silent``: Disables stdout output.
   * ``-n`` ``--no-comments``: Don't generate help comments for resulting domain definitions.
   * ``-g`` ``--generate-domains``: A list of domain definitions to generate. Use "all" to generate all domains (Default).

## IDAClang usage example

One of the main advantages of this plugin and its scripts is that it allows for a full idaclang compilation support! But it requires a small setup beforehand if you are new to idaclang and only starting to setting up, some key notes to know:
 * This guide was written with windows in mind, it should be possible to do the same for linux binaries, but be aware that schema differs per platform, so you'd need a linux dump first, as well as some corrections to compile arguments!
 * Only versions of ida 7.7 and above are supported, as that was the first version where idaclang was introduced;
 * Make sure compiler used by ida is clang, ``Options->Compiler`` locate ``Source parser``, should be set to ``clang``. To parse c++ file, use ``File->Load file->Parse C header file``;
 * Compile arguments:
   * Target platform, make sure you are targetting correct platform (example: ``-target x86_64-pc-windows-msvc19.16.27045``);
   * C++ version, 17 and above should be supported, but was tested on c++17 (example: ``-std=c++17``);
   * Some hl2sdk code expects certain defines set, notably: ``X64BITS``, ``PLATFORM_64BITS`` (example: ``-DX64BITS -DPLATFORM_64BITS``);
   * There are few other handy defines and arguments I'd suggest using (especially for hl2sdk support), so here's the complete argument string for reference that was used during test (ida 7.7 but should work for further versions as well):
      ```
      -x c++ -std=c++17 -Wno-c++11-narrowing -target x86_64-pc-windows-msvc19.16.27045 -D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_GLIBCXX_DEBUG=0 -D_ITERATOR_DEBUG_LEVEL=0 -DDEBUG -D_DEBUG -DWIN32 -D_WINDOWS -DCOMPILER_MSVC -DCOMPILER_MSVC64 -DX64BITS -DPLATFORM_64BITS -D_WIN32=1 -D_MSC_VER=1900 -D__clang__ -D__clang_major__=17 -D_CRT_USE_BUILTIN_OFFSETOF=1
      ```
 * Include paths:
   * Generally for non-hl2sdk build you should be fine with whatever idaclang has already, but if you want to have hl2sdk to build correctly, here's the paths list you need to have for it to locate everything correctly:
     * ``{HL2SDKPATH}\public``
     * ``{HL2SDKPATH}\game\shared``
     * ``{HL2SDKPATH}\game\server``
     * ``{HL2SDKPATH}\public\tier0``
     * ``{HL2SDKPATH}\public\tier1``
     * ``{HL2SDKPATH}\public\mathlib``
     * ``{HL2SDKPATH}\public\entity2``
     * ``{HL2SDKPATH}\public\engine``
     * ``{HL2SDKPATH}\public\game\server``
     * ``{HL2SDKPATH}\thirdparty\protobuf-3.21.8\src``
     * ``{HL2SDKPATH}\common``
     * Replace ``{HL2SDKPATH}`` with an absolute path to hl2sdk for a game you are targetting (like ``hl2sdk-cs2``);
 * If you want to preserve arguments and include paths between binaries, you can edit ``{IDAPATH}/cfg/idaclang.cfg`` in there edit ``CLANG_ARGV``, where include directories could be supplied with ``-I`` argument (example ``-ID:\\hl2sdk-cs2\\public``);

### Some gotchas & ABI differences

Since idaclang uses clang under the hood to compile c++, it would mismatch on some stuff with MSVC, you can see what is supported [here](https://clang.llvm.org/docs/MSVCCompatibility.html). But here's the general gist of what you'd most likely encounter:
 * [Member function pointers](https://isocpp.org/wiki/faq/pointers-to-members) are of a different size (16 on clang, 8 on MSVC) which would produce different sized structs. That's not used in schema, but is used in hl2sdk if you plan on not only limiting with schema (Notable cases in hl2sdk: ``CUtlDelegate``, ``CUtlBuffer``);
 * Inlined structs/classes of templated classes might not be picked up correctly by idaclang and might be missing, example:
    ```cpp
    template <typename T>
    class TemplateTest
    {
        struct SubStruct
        {
            T m_Val;
        };
        
        SubStruct *m_Dummy;
    };

    TemplateTest<int> testvar;
    ```
    in this example, ``TemplateTest<int>::SubStruct`` would be only be declared but not defined in ida's type database, producing incomplete results. There's no global fix for this (if you know any lmk!!!), you can only fix it locally per template instance by providing [explicit template specialization](https://en.cppreference.com/w/cpp/language/template_specialization.html) of said type which would compile everything there's for it, example fix for the above case would be adding this before any usages of said template specialization:
    ```cpp
    template class TemplateTest<int>;
    ```
    Example hl2sdk code that is affected by this: ``CUtlLinkedList`` and its variations;

### CUtlDelegate compilation issues

That's not part of this plugin scope, as nothing in schema uses it, but I'll anyway provide a "fix" for this case for anyone who might wonder how to deal with errors such as ``fatal: failed to forward declare type: name="utldelegate::SimplifyMemFunc<SINGLE_MEMFUNCPTR_SIZE + sizeof(int)>" code=-3`` and alike, put this cpp code before any hl2sdk header files:
```cpp
// Hack fix as clang compiles member function pointers to 16 bytes and msvc uses 8,
// so this struct ends up +8 bytes longer than msvc produces
#define UTLDELEGATE_H
template <typename Signature>
class CUtlDelegate;

template<typename R>
class CUtlDelegate< R( ) > { char pad[16]; };
template<typename R, class p1>
class CUtlDelegate< R( p1 ) > { char pad[16]; };
template<typename R, class p1, class p2>
class CUtlDelegate< R( p1, p2 ) > { char pad[16]; };
template<typename R, class p1, class p2, class p3>
class CUtlDelegate< R( p1, p2, p3 ) > { char pad[16]; };
template<typename R, class p1, class p2, class p3, class p4>
class CUtlDelegate< R( p1, p2, p3, p4 ) > { char pad[16]; };
template<typename R, class p1, class p2, class p3, class p4, class p5>
class CUtlDelegate< R( p1, p2, p3, p4, p5 ) > { char pad[16]; };
class CUtlAbstractDelegate { char pad[16]; };
```

## Manual building example

### Prerequisites
 * [hl2sdk](https://github.com/alliedmodders/hl2sdk) for CS2, Dota2 and Deadlock;
 * [metamod-source](https://www.metamodsource.net/downloads.php?branch=dev);
 * [python3](https://www.python.org/);
 * [ambuild](https://github.com/alliedmodders/ambuild), make sure ``ambuild`` command is available via the ``PATH`` environment variable;

### Setting up
 * **(Optional)** Setup environment variables to point to correct directories:
   * ``MMSOURCE20``/``MMSOURCE_DEV`` should point to root of [metamod-source](https://www.metamodsource.net/downloads.php?branch=dev) of the version 2.0 and higher;
   * ``HL2SDKMANIFESTS`` should point to [hl2sdk-manifests](https://github.com/alliedmodders/hl2sdk-manifests) directory.
   * ``HL2SDKROOT`` should point to root folder where [hl2sdk](https://github.com/alliedmodders/hl2sdk) directories are in;
   * Alternatively ``HL2SDK{GAME}`` (e.g. ``HL2SDKCS2``) can be used to point to game specific [hl2sdk](https://github.com/alliedmodders/hl2sdk) directories, in which case a direct path to its root needs to be provided;
> [!NOTE]
> If you have ``HL2SDKROOT`` defined as well, it will take priority over game specific environment variables!
 * ``mkdir build`` & ``cd build`` in the root of the plugin folder.
 * Open the [MSVC developer console](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line) with the correct platform (x86_64).
 * Run ``python3 ../configure.py --enable-optimize -s {GAMES}`` if you have setup env vars or provide correct paths via ``--hl2sdk-root``, ``--hl2sdk-manifests`` and ``--mms_path`` args (``{GAMES}`` should be comma separated list of names like ``cs2,dota,deadlock``).

### Building
 * Open the [MSVC developer console](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line) with the correct platform (x86_64).
 * Run ``ambuild`` in ``./build`` subdirectory created earlier.
 * Once the plugin is compiled the files would be packaged and placed in ``\build\package`` folder.
  
### Generating MSVC solution
 * Run ``python3 ../configure.py --enable-optimize --gen=vs --vs-version=2022`` in ``./build`` subdirectory if you have setup env vars or provide correct paths via ``--hl2sdk-root``, ``--hl2sdk-manifests`` and ``--mms_path`` args.
 * Solutions would be created in ``./build`` subdirectory separately for each supported game.

## KV3/JSON Dump structure
 * ``dump_flags``: An array of flags that were used during dumping process.
 * ``defs``: Main entry point for all schema definitions, contains an array of objects having following structure:
   * ``type``: Object type (Could either be ``builtin``, ``class`` or ``enum``);
   * ``name``: Object name;
   * ``scope``: Scope where this object was defined in the schema during dumping procedure;
   * ``size``: Object size in bytes;
   * ``alignment``: Object alignment in bytes (Could be ``255``, which means the actual alignment isn't defined in schema and is unknown);
   * ``class`` and ``enum`` type specific members:
     * ``project``: Project name where this type originates from in schema;
     * ``traits``: Object of an additional traits, like:
       * ``flags``: An array of schema flags used for this object in a string form (Either ``SchemaClassFlags1_t`` or ``SchemaEnumFlags_t``);
       * ``metatags``: An array of schema metatags defined for this object;
       * Traits that are available only with parent scope dumps:
         * ``parent_class_idx``: Parent class index into ``defs`` array (Could be -1 if parent class is missing from schema);
         * ``child_class_idx``: An array of child class indexes into ``defs`` array;
       * ``class`` specific traits:
         * ``members``: An array of member objects that lists all members of a schema class and its structure is provided below;
         * ``multi_depth``: Integer value representing multiple inheritance depth calculated by schema;
         * ``single_depth``: Integer value representing single inheritance depth calculated by schema;
         * ``baseclasses``: An array of baseclass references in the following format:
           * ``offset``: Byte offset of this baseclass;
           * ``ref_idx``: Reference index into ``defs`` array;
       * ``enum`` specific traits:
         * ``fields``: An array of field objects that lists all fields available for this enum in schema, fields have the following format:
           * ``name``: Field name;
           * ``value``: Field value;
           * Fields can also have metatags at ``traits/metatags``;
 * ``atomics``: An array of atomic info objects in the following format:
   * ``name``: Atomic name;
   * ``token``: ``CUtlStringToken`` hash of a name;
   * Atomics can also have metatags at ``traits/metatags``;
 * ``pulse_bindings``: An array of pulse domain objects in the following format:
   * ``name``: Domain name;
   * ``description``: Domain description;
   * ``friendly_name``: Domain friendly name;
   * ``cursor``: Cursor name used for this domain;
   * ``cpp_scopes``: An array of scope objects attached to this domain in the following format:
     * ``name``: Scope name;
     * ``functions``: An array of function objects in the following format:
       * ``name``: Function name in a friendly format (e.g. has spaces after upper case letters);
       * ``library_name``: Cpp version of a name;
       * ``description``: Valve provided description;
       * ``type``: Could either be ``plain`` or ``event``;
       * ``params``: An array of param objects in the following format:
         * ``name``: Param name;
         * ``type``: Param type in a string format;
         * ``default_value``: Param default value (If it has one set);
         * Can also have metatags at ``traits/metatags``;
       * ``rets``: An array of return objects (Could be empty meaning no return) in the following format:
         * ``name``: Return name;
         * ``type``: Return type in a string format;
         * ``default_value``: Return default value (If it has one set);
         * Can also have metatags at ``traits/metatags``;
       * Can also have metatags at ``traits/metatags``;

### Metatags

By default metatags require a flag to be dumped, otherwise they would not be present in a dump. Metatags can contain various type of values but this tool stringifies them all if possible otherwise would put ``!!{EXPLANATION}!!`` in its value (like ``!!UNKNOWN!!`` or ``!!NOT SUPPORTED!!``).

Metatags structure is simple:
 * ``name``: Metatag name;
 * ``value``: Metatag stringified value (Could be missing in case metatag doesn't support having a value);

### Class member fields

Class members have the following format:
 * ``name``: Member name;
 * ``offset``: Member byte offset in the object;
 * ``traits``: Object of an additional traits, like:
   * ``metatags``: An array of schema metatags defined for this member;
   * ``subtype``: Member type info object in the following format:
     * ``type``: Member type. Refer to class member typing for more info;
     * Type specific fields;

### Class member typing

Class members types could be nested and have a specific set of types available:
 * ``literal``: A literal type representing an integer value, has following additional fields:
   * ``value``: Literal integer value;
 * ``ref``: Reference type, has following additional fields:
   * ``ref_idx``: Reference index into ``defs`` array;
 * ``fixed_array``: Static array type, has following additional fields:
   * ``element_size``: Element size in bytes;
   * ``count``: Element count in an array;
   * ``subtype``: Nested subtype;
 * ``bitfield``: Bitfield type, has following additional fields:
   * ``count``: Bits count;
 * ``ptr``: Pointer type, has following additional fields:
   * ``subtype``: Nested subtype;
 * ``atomic``: Atomic type, has following additional fields:
   * ``name``: Atomic name;
   * ``size``: Atomic byte size;
   * ``alignment``: Atomic byte alignment;
   * ``template``: An array of nested template argument subtypes (Only exists if atomic is templated);
