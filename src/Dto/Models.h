#pragma once
#include <string>
#include <vector>
struct Models
{

	std::string name;
	std::vector<std::string> slugs;
	int64_t tokenlimit;
	bool operator<(const Models& other) const
	{
		return name < other.name;
	}
};
   

