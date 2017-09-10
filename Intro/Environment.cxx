#include "stdafx.h"
#include "Environment.h"

long intro::Environment::_fresh=0;
std::set<intro::TypeVariable*> intro::Environment::_typevars;
std::set<intro::Type*> intro::Environment::_intermediates;

intro::Environment::Module intro::Environment::root; ///< The module holding the :: namespace
std::stack<intro::Environment::Module*> intro::Environment::current; ///< The stack of the current module, used for relative access
