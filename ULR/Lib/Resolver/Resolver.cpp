#include "../Resolver.hpp"
#include <dbghelp.h>
#include <sstream>

#define COLOR_INTEGER "\u001b[1m" // bold actually
#define COLOR_TYPE_GREEN "\u001b[92m"
#define COLOR_YELLOW "\u001b[33m"
#define COLOR_MAGENTA "\u001b[35m"
#define COLOR_FIELD_BLUE "\u001b[36m"
#define COLOR_END "\u001b[0m"

namespace ULR::Resolver
{
	ULRAPIImpl::ULRAPIImpl(
		std::map<char*, Assembly*, cmp_chr_ptr>* assemblies,
		std::map<char*, Assembly*, cmp_chr_ptr>* read_assemblies,
		HMODULE (*ReadAssembly)(char name[]),
		Assembly* (*LoadAssembly)(char name[], ULRAPIImpl* api)
	)
	{
		this->assemblies = assemblies;
		this->read_assemblies = read_assemblies;
		this->LoadAssemblyPtr = LoadAssembly;
		this->ReadAssemblyPtr = ReadAssembly;
	}

	// returns true if the assembly is successfully loaded. returns false if the assembly was not read yet (and therefore cannot be loaded). If the assembly was read but not loaded, this function loads the assembly fully and returns true
	bool ULRAPIImpl::EnsureLoaded(char assembly_name[])
	{
		if (assemblies->count(assembly_name) == 0)
		{
			if (read_assemblies->count(assembly_name) == 0) return false;

			LoadAssemblyPtr(assembly_name, this);
		}

		return true;
	}

	Assembly* ULRAPIImpl::LoadAssembly(char assembly_name[])
	{
		if (read_assemblies->count(assembly_name))
		{
			if (assemblies->count(assembly_name)) return assemblies->at(assembly_name);

			return LoadAssemblyPtr(assembly_name, this);
		}

		ReadAssemblyPtr(assembly_name);

		return LoadAssemblyPtr(assembly_name, this);
	}

	Assembly* ULRAPIImpl::LocateAssembly(char assembly_name[])
	{
		if (read_assemblies->count(assembly_name) != 0) return (*read_assemblies)[assembly_name];

		return (*assemblies)[assembly_name];
	}

	void* ULRAPIImpl::LocateSymbol(Assembly* assembly, char symbol_name[])
	{
		std::string_view cache_key = symbol_name;

		if (assembly->cached_sym_lookups.count(cache_key)) return assembly->cached_sym_lookups[cache_key];

		HMODULE mod = assembly->handle;

		void* addr = (void*) GetProcAddress(mod, symbol_name);

		if (addr) assembly->cached_sym_lookups[cache_key] = addr;

		return addr;
	}

	std::vector<MemberInfo*> ULRAPIImpl::GetMember(Type* type, char name[])
	{
		std::vector<MemberInfo*> matches = type->static_attrs[name];

		for (auto& to_add : type->inst_attrs[name])
		{
			matches.emplace_back(to_add);
		}

		return matches;
	}

	ConstructorInfo* ULRAPIImpl::GetCtor(Type* type, std::vector<Type*> signature)
	{
		std::vector<MemberInfo*> ctors = type->static_attrs[".ctor"];

		for (auto& ctor : ctors)
		{
			ConstructorInfo* casted = (ConstructorInfo*) ctor;

			if (casted->signature == signature) return casted;
		}
		
		throw /* new NoConstructor exc*/;
	}

	MethodInfo* ULRAPIImpl::GetMethod(Type* type, char name[], std::vector<Type*> argsignature, int bindingflags)
	{
		if (bindingflags & BindingFlags::Instance)
		{
			for (auto& member : type->inst_attrs[name])
			{
				if (member->decl_type & MemberType::Method)
				{
					MethodInfo* casted = (MethodInfo*) member;

					if (casted->argsig == argsignature)
					{
						if (member->attrs & Modifiers::Public)
						{
							if(bindingflags & BindingFlags::Public) return casted;
						}
						else if (bindingflags & BindingFlags::NonPublic) return casted;
					}
				}
			}
		}

		if (bindingflags & BindingFlags::Static)
		{
			for (auto& member : type->static_attrs[name])
			{
				if (member->decl_type & MemberType::Method)
				{
					MethodInfo* casted = (MethodInfo*) member;

					if (casted->argsig == argsignature)
					{
						if (member->attrs & Modifiers::Public)
						{
							if(bindingflags & BindingFlags::Public) return casted;
						}
						else if (bindingflags & BindingFlags::NonPublic) return casted;
					}
				}
			}
		}

		throw /* new MethodNotFound exc */;
	}

	FieldInfo* ULRAPIImpl::GetField(Type* type, char name[], int bindingflags)
	{
		if (bindingflags & BindingFlags::Instance)
		{
			for (auto& member : type->inst_attrs[name])
			{
				if (member->decl_type & MemberType::Field)
				{
					if (member->attrs & Modifiers::Public && bindingflags & BindingFlags::Public) return (FieldInfo*) member;
					if (bindingflags & BindingFlags::NonPublic) return (FieldInfo*) member;
				}
			}
		}

		if (bindingflags & BindingFlags::Static)
		{
			for (auto& member : type->static_attrs[name])
			{
				if (member->decl_type & MemberType::Field)
				{
					if (member->attrs & Modifiers::Public && bindingflags & BindingFlags::Public) return (FieldInfo*) member;
					if (bindingflags & BindingFlags::NonPublic) return (FieldInfo*) member;
				}
			}
		}

		throw /* new FieldNotFound exc */;		
	}

	PropertyInfo* ULRAPIImpl::GetProperty(Type* type, char name[], int bindingflags)
	{
		if (bindingflags & BindingFlags::Instance)
		{
			for (auto& member : type->inst_attrs[name])
			{
				if (member->decl_type & MemberType::Property)
				{
					if (member->attrs & Modifiers::Public && bindingflags & BindingFlags::Public) return (PropertyInfo*) member;
					if (bindingflags & BindingFlags::NonPublic) return (PropertyInfo*) member;
				}
			}
		}

		if (bindingflags & BindingFlags::Static)
		{
			for (auto& member : type->static_attrs[name])
			{
				if (member->decl_type & MemberType::Property)
				{
					if (member->attrs & Modifiers::Public && bindingflags & BindingFlags::Public) return (PropertyInfo*) member;
					if (bindingflags & BindingFlags::NonPublic) return (PropertyInfo*) member;
				}
			}
		}

		throw /* new FieldNotFound exc */;		
	}

	DestructorInfo* ULRAPIImpl::GetDtor(Type* type)
	{
		std::vector<MemberInfo*> dtors = type->static_attrs[".dtor"];

		if (dtors.size() == 0) throw /* new NoDestructor exc */;

		return (DestructorInfo*) dtors[0];
	}

	// }
	Type* ULRAPIImpl::GetType(char full_qual_typename[])
	{
		for (auto& entry : *assemblies) // optimize this somehow
		{
			auto& assembly = entry.second;

			if (assembly->types.count(full_qual_typename) != 0) return assembly->types[full_qual_typename];
		}

		/* try to load assemblies until type is found */
		for (auto& entry : *read_assemblies)
		{
			if (assemblies->count(entry.first)) continue;

			if (!EnsureLoaded(entry.first)) continue;

			if (entry.second->types.count(full_qual_typename) != 0) return entry.second->types[full_qual_typename];
		}

		throw /* new TypeNotFound exc */;
	}

	Type* ULRAPIImpl::GetType(char full_qual_typename[], char assembly_hint[])
	{
		if (read_assemblies->count(assembly_hint) == 0 && assemblies->count(assembly_hint)) throw std::runtime_error("first fault") /* new TypeNotFound exc */;

		auto& assembly = (*assemblies)[assembly_hint];
		
		if (assembly->types.count(full_qual_typename) != 0) return assembly->types[full_qual_typename];

		throw std::runtime_error("second fault") /* new TypeNotFound exc */;
	}

	Type* ULRAPIImpl::GetTypeOf(char* obj)
	{
		Type** type_ptr_extract = reinterpret_cast<Type**>(obj);

		return type_ptr_extract[0];
	}
	
	char* ULRAPIImpl::AllocateObject(size_t size)
	{
		if (allocated_size+size > GC_TRIGGER_SIZE && ((allocated_size+size) - prev_size_accessible) > 10 MB) Collect();

		return AllocateObjectNoGC(size);
	}

	char* ULRAPIImpl::AllocateZeroed(size_t size)
	{
		if (allocated_size+size > GC_TRIGGER_SIZE && ((allocated_size+size) - prev_size_accessible) > 10 MB) Collect();

		return AllocateZeroedNoGC(size);
	}

	char* ULRAPIImpl::AllocateObjectNoGC(size_t size)
	{
		char* mem = (char*) malloc(size);

		allocated_objs.emplace(mem, size);

		allocated_size+=size;

		return mem;
	}

	char* ULRAPIImpl::AllocateZeroedNoGC(size_t size)
	{
		char* mem = (char*) calloc(size, 1);

		allocated_objs.emplace(mem, size);

		allocated_size+=size;
		
		return mem;
	}

	std::set<char*> ULRAPIImpl::ExamineRoot(char* root)
	{
		std::set<char*> found = { root };

		Type* root_type = GetTypeOf(root);

		for (auto& entry : root_type->inst_attrs)
		{
			if (entry.second[0]->decl_type == MemberType::Field)
			{
				FieldInfo* field = (FieldInfo*) entry.second[0];

				char* fieldval = field->GetValue(root);

				if (found.count(fieldval)) continue; // prevent circular refs

				auto found_from_field = ExamineRoot(fieldval);

				found.insert(found_from_field.begin(), found_from_field.end());	
			}
		}

		return found;
	}

	std::set<char*> ULRAPIImpl::ExamineRoots(std::set<char*> roots)
	{
		std::set<char*> total;

		for (auto& root : roots)
		{
			if (total.count(root)) continue; // prevent circular refs

			auto found = ExamineRoot(root);

			total.insert(found.begin(), found.end());	
		}

		return total;
	}

	GCResult ULRAPIImpl::Collect()
	{
		std::set<char*> roots; // aggregate a list of all registered locals

		for (auto& entry : *assemblies)
		{
			// NOTE: BELOW IS BROKEN FOR VALUE TYPES, TODO: FIX
			// add local var addrs to roots
			for (size_t i = 0; i < entry.second->localslen; i++)
			{
				char* lcladdr = entry.second->locals[i];

				if (lcladdr == nullptr) continue;

				roots.emplace(lcladdr); // add the local var to the application root
			}

			// add static roots to local var roots
			for (auto& type_entry : entry.second->types)
			{
				for (auto& static_entry : type_entry.second->static_attrs)
				{
					if (static_entry.second[0]->decl_type == MemberType::Field)
					{
						roots.emplace(((FieldInfo*) static_entry.second[0])->GetValue(nullptr));
					}
				}
			}
		}

		std::set<char*> still_accessible = ExamineRoots(roots);

		GCResult result;

		auto alloced_copy = allocated_objs;

		for (auto& entry : alloced_copy)
		{
			auto& alloced = entry.first;

			if (!still_accessible.count(alloced))
			{
				result.num_collected++;
				result.size_collected+=entry.second;

				Type* objtype = GetTypeOf(alloced);

				GetDtor(objtype)->Invoke(alloced); // call destructor

				free(alloced);

				allocated_objs.erase(alloced);
			}
		}

		allocated_size-=result.size_collected;

		prev_size_accessible = allocated_size;

		return result;
	}

	Assembly* ULRAPIImpl::ResolveAddressToAssembly(void* addr)
	{
		for (auto& entry : *assemblies)
		{
			if (entry.second->handle == addr) return entry.second;
		}
		
		for (auto& entry : *read_assemblies)
		{
			if (entry.second->handle == addr) return entry.second;
		}

		return nullptr;
	}

	MemberInfo* ULRAPIImpl::ResolveAddressToMember(void* addr)
	{
		for (auto& entry : *assemblies)
		{
			for (auto& type_entry : entry.second->types)
			{
				for (auto& member_entry : type_entry.second->inst_attrs)
				{
					if (
						member_entry.second[0]->decl_type == MemberType::Field ||
						member_entry.second[0]->decl_type == MemberType::Property)
							continue;
					
					for (auto& member : member_entry.second)
					{
						if (member->decl_type == MemberType::Method && ((MethodInfo*) member)->offset == addr) return member;
					}
				}

				for (auto& member_entry : type_entry.second->static_attrs)
				{
					if (
						member_entry.second[0]->decl_type == MemberType::Field ||
						member_entry.second[0]->decl_type == MemberType::Property)
							continue;
					
					for (auto& member : member_entry.second)
					{
						if (member->decl_type == MemberType::Method && ((MethodInfo*) member)->offset == addr) return member;
						if (member->decl_type == MemberType::Ctor && ((ConstructorInfo*) member)->offset == addr) return member;
						if (member->decl_type == MemberType::Dtor && ((DestructorInfo*) member)->offset == addr) return member;
					}
				}
			}
		}

		return nullptr;
	}

	std::string ULRAPIImpl::GetFullyQualifiedNameOf(MemberInfo* member)
	{
		std::string base = GetDisplayNameOf(member->parent_type);

		base.push_back('.');
		base.append(member->name);

		return base;
	}

	std::string ULRAPIImpl::GetDisplayNameOf(Type* type)
	{		
		if (type->decl_type == TypeType::ArrayType)
		{
			std::string base = GetDisplayNameOf(type->element_type);

			base.append("[]");

			return base;
		}
		
		std::string dispname = type->name;

		if (dispname.find("[]") == 0)
		{
			dispname.erase(0, 2);

			dispname.insert(0, COLOR_TYPE_GREEN);
			dispname.append(COLOR_END);

			return dispname;
		}

		dispname.erase(0, 1);

		dispname.insert(0, COLOR_TYPE_GREEN);

		dispname.replace(dispname.find(']'), 1, ".");

		size_t lastpos = 0;

		while ((lastpos = dispname.find('.', lastpos)) != std::string::npos)
		{
			dispname.replace(lastpos, 1, COLOR_END "." COLOR_TYPE_GREEN);

			lastpos+=sizeof(COLOR_END)+sizeof(".")+sizeof(COLOR_TYPE_GREEN);
		}

		dispname.append(COLOR_END);

		return dispname;
	}

	std::string ULRAPIImpl::GetDisplayNameOf(MemberInfo* member)
	{
		std::string base_display = GetFullyQualifiedNameOf(member);

		if (member->decl_type == MemberType::Field)
		{
			base_display.replace(base_display.find_last_of('.'), 1, "." COLOR_FIELD_BLUE);
			base_display.append(COLOR_END);

			base_display.insert(base_display.begin(), ' ');
			base_display.insert(0, GetDisplayNameOf(((FieldInfo*) member)->valtype));
			
			return base_display;
		}

		if (member->decl_type == MemberType::Property)
		{
			base_display.replace(base_display.find_last_of('.'), 1, "." COLOR_FIELD_BLUE);
			base_display.append(COLOR_END);

			base_display.insert(base_display.begin(), ' ');
			base_display.insert(0, GetDisplayNameOf(((PropertyInfo*) member)->valtype));
			
			return base_display;
		}

		if (member->decl_type == MemberType::Dtor)
		{
			base_display.insert(0, COLOR_YELLOW);
			base_display.append(COLOR_END);

			base_display.append("()");

			return base_display;
		}

		if (member->decl_type == MemberType::Ctor)
		{
			base_display.insert(0, COLOR_YELLOW);
			base_display.append(COLOR_END);

			base_display.push_back('(');

			for (auto& arg : ((ConstructorInfo*) member)->signature)
			{
				base_display.append(GetDisplayNameOf(arg));
				base_display.append(", ");
			}

			if (((ConstructorInfo* ) member)->signature.size() != 0) base_display.erase(base_display.size()-2);
			
			base_display.push_back(')');

			return base_display;
		}

		if (member->decl_type == MemberType::Method)
		{
			base_display.replace(base_display.find_last_of('.'), 1, "." COLOR_YELLOW);
			base_display.append(COLOR_END);

			base_display.insert(base_display.begin(), ' ');
			base_display.insert(0, GetDisplayNameOf(((MethodInfo*) member)->rettype));

			base_display.insert(base_display.end(), '(');

			for (auto& arg : ((MethodInfo*) member)->argsig)
			{
				base_display.append(GetDisplayNameOf(arg));
				base_display.append(", ");
			}

			if (((MethodInfo* ) member)->argsig.size() != 0) base_display.erase(base_display.size()-2);
			
			base_display.insert(base_display.end(), ')');

			return base_display;
		}

		return "invalid member type";
	}
	
	std::string ULRAPIImpl::GetStackTrace(int skipframes)
	{
		std::string bt_str("\n\t");

		bt_str.reserve(MAX_TRACEBACK*20);

		void* bt[MAX_TRACEBACK];

		HANDLE proc = GetCurrentProcess();

		SymInitialize(proc, NULL, true);

		unsigned short num_frames = CaptureStackBackTrace(1+skipframes, MAX_TRACEBACK, bt, NULL);

		for (int i = 0; i < num_frames; i++)
		{
			IMAGEHLP_SYMBOL64 info;

			if (!SymGetSymFromAddr(proc, (DWORD64) bt[i], NULL, &info)) break;

			void* funcaddr = (void*) info.Address;

			MemberInfo* member = ResolveAddressToMember(funcaddr);
			
			if (!member)
			{
				MEMORY_BASIC_INFORMATION mbi;

				if (!VirtualQuery(bt[i], &mbi, sizeof(mbi))) break;

				HMODULE instrmod = (HMODULE) mbi.AllocationBase;

				std::stringstream fmt_ptr;

				fmt_ptr << COLOR_INTEGER << std::hex << bt[i] << COLOR_END;

				if (GetProcAddress(instrmod, "ulr_identify_nativelib") != nullptr)
				{
					bt_str.append("in ULR API function (ULR NativeLib) @ ");
				}
				else
				{
					Assembly* assembly = ResolveAddressToAssembly(instrmod);

					if (assembly)
					{
						bt_str.append("in assembly-local API function (");
						bt_str.append(COLOR_MAGENTA);
						bt_str.append(assembly->name);
						bt_str.append(COLOR_END);
						bt_str.append(") @ ");
					}
					else 
					{
						// grab dll name
						char* name = new char[0];
						size_t name_size = 20;
						size_t actual_size = 0;

						do
						{
							name_size+=5;

							delete[] name;
							name = new char[name_size];

							actual_size = GetModuleFileNameA(instrmod, name, name_size-1);
						} while (actual_size > name_size-2);

						name[actual_size] = '\0';
						
						// end grab dll name

						// grab func name
						size_t base_funcinfo_size = sizeof(SYMBOL_INFO);
						char* funcinfobuf = new char[0];
						char* funcname;
						name_size = 20;
						actual_size = 0;

						DWORD64 displacement;

						do
						{
							name_size+=5;

							delete[] funcinfobuf;
							funcinfobuf = new char[base_funcinfo_size+name_size];

							PSYMBOL_INFO info_ptr = (PSYMBOL_INFO) funcinfobuf;
							info_ptr->SizeOfStruct = base_funcinfo_size;
							info_ptr->MaxNameLen = name_size;

							if (!SymFromAddr(proc, info.Address, &displacement, info_ptr))
							{
								funcname = "<unknown func>";
								break;
							}

							funcname = info_ptr->Name;
							actual_size = info_ptr->NameLen;
						} while (actual_size > name_size-2);

						funcname[actual_size] = '\0';
						
						// end grab func name

						bt_str.append("in unmanaged function '");
						bt_str.append(funcname);
						bt_str.append("' (");
						bt_str.append(COLOR_MAGENTA);
						bt_str.append(name);
						bt_str.append(COLOR_END);
						bt_str.append(") @ ");

						delete[] name;
						delete[] funcinfobuf;
					}
				}

				bt_str.append(fmt_ptr.str());
				bt_str.append("...\n\t");

				continue;
			}

			Assembly* assembly = member->parent_type->assembly;

			std::stringstream fmt_ptr;

			fmt_ptr << COLOR_INTEGER << std::hex << bt[i] << COLOR_END;

			bt_str.append("in ");
			bt_str.append(
				GetDisplayNameOf(member)
			);
			bt_str.append(" (");
			bt_str.append(COLOR_MAGENTA);
			bt_str.append(assembly->name);
			bt_str.append(COLOR_END);
			bt_str.push_back(')');
			bt_str.append(" @ ");
			bt_str.append(fmt_ptr.str());
			bt_str.append("...\n\t");
		}
		
		bt_str.append(" ^ outermost frame\n");

		return bt_str;
	}
}