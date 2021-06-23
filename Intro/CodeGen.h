#ifndef CODEGEN_H
#define CODEGEN_H

#ifdef _MSC_VER
__pragma(warning(push))
__pragma(warning(disable:4355))
__pragma(warning(disable:4244))
#endif

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"
#include "JIT.h"

#ifdef _MSC_VER
__pragma(warning(pop))
#endif

#ifdef WIN32
	typedef unsigned __int64 __uint64;
#else
	typedef long long int __int64;
	typedef unsigned long long int __uint64;
#endif

#include "Type.h"
#include "Intro.h"

namespace intro 
{
	extern std::unique_ptr<llvm::LLVMContext> theContext;
	extern llvm::IRBuilder<> Builder;
	extern std::unique_ptr<llvm::Module> TheModule;
	
	void initRuntime(void);

	/*
	/// @name Enumerations mapping names to llvm struct slots for the definitions in runtime.h
	///@{
	/// Shared records components (no GC because they're actually global constants)
	enum SharedRecordParts
	{
		Size,			///< Number of labels in record (use 64 bit for uniform size of firs four elements, i.e. 4 bytes for pseudo-padding)
		ToString,		///< Pointer to function creating a string representation for records of this type
		Hash,			///< Pointer to function computing hash value for records of this type
		Equals,			///< Pointer to function comparing two records of this type
		Release,		///< Pointer to function releasing contained refereces when records of this type are deleted
		StartDisplace,	///< Index in structure of (first element in) displacement array (array of i32)
		SharedRecordPartCount
	};

	enum RecordParts
	{
		GCData,			///< 64 Bits of data available for garbage collection algorithm
		SharedDataPtr,	///< Pointer to shared data for this record type
		StartFields,		///< Index of first field in record (for variants, add one!!!)
		RecordPartCount ///< for variants, add one!!!
	};
/ *	enum DictionaryElementParts
	{
		ElemKey,		///< Key, boxed to 64 bit
		ElemHash,		///< Hash value of the key
		ElemValue,		///< Index of first field in record
		ElemNext,		///< Next hash in collision resolution list
		DictionaryElementPartCount
	};
* /
	enum DictionaryParts
	{
		// GCData=0,
		SlotCntPow=1,		///< Index of the value k so that the number of slots is the greatest prime less than 2^k.
		UsedCount,		///< Index of Number of slots used.
		HashesPtr,		///< Index of the pointer to the array of hash values for each slot.
		KeysPtr,		///< Index of the pointer to the array of keys for each slot.
		ValuesPtr,		///< Index of the pointer to the array of values for each slot.
		DictionaryPartCount
	};
*/	

	enum ClosureParts
	{
		// GCData=0,
		ClosureFunction=1, ///< Pointer to function holding code for closure
		ClosureFieldCount,
		ClosureFields
	};
	
	enum GeneratorParts
	{
		// GCData=0,
		GeneratorFunction=1,
		GeneratorClosureCount,
		GeneratorFieldCount,
		GeneratorRetVal,
		GeneratorRetValType,
		GeneratorState,
		GeneratorFields
	};
	///@}
	void executeStatement(Statement *stmt);
	/// Create a constant global string in the llvm module
	llvm::Constant *createGlobalString(const std::wstring &s);
	/// Construct a llvm::Type that  reprsents the given Type during runtime
	llvm::Type *toTypeLLVM(intro::Type *type);
	/// Create code for type specific unboxing of a value
	llvm::Value *createUnboxValue(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env,llvm::Value *field,intro::Type *type);
	/// Create code for type specific boxing of a value
	llvm::Value *createBoxValue(llvm::IRBuilder<> &TmpB,CodeGenEnvironment *env,llvm::Value *field,intro::Type *type);
}

#endif