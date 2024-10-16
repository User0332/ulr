#include "Lib/Loader.hpp"
#include "Lib/Resolver.hpp"
#include <iostream>
#include <set>
#include <map>
#include <chrono>
#include <vector>
#include <codecvt>
#include <locale>
#include <string>
#include <csignal>
#include <Windows.h>
#include <dbghelp.h>

using namespace ULR;
using namespace ULR::Resolver;

char* (*special_string_MAKE_FROM_LITERAL)(char* str, int len);
char* (*special_array_from_ptr)(void* ptr, int size, Type* type);

ULRResult<char*> generate_ulr_argv(int argc, char* argv[])
{
	char** ulr_args = new char*[argc-1]; // argc-1 to skip ulrhost.exe arg

	for (int i = 0; i < argc-1; i++) // argc-1 to skip ulrhost.exe arg
	{
		ulr_args[i] = special_string_MAKE_FROM_LITERAL(argv[i+1], strlen(argv[i+1]));
	}

	auto res = Loader::GetType("[System]String[]");

	if (res.error) return { nullptr, res.error };

	Type* StringArrayType = res.result;

	char* arr = special_array_from_ptr(ulr_args, argc-1, StringArrayType);

	delete[] ulr_args;

	return { arr, None };
}

void handle_access_violation(int signo)
{
	std::cerr
		<< "Signal SIGSEGV recieved: Segmentation Fault. May be due to a null dereference or invalid pointer dereference. Stacktrace:"
		<< internal_api->GetStackTrace(2);
}

struct HostingResult // this implementation is not exposed to the caller of HostXXXXAssembly(), so no fields are public 
{
	int retcode;
	ULRInternalError error;
};

extern "C" HostingResult HostNativeAssembly(
	char* assembly_name,
	char* debugger_path,
	char* stdlib_path,
	int argc_full,
	char** argv_full
)
{
	std::signal(SIGSEGV, handle_access_violation);

	HMODULE debugger = LoadLibraryA(debugger_path); // TODO: grab from cli args

	if (!debugger)
	{
		return { 0, DebuggerNotFound };
	}

	bool debugger_successfully_loaded;

	ULRAPIImpl lclapi = ULRAPIImpl( // perhaps refactor Loader into an object someday
		&Loader::LoadedAssemblies,
		&Loader::ReadAssemblies,
		Loader::ReadNativeAssembly,
		Loader::LoadNativeAssembly,
		Loader::PopulateVtable,
		debugger,
		debugger_successfully_loaded
	);

	if (!debugger_successfully_loaded) return { 0, InvalidDebugger };

	/* Initialize Internal Library */

	internal_api = &lclapi;

	Assembly* ArrayTypeAssembly = new Assembly(strdup("ULR.<ArrayTypes>"), strdup(""), "", 0, { }, { nullptr }, (HMODULE) nullptr);
	Loader::ReadAssemblies[ArrayTypeAssembly->name] = ArrayTypeAssembly;
	Loader::LoadedAssemblies[ArrayTypeAssembly->name] = ArrayTypeAssembly;
	
	/* Load Stdlib*/
	
	Loader::ReadNativeAssembly(stdlib_path);

	auto res = Loader::LoadNativeAssembly(stdlib_path, &lclapi);

	if (res.error) return { 0, res.error }; // TODO: this and below may actually cause leaks, have a ULRCleanup function which is called before returning (could be done by wrapper function)

	Assembly* stdlibasm = res.result;

	/* Load Main Assembly */

	Loader::ReadNativeAssembly(assembly_name);

	res = Loader::LoadNativeAssembly(assembly_name, &lclapi);

	if (res.error) return { 0, res.error };

	Assembly* mainasm = res.result;

	if (mainasm->entry == nullptr)
	{
		return { 0, EntryPointNotFound };
	}

	/* Initialize string[] args / argv */

	special_string_MAKE_FROM_LITERAL = (char* (*)(char*, int)) lclapi.LocateSymbol(stdlibasm, "special_string_MAKE_FROM_LITERAL");
	special_array_from_ptr = (char* (*)(void*, int, Type*)) lclapi.LocateSymbol(stdlibasm, "special_array_from_ptr");
	
	auto args_res = generate_ulr_argv(argc_full, argv_full);

	if (args_res.error) return { 0, args_res.error };

	char* ulr_args_arr_obj = args_res.result;

	int retcode;
	
	lclapi.InitGCLocalVarRoot((char**) &retcode);

	retcode = mainasm->entry(ulr_args_arr_obj);

	
	// Final deallocation and cleanup (of ULR objects and the allocated assemblies)

	for (auto& entry : lclapi.allocated_objs)
	{		
		free(entry.first);
	}

	lclapi.allocated_size = 0;

	for (void* ptr : lclapi.allocated_field_offsets)
	{
		free(ptr);
	}

	std::set<Assembly*> allocated_asms;

	for (auto& entry : Loader::ReadAssemblies) allocated_asms.emplace(entry.second);
	for (auto& entry : Loader::LoadedAssemblies) allocated_asms.emplace(entry.second);

	for (auto allocated_assembly : allocated_asms)
	{
		delete allocated_assembly;
	}

	for (auto placeholder : Loader::alloced_generic_placeholders)
	{
		delete placeholder;
	}

	FreeLibrary(debugger);
	SymCleanup(GetCurrentProcess());

	return { retcode, None };
}