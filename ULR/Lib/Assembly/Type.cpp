#include "../Assembly.hpp"
#include <map>
#include <iostream>

namespace ULR
{
	Type::Type(TypeType decl_type, char* name, int attrs, size_t size)
	{
		this->decl_type = decl_type;
		this->name = name;
		this->attrs = attrs;
		this->size = size;
	}

	void Type::AddStaticMember(MemberInfo* member)
	{
		static_attrs[member->name].emplace_back(member);
	}

	void Type::AddInstanceMember(MemberInfo* member)
	{
		inst_attrs[member->name].emplace_back(member);
	}

	Type::~Type()
	{
		free(name);
		
		for (auto& entry : static_attrs)
		{
			for (auto& member : entry.second)
			{
				delete member;
			}
		}

		for (auto& entry : inst_attrs)
		{
			for (auto& member : entry.second)
			{
				delete member;
			}
		}
	}
}