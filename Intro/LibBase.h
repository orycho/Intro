#ifndef LIB_BASE_H
#define LIB_BASE_H

#include "Runtime.h"
#include "Type.h"
#include "CodeGenModule.h"
#include "Environment.h"

struct LibLoader
{
	struct elem
	{
		const wchar_t *name;
		const char *sourcename;
		intro::Type::pointer_t type;
	};
protected:
	std::vector<std::wstring> path;
	static std::vector<LibLoader *> instances;
	void load(const elem elems[])
	{
		size_t i = 0;
		intro::Environment::Module *ti_module = intro::Environment::getRootModule()->getCreatePath(path);
		intro::CodeGenModule *cg_module = intro::CodeGenModule::getRoot()->getRelativePath(path);
		intro::CodeGenEnvironment::element cgelem(nullptr, nullptr);
		while (elems[i].name != nullptr)
		{
			// Add to module
			ti_module->addExport(elems[i].name, elems[i].type, false);
			cg_module->importElement(elems[i].name, cgelem)->second.altname = elems[i].sourcename;
			++i;
		}
	}
public:
	LibLoader(std::wstring name)
	{
		path.push_back(name);
	};

	LibLoader(std::vector<std::wstring> &path_)
		: path(path_)
	{};
	virtual void create() = 0;
	static void initialize(void)
	{
		for (LibLoader *instance : instances) instance->create();
	};
};

#define MKCLOSURE(name,codeptr) rtclosure name ## _src = { \
	{false, gcdata::Octarine, 1}, \
	(void*)(codeptr), \
	1, { 0ll, 0} }; \
rtclosure * name = & name ## _src; \
rtt_t name ## _rtt = intro::rtt::Function;

#define EXPORT_CLOSURE(name) FORCE_EXPORT extern rtclosure *name; \
FORCE_EXPORT extern rtt_t name ## _rtt;

#define REGISTER_MODULE(name) struct name ## Loader_ : public LibLoader { \
static const elem elems[]; \
name ## Loader_(void) : LibLoader(L ## #name) {	instances.push_back(this); }; \
virtual void create() { load(elems); }; \
} name ## loader; \
const LibLoader::elem name ## Loader_::elems[] = {

#define EXPORT(name,closure,type) {name,closure,type},

#define CLOSE_MODULE { nullptr,nullptr,nullptr } }; 

#endif
