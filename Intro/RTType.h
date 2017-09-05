#ifndef RTTYPE_H
#define RTTYPE_H

#include <string>
#include <sstream>
#include <vector>


#ifdef _MSC_VER

__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))

#endif

//#include "llvm/Analysis/Passes.h"
//#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/JIT.h"
//#include "llvm/IR/DataLayout.h"
//#include "llvm/IR/DerivedTypes.h"
//#include "llvm/IR/IRBuilder.h"
//#include "llvm/IR/LLVMContext.h"
//#include "llvm/IR/Module.h"
//#include "llvm/PassManager.h"
//#include "llvm/Support/TargetSelect.h"
//#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/DataTypes.h"

#ifdef _MSC_VER

__pragma(warning(pop))

#endif

namespace intro
{

/// Run time type representation
/** Polymorphic functions as implemented by Intro require to be told the parameters' types
	by the call site, so that Variables in the function definition can be matched with the
	actual types provided by the call site. This allows the function to branch to the
	correct monomorphic realization of it's polymorphic definition.
	
	This is called intensional polymorphism.

	In order to not cmopletely destroy efficiency of generated code (cpu cache friendlinessw wanted), 
	during run time a flat type representation based on arrays of binary tags and lengths encoded in 
	16 bits:
	 - 12 Bits are available for the length, so types must not exceed 4096 tags in representation.
	 - 4 bits hold the type - this is always a concrete type with an actual implementation
	May be a problem for large records and variants.
*/
namespace rtt
{
	/// Runtime type identifiers, at most 16 values allowed (4 bits).
	/** Maybe use empty record to represent builtin types?
		Or use a variant with a single (internal) tag type, like {[@RegExp: ...]} or so.
		This would allow adding members tp builtins.
		Or do that while using special builtin type, with label and record like (public) members.
	*/
	enum RTType 
	{
		//Top, ///< Used only inside types generated during subtype expansion?!
		Undefined,	// OCL: I like this to be 0
		Function, 
		Record, 
		Variant, 
		Generator, // (A)
		String, 
		List, // (A)
		Dictionary, // (K,V)
		Integer, 
		Real, 
		Boolean,
		BuiltIn,	///< Library opaque types with a tag indicated by label and a numbr of parameters? Supertype to record to allow members as well Pair(A,B)<:[first:A;second:B;]?
		Label,	///< Used by record types to indicate a label and by variants to indicate a tag, (builtins to indicate type)
		ParamCount	///< Used by function types to indicate number of parameters
	};

	typedef uint16_t tag_t;
	typedef std::vector<tag_t> encoding;
	inline tag_t makeTag(RTType t,size_t length) { return (tag_t)((length<<4)|t); }
	inline RTType getType(tag_t tag) { return (RTType)(tag&(tag_t)0x0f); }
	inline size_t getLength(tag_t tag) { return ((size_t)tag)>>4; }

	inline void addLabel(encoding &result,const std::wstring &label)
	{
		result.push_back(makeTag(Label,label.size()+1));
		for (size_t i=0;i<label.size();++i) result.push_back(label[i]);
	}
	
	inline void addParamCount(encoding &result,size_t count) 
	{
		size_t lentags=sizeof(size_t)/sizeof(tag_t);
		result.push_back(makeTag(ParamCount,1+lentags));
		size_t localcount=count;
		tag_t *tpc=(tag_t*)&localcount;
		for (size_t i=0;i<lentags;++i)
			result.push_back(tpc[i]);
	}

	/// extract a 64bit parameter count - maybe limit to 255 parameters...that should be enough... or use a tag!!!!!
	inline size_t getParamCount(const encoding &src,size_t &offset)
	{
		size_t lentags=sizeof(size_t)/sizeof(tag_t);
		//assert(src[offset]==makeTag(ParamCount,1+lentags));
		++offset;
		size_t localcount=0;
		tag_t *tpc=(tag_t*)&localcount;
		for (size_t i=0;i<lentags;++i,++offset)
			tpc[i]=src[offset];
		return localcount;
	}

	inline const wchar_t *getRTTypeName(RTType type) 
	{
		switch(type)
		{
		//case Top:
		//	return L"Top";
		//	break;
		case Function:
			return L"Function";
			break;
		case Record:
			return L"Record";
			break;
		case Variant:
			return L"Variant";
			break;
		case Generator:
			return L"Generator";
			break;
		case String:
			return L"String";
			break;
		case List:
			return L"List";
			break;
		case Dictionary:
			return L"Dictionary";
			break;
		case Integer:
			return L"Integer";
			break;
		case Real:
			return L"Real";
			break;
		case Boolean:
			return L"Boolean";
			break;
		default: return L"Unknown";
		}
	}

	inline std::wstring print(tag_t tag)
	{
		std::wstringstream ws;
		ws<<(uint16_t)getLength(tag)<<getRTTypeName(getType(tag));
		return ws.str();
	}

	
}

}
#endif