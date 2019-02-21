/**
 * @file src/retdec/bin2llvmir/providers/calling_convention/calling_convention.cpp
 * @brief Calling convention information.
 * @copyright (c) 2019 Avast Software, licensed under the MIT license
 */


#include "retdec/bin2llvmir/providers/abi/abi.h"
#include "retdec/bin2llvmir/providers/calling_convention/calling_convention.h"
#include "retdec/bin2llvmir/providers/calling_convention/arm.h"
#include "retdec/bin2llvmir/providers/calling_convention/arm64.h"
#include "retdec/bin2llvmir/providers/calling_convention/mips.h"
#include "retdec/bin2llvmir/providers/calling_convention/mips64.h"
#include "retdec/bin2llvmir/providers/calling_convention/pic32.h"
#include "retdec/bin2llvmir/providers/calling_convention/powerpc.h"
#include "retdec/bin2llvmir/providers/calling_convention/powerpc64.h"
#include "retdec/bin2llvmir/providers/calling_convention/x86.h"
#include "retdec/bin2llvmir/providers/calling_convention/x64.h"

using namespace llvm;

namespace retdec {
namespace bin2llvmir {

//
//==============================================================================
// CallingConvention
//==============================================================================
//

const bool CallingConvention::RTL = true;
const bool CallingConvention::LTR = false;

CallingConvention::CallingConvention(
			const Abi* abi) :
		_abi(abi)
{
}

CallingConvention::~CallingConvention()
{
}

std::vector<uint32_t> CallingConvention::getParamRegisters() const
{
	return _paramRegs;
}

std::vector<uint32_t> CallingConvention::getParamFPRegisters() const
{
	return _paramFPRegs;
}

std::vector<uint32_t> CallingConvention::getParamDoubleRegisters() const
{
	return _paramDoubleRegs;
}

std::vector<uint32_t> CallingConvention::getParamVectorRegisters() const
{
	return _paramVectorRegs;
}

std::vector<uint32_t> CallingConvention::getReturnRegisters() const
{
	return _returnRegs;
}

std::vector<uint32_t> CallingConvention::getReturnFPRegisters() const
{
	return _returnFPRegs;
}

std::vector<uint32_t> CallingConvention::getReturnDoubleRegisters() const
{
	return _returnDoubleRegs;
}

std::vector<uint32_t> CallingConvention::getReturnVectorRegisters() const
{
	return _returnVectorRegs;
}

bool CallingConvention::usesFPRegistersForParameters() const
{
	return !(_paramFPRegs.empty() && _paramDoubleRegs.empty());
}

bool CallingConvention::parameterRegistersOverlay() const
{
	return _paramRegsOverlay;
}

std::size_t CallingConvention::getRegsNumPerParam() const
{
	return _regNumPerParam;
}

bool CallingConvention::getStackParamOrder() const
{
	return _stackParamOrder;
}

bool CallingConvention::passesStructsOnStack() const
{
	return _paramStructsOnStack;
}

std::size_t CallingConvention::getMaxBytesPerStackParam() const
{
	return _abi->getWordSize();
}

bool CallingConvention::valueCanBeParameter(const Value *val) const
{
	if (_abi->isStackVariable(val))
	{
		return true;
	}

	auto rId = _abi->getRegisterId(val);

	if (rId == Abi::REG_INVALID)
	{
		return false;
	}

	return std::count(_paramRegs.begin(), _paramRegs.end(), rId)
		|| std::count(_paramFPRegs.begin(), _paramFPRegs.end(), rId)
		|| std::count(_paramDoubleRegs.begin(), _paramDoubleRegs.end(), rId)
		|| std::count(_paramVectorRegs.begin(), _paramVectorRegs.end(), rId);
}

bool CallingConvention::canHoldReturnValue(const Value* val) const
{
	auto rId = _abi->getRegisterId(val);

	if (rId == Abi::REG_INVALID)
	{
		return false;
	}

	return std::count(_returnRegs.begin(), _returnRegs.end(), rId)
		|| std::count(_returnFPRegs.begin(), _returnFPRegs.end(), rId)
		|| std::count(_returnDoubleRegs.begin(), _returnDoubleRegs.end(), rId)
		|| std::count(_returnVectorRegs.begin(), _returnVectorRegs.end(), rId);
}

//
//==============================================================================
// CallingConventionProvider
//==============================================================================
//

CallingConventionProvider::CallingConventionProvider()
{
	registerCC(CallingConvention::ID::CC_CDECL, &CdeclCallingConvention::create);
	registerCC(CallingConvention::ID::CC_ELLIPSIS, &CdeclCallingConvention::create);
	registerCC(CallingConvention::ID::CC_STDCALL, &CdeclCallingConvention::create);
	registerCC(CallingConvention::ID::CC_THISCALL, &ThiscallCallingConvention::create);
	registerCC(CallingConvention::ID::CC_PASCAL, &PascalCallingConvention::create);
	registerCC(CallingConvention::ID::CC_FASTCALL, &FastcallCallingConvention::create);
	registerCC(CallingConvention::ID::CC_PASCAL_FASTCALL, &PascalFastcallCallingConvention::create);
	registerCC(CallingConvention::ID::CC_WATCOM, &WatcomCallingConvention::create);
	registerCC(CallingConvention::ID::CC_SYSTEMVX64, &SystemVX64CallingConvention::create);
	registerCC(CallingConvention::ID::CC_MICROSOFTX64, &MicrosoftX64CallingConvention::create);
	registerCC(CallingConvention::ID::CC_ARM, &ArmCallingConvention::create);
	registerCC(CallingConvention::ID::CC_ARM64, &Arm64CallingConvention::create);
	registerCC(CallingConvention::ID::CC_POWERPC, &PowerPCCallingConvention::create);
	registerCC(CallingConvention::ID::CC_POWERPC64, &PowerPC64CallingConvention::create);
	registerCC(CallingConvention::ID::CC_MIPS, &MipsCallingConvention::create);
	registerCC(CallingConvention::ID::CC_MIPSPSP, &MipsPSPCallingConvention::create);
	registerCC(CallingConvention::ID::CC_MIPS64, &Mips64CallingConvention::create);
	registerCC(CallingConvention::ID::CC_PIC32, &Pic32CallingConvention::create);
}

CallingConventionProvider::~CallingConventionProvider()
{
	_id2cc.clear();
}

CallingConventionProvider* CallingConventionProvider::getProvider()
{
	static CallingConventionProvider instance;

	return &instance;
}

void CallingConventionProvider::registerCC(
					const CallingConvention::ID& cc,
					const CallingConvention::ConstructorMethod& con)
{
	auto ccId = static_cast<size_t>(cc);

	if (ccId >= _id2cc.size()) 
	{
		_id2cc.resize(ccId+1, nullptr);
	}

	_id2cc[ccId] = con;
}

CallingConvention::Ptr CallingConventionProvider::createCallingConvention(
					const CallingConvention::ID& cc,
					const Abi* a) const
{
	auto ccId = static_cast<size_t>(cc);

	if (ccId >= _id2cc.size() || _id2cc[ccId] == nullptr)
	{
		return nullptr;
	}

	return _id2cc[ccId](a);
}

}
}