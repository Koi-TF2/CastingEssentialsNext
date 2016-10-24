#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class IClientEntity;
class RecvTable;
class RecvProp;

class Entities final
{
public:
	static bool RetrieveClassPropOffset(const std::string& className, const std::string& propertyString, const std::vector<std::string>& propertyTree);
	__forceinline static bool RetrieveClassPropOffset(const std::string& className, const std::vector<std::string>& propertyTree)
	{
		return RetrieveClassPropOffset(className, ConvertTreeToString(propertyTree), propertyTree);
	}

	template<typename T> __forceinline static T GetEntityProp(IClientEntity* entity, const std::vector<std::string>& propertyTree)
	{
		return reinterpret_cast<T>(GetEntityProp(entity, propertyTree));
	}

private:
	Entities() { }
	~Entities() { }

	static std::string ConvertTreeToString(const std::vector<std::string>& tree);

	static bool GetSubProp(RecvTable* table, const char* propName, RecvProp*& prop, int& offset);
	static void* GetEntityProp(IClientEntity* entity, const std::vector<std::string>& propertyTree);

	static std::unordered_map<std::string, std::unordered_map<std::string, int>> s_ClassPropOffsets;
};