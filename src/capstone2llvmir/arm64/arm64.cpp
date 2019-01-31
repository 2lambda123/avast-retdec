/**
 * @file src/capstone2llvmir/arm64/arm64.cpp
 * @brief ARM64 implementation of @c Capstone2LlvmIrTranslator.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <iomanip>
#include <iostream>

#include "capstone2llvmir/arm64/arm64_impl.h"

namespace retdec {
namespace capstone2llvmir {

Capstone2LlvmIrTranslatorArm64_impl::Capstone2LlvmIrTranslatorArm64_impl(
		llvm::Module* m,
		cs_mode basic,
		cs_mode extra)
		:
		Capstone2LlvmIrTranslator_impl(CS_ARCH_ARM64, basic, extra, m),
		_reg2parentMap(ARM64_REG_ENDING, ARM64_REG_INVALID)
{
	initialize();
}

Capstone2LlvmIrTranslatorArm64_impl::~Capstone2LlvmIrTranslatorArm64_impl()
{

}

//
//==============================================================================
// Mode query & modification methods - from Capstone2LlvmIrTranslator.
//==============================================================================
//

bool Capstone2LlvmIrTranslatorArm64_impl::isAllowedBasicMode(cs_mode m)
{
	return m == CS_MODE_ARM;
}

bool Capstone2LlvmIrTranslatorArm64_impl::isAllowedExtraMode(cs_mode m)
{
	return m == CS_MODE_LITTLE_ENDIAN || m == CS_MODE_BIG_ENDIAN;
}

uint32_t Capstone2LlvmIrTranslatorArm64_impl::getArchByteSize()
{
	return 8;
}

//
//==============================================================================
// Pure virtual methods from Capstone2LlvmIrTranslator_impl
//==============================================================================
//

void Capstone2LlvmIrTranslatorArm64_impl::generateEnvironmentArchSpecific()
{
	initializeRegistersParentMap();
}

void Capstone2LlvmIrTranslatorArm64_impl::generateDataLayout()
{
	// clang -x c /dev/null -emit-llvm -S -o -
	_module->setDataLayout("e-m:e-i64:64-i128:128-n32:64-S128");
}

void Capstone2LlvmIrTranslatorArm64_impl::generateRegisters()
{
	// General purpose registers.
	//
	createRegister(ARM64_REG_X0, _regLt);
	createRegister(ARM64_REG_X1, _regLt);
	createRegister(ARM64_REG_X2, _regLt);
	createRegister(ARM64_REG_X3, _regLt);
	createRegister(ARM64_REG_X4, _regLt);
	createRegister(ARM64_REG_X5, _regLt);
	createRegister(ARM64_REG_X6, _regLt);
	createRegister(ARM64_REG_X7, _regLt);
	createRegister(ARM64_REG_X8, _regLt);
	createRegister(ARM64_REG_X9, _regLt);
	createRegister(ARM64_REG_X10, _regLt);
	createRegister(ARM64_REG_X11, _regLt);
	createRegister(ARM64_REG_X12, _regLt);
	createRegister(ARM64_REG_X13, _regLt);
	createRegister(ARM64_REG_X14, _regLt);
	createRegister(ARM64_REG_X15, _regLt);
	createRegister(ARM64_REG_X16, _regLt);
	createRegister(ARM64_REG_X17, _regLt);
	createRegister(ARM64_REG_X18, _regLt);
	createRegister(ARM64_REG_X19, _regLt);
	createRegister(ARM64_REG_X20, _regLt);
	createRegister(ARM64_REG_X21, _regLt);
	createRegister(ARM64_REG_X22, _regLt);
	createRegister(ARM64_REG_X23, _regLt);
	createRegister(ARM64_REG_X24, _regLt);
	createRegister(ARM64_REG_X25, _regLt);
	createRegister(ARM64_REG_X26, _regLt);
	createRegister(ARM64_REG_X27, _regLt);
	createRegister(ARM64_REG_X28, _regLt);

	// Lower 32 bits of 64 arm{xN} bit regs.
	//
	/*
	createRegister(ARM64_REG_W0, _regLt);
	createRegister(ARM64_REG_W1, _regLt);
	createRegister(ARM64_REG_W2, _regLt);
	createRegister(ARM64_REG_W3, _regLt);
	createRegister(ARM64_REG_W4, _regLt);
	createRegister(ARM64_REG_W5, _regLt);
	createRegister(ARM64_REG_W6, _regLt);
	createRegister(ARM64_REG_W7, _regLt);
	createRegister(ARM64_REG_W8, _regLt);
	createRegister(ARM64_REG_W9, _regLt);
	createRegister(ARM64_REG_W10, _regLt);
	createRegister(ARM64_REG_W11, _regLt);
	createRegister(ARM64_REG_W12, _regLt);
	createRegister(ARM64_REG_W13, _regLt);
	createRegister(ARM64_REG_W14, _regLt);
	createRegister(ARM64_REG_W15, _regLt);
	createRegister(ARM64_REG_W16, _regLt);
	createRegister(ARM64_REG_W17, _regLt);
	createRegister(ARM64_REG_W18, _regLt);
	createRegister(ARM64_REG_W19, _regLt);
	createRegister(ARM64_REG_W20, _regLt);
	createRegister(ARM64_REG_W21, _regLt);
	createRegister(ARM64_REG_W22, _regLt);
	createRegister(ARM64_REG_W23, _regLt);
	createRegister(ARM64_REG_W24, _regLt);
	createRegister(ARM64_REG_W25, _regLt);
	createRegister(ARM64_REG_W26, _regLt);
	createRegister(ARM64_REG_W27, _regLt);
	createRegister(ARM64_REG_W28, _regLt);
	createRegister(ARM64_REG_W29, _regLt);
	createRegister(ARM64_REG_W30, _regLt);
	*/

	// Special registers.

	// FP Frame pointer.
	createRegister(ARM64_REG_X29, _regLt);

	// LP Link register.
	createRegister(ARM64_REG_X30, _regLt);

	// Stack pointer.
	createRegister(ARM64_REG_SP, _regLt);
	//createRegister(ARM64_REG_WSP, _regLt);

	// Zero.
	createRegister(ARM64_REG_XZR, _regLt);
	//createRegister(ARM64_REG_WZR, _regLt);

	// Flags.
	createRegister(ARM64_REG_CPSR_N, _regLt);
	createRegister(ARM64_REG_CPSR_Z, _regLt);
	createRegister(ARM64_REG_CPSR_C, _regLt);
	createRegister(ARM64_REG_CPSR_V, _regLt);

	// Program counter.
	createRegister(ARM64_REG_PC, _regLt);
}

uint32_t Capstone2LlvmIrTranslatorArm64_impl::getCarryRegister()
{
	return ARM64_REG_CPSR_C;
}

void Capstone2LlvmIrTranslatorArm64_impl::translateInstruction(
		cs_insn* i,
		llvm::IRBuilder<>& irb)
{
	_insn = i;

	cs_detail* d = i->detail;
	cs_arm64* ai = &d->arm64;

	auto fIt = _i2fm.find(i->id);
	if (fIt != _i2fm.end() && fIt->second != nullptr)
	{
		auto f = fIt->second;

		if (ai->cc == ARM64_CC_AL || ai->cc == ARM64_CC_NV /* || branchInsn */)
		{
			_inCondition = false;
			(this->*f)(i, ai, irb);
		}
		else
		{
			(this->*f)(i, ai, irb);

			//_inCondition = true;
			//auto* cond = generateInsnConditionCode(irb, ai);
			//auto bodyIrb = generateIfThen(cond, irb);

			//(this->*f)(i, ai, bodyIrb);
		}
	}
	else
	{
		throwUnhandledInstructions(i);

		if (ai->cc == ARM64_CC_AL || ai->cc == ARM64_CC_INVALID)
		{
			_inCondition = false;
			translatePseudoAsmGeneric(i, ai, irb);
		}
		else
		{
			_inCondition = true;

			auto* cond = generateInsnConditionCode(irb, ai);
			auto bodyIrb = generateIfThen(cond, irb);

			translatePseudoAsmGeneric(i, ai, bodyIrb);
		}
	}
}

uint32_t Capstone2LlvmIrTranslatorArm64_impl::getParentRegister(uint32_t r) const
{
	return r < _reg2parentMap.size() ? _reg2parentMap[r] : r;
}

//
//==============================================================================
// ARM64-specific methods.
//==============================================================================
//

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::getCurrentPc(cs_insn* i)
{
	return llvm::ConstantInt::get(
			getDefaultType(),
			i->address + i->size);
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateOperandExtension(
		llvm::IRBuilder<>& irb,
		arm64_extender ext,
		llvm::Value* val,
		llvm::Type* destType)
{
	auto* i8  = llvm::IntegerType::getInt8Ty(_module->getContext());
	auto* i16 = llvm::IntegerType::getInt16Ty(_module->getContext());
	auto* i32 = llvm::IntegerType::getInt32Ty(_module->getContext());
	auto* i64 = llvm::IntegerType::getInt64Ty(_module->getContext());

	auto* ty  = destType ? destType : getDefaultType();

	llvm::Value* trunc = nullptr;
	switch(ext)
	{
		case ARM64_EXT_INVALID:
		{
			return val;
		}
		case ARM64_EXT_UXTB:
		{
			trunc = irb.CreateTrunc(val, i8);
			return irb.CreateZExt(trunc, ty);
		}
		case ARM64_EXT_UXTH:
		{
			trunc = irb.CreateTrunc(val, i16);
			return irb.CreateZExt(trunc, ty);
		}
		case ARM64_EXT_UXTW:
		{
			trunc = irb.CreateTrunc(val, i32);
			return irb.CreateZExt(trunc, ty);
		}
		case ARM64_EXT_UXTX:
		{
			trunc = irb.CreateTrunc(val, i64);
			return irb.CreateZExt(trunc, ty);
		}
		case ARM64_EXT_SXTB:
		{
			trunc = irb.CreateTrunc(val, i8);
			return irb.CreateSExt(trunc, ty);
		}
		case ARM64_EXT_SXTH:
		{
			trunc = irb.CreateTrunc(val, i16);
			return irb.CreateSExt(trunc, ty);
		}
		case ARM64_EXT_SXTW:
		{
			trunc = irb.CreateTrunc(val, i32);
			return irb.CreateSExt(trunc, ty);
		}
		case ARM64_EXT_SXTX:
		{
			trunc = irb.CreateTrunc(val, i64);
			return irb.CreateSExt(trunc, ty);
		}
		default:
		    assert(false && "generateOperandExtension(): Unsupported extension type");
	}
	return val;
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateOperandShift(
		llvm::IRBuilder<>& irb,
		cs_arm64_op& op,
		llvm::Value* val,
		bool updateFlags)
{
	llvm::Value* n = nullptr;
	if (op.shift.type == ARM64_SFT_INVALID)
	{
		return val;
	}
	else
	{
		n = llvm::ConstantInt::get(val->getType(), op.shift.value);
	}

	if (n == nullptr)
	{
		assert(false && "should not be possible");
		return val;
	}
	n = irb.CreateZExtOrTrunc(n, val->getType());

	switch (op.shift.type)
	{
		case ARM64_SFT_ASR:
		{
			return generateShiftAsr(irb, val, n, updateFlags);
		}
		case ARM64_SFT_LSL:
		{
			return generateShiftLsl(irb, val, n, updateFlags);
		}
		case ARM64_SFT_LSR:
		{
			return generateShiftLsr(irb, val, n, updateFlags);
		}
		case ARM64_SFT_ROR:
		{
			return generateShiftRor(irb, val, n, updateFlags);
		}
		case ARM64_SFT_MSL:
		{
			return generateShiftMsl(irb, val, n, updateFlags);
		}
		case ARM64_SFT_INVALID:
		default:
		{
			return val;
		}
	}
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateShiftAsr(
		llvm::IRBuilder<>& irb,
		llvm::Value* val,
		llvm::Value *n,
		bool updateFlags)
{
	if (updateFlags)
	{
		auto* cfOp1 = irb.CreateSub(n, llvm::ConstantInt::get(n->getType(), 1));
		auto* cfShl = irb.CreateShl(llvm::ConstantInt::get(cfOp1->getType(), 1), cfOp1);
		auto* cfAnd = irb.CreateAnd(cfShl, val);
		auto* cfIcmp = irb.CreateICmpNE(cfAnd, llvm::ConstantInt::get(cfAnd->getType(), 0));
		storeRegister(ARM64_REG_CPSR_C, cfIcmp, irb);
	}
	return irb.CreateAShr(val, n);
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateShiftLsl(
		llvm::IRBuilder<>& irb,
		llvm::Value* val,
		llvm::Value *n,
		bool updateFlags)
{
	if (updateFlags)
	{
		auto* cfOp1 = irb.CreateSub(n, llvm::ConstantInt::get(n->getType(), 1));
		auto* cfShl = irb.CreateShl(val, cfOp1);
		auto* cfIntT = llvm::cast<llvm::IntegerType>(cfShl->getType());
		auto* cfRightCount = llvm::ConstantInt::get(cfIntT, cfIntT->getBitWidth() - 1);
		auto* cfLow = irb.CreateLShr(cfShl, cfRightCount);
		storeRegister(ARM64_REG_CPSR_C, cfLow, irb);
	}
	return irb.CreateShl(val, n);
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateShiftLsr(
		llvm::IRBuilder<>& irb,
		llvm::Value* val,
		llvm::Value *n,
		bool updateFlags)
{
	if (updateFlags)
	{
		auto* cfOp1 = irb.CreateSub(n, llvm::ConstantInt::get(n->getType(), 1));
		auto* cfShl = irb.CreateShl(llvm::ConstantInt::get(cfOp1->getType(), 1), cfOp1);
		auto* cfAnd = irb.CreateAnd(cfShl, val);
		auto* cfIcmp = irb.CreateICmpNE(cfAnd, llvm::ConstantInt::get(cfAnd->getType(), 0));
		storeRegister(ARM64_REG_CPSR_C, cfIcmp, irb);
	}

	return irb.CreateLShr(val, n);
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateShiftRor(
		llvm::IRBuilder<>& irb,
		llvm::Value* val,
		llvm::Value *n,
		bool updateFlags)
{
	unsigned op0BitW = llvm::cast<llvm::IntegerType>(n->getType())->getBitWidth();

	auto* srl = irb.CreateLShr(val, n);
	auto* sub = irb.CreateSub(llvm::ConstantInt::get(n->getType(), op0BitW), n);
	auto* shl = irb.CreateShl(val, sub);
	auto* orr = irb.CreateOr(srl, shl);
	if (updateFlags)
	{

		auto* cfSrl = irb.CreateLShr(orr, llvm::ConstantInt::get(orr->getType(), op0BitW - 1));
		auto* cfIcmp = irb.CreateICmpNE(cfSrl, llvm::ConstantInt::get(cfSrl->getType(), 0));
		storeRegister(ARM64_REG_CPSR_C, cfIcmp, irb);
	}

	return orr;
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateShiftMsl(
		llvm::IRBuilder<>& irb,
		llvm::Value* val,
		llvm::Value *n,
		bool updateFlags)
{
	assert(false && "Check implementation");
	unsigned op0BitW = llvm::cast<llvm::IntegerType>(n->getType())->getBitWidth();
	auto* doubleT = llvm::Type::getIntNTy(_module->getContext(), op0BitW*2);

	auto* cf = loadRegister(ARM64_REG_CPSR_C, irb);
	cf = irb.CreateZExtOrTrunc(cf, n->getType());

	auto* srl = irb.CreateLShr(val, n);
	auto* srlZext = irb.CreateZExt(srl, doubleT);
	auto* op0Zext = irb.CreateZExt(val, doubleT);
	auto* sub = irb.CreateSub(llvm::ConstantInt::get(n->getType(), op0BitW + 1), n);
	auto* subZext = irb.CreateZExt(sub, doubleT);
	auto* shl = irb.CreateShl(op0Zext, subZext);
	auto* sub2 = irb.CreateSub(llvm::ConstantInt::get(n->getType(), op0BitW), n);
	auto* shl2 = irb.CreateShl(cf, sub2);
	auto* shl2Zext = irb.CreateZExt(shl2, doubleT);
	auto* or1 = irb.CreateOr(shl, srlZext);
	auto* or2 = irb.CreateOr(or1, shl2Zext);
	auto* or2Trunc = irb.CreateTrunc(or2, val->getType());

	auto* sub3 = irb.CreateSub(n, llvm::ConstantInt::get(n->getType(), 1));
	auto* shl3 = irb.CreateShl(llvm::ConstantInt::get(sub3->getType(), 1), sub3);
	auto* and1 = irb.CreateAnd(shl3, val);
	auto* cfIcmp = irb.CreateICmpNE(and1, llvm::ConstantInt::get(and1->getType(), 0));
	storeRegister(ARM64_REG_CPSR_C, cfIcmp, irb);

	return or2Trunc;
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateGetOperandMemAddr(
		cs_arm64_op& op,
		llvm::IRBuilder<>& irb)
{
	// TODO: generateGetOperandAddr?
	auto* baseR = loadRegister(op.mem.base, irb);
	auto* t = baseR ? baseR->getType() : getDefaultType();
	llvm::Value* disp = op.mem.disp
			? llvm::ConstantInt::get(t, op.mem.disp)
			: nullptr;

	auto* idxR = loadRegister(op.mem.index, irb);
	if (idxR)
	{
		idxR = generateOperandShift(irb, op, idxR);
	}

	llvm::Value* addr = nullptr;
	if (baseR && disp == nullptr)
	{
		addr = baseR;
	}
	else if (disp && baseR == nullptr)
	{
		addr = disp;
	}
	else if (baseR && disp)
	{
		disp = irb.CreateSExtOrTrunc(disp, baseR->getType());
		addr = irb.CreateAdd(baseR, disp);
	}
	else if (idxR)
	{
		addr = idxR;
	}
	else
	{
		addr = llvm::ConstantInt::get(getDefaultType(), 0);
	}

	if (idxR && addr != idxR)
	{
		idxR = irb.CreateZExtOrTrunc(idxR, addr->getType());
		addr = irb.CreateAdd(addr, idxR);
	}
	return addr;
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::loadRegister(
		uint32_t r,
		llvm::IRBuilder<>& irb,
		llvm::Type* dstType,
		eOpConv ct)
{
	if (r == ARM64_REG_INVALID)
	{
		return nullptr;
	}

	if (r == ARM64_REG_PC)
	{
		return getCurrentPc(_insn);
		// TODO: Check
	}

	auto* rt = getRegisterType(r);
	auto pr = getParentRegister(r);
	auto* llvmReg = getRegister(pr);
	if (llvmReg == nullptr)
	{
		//throw Capstone2LlvmIrError("loadRegister() unhandled reg.");
	}

	llvm::Value* ret = irb.CreateLoad(llvmReg);
	if (r != pr)
	{
	    ret = irb.CreateTrunc(ret, rt);
	}

	ret = generateTypeConversion(irb, ret, dstType, ct);
	return ret;
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::loadOp(
		cs_arm64_op& op,
		llvm::IRBuilder<>& irb,
		llvm::Type* ty,
		bool lea)
{
	switch (op.type)
	{
		case ARM64_OP_SYS:
		case ARM64_OP_REG:
		{
			auto* val = loadRegister(op.reg, irb);
			auto* ext = generateOperandExtension(irb, op.ext, val, ty);
			return generateOperandShift(irb, op, ext);
		}
		case ARM64_OP_IMM:
		{
			auto* val = llvm::ConstantInt::getSigned(getDefaultType(), op.imm);
			return generateOperandShift(irb, op, val);
		}
		case ARM64_OP_MEM:
		{
			auto* addr = generateGetOperandMemAddr(op, irb);

			auto* lty = ty ? ty : getDefaultType();
			auto* pt = llvm::PointerType::get(lty, 0);
			addr = irb.CreateIntToPtr(addr, pt);
			return irb.CreateLoad(addr);
		}
		case ARM64_OP_FP:
		case ARM64_OP_INVALID: 
		case ARM64_OP_CIMM: 
		case ARM64_OP_REG_MRS: 
		case ARM64_OP_REG_MSR: 
		case ARM64_OP_PSTATE: 
		case ARM64_OP_PREFETCH: 
		case ARM64_OP_BARRIER: 
		default:
		{
			assert(false && "loadOp(): unhandled operand type.");
			return nullptr;
		}
	}
}

llvm::Instruction* Capstone2LlvmIrTranslatorArm64_impl::storeRegister(
		uint32_t r,
		llvm::Value* val,
		llvm::IRBuilder<>& irb,
		eOpConv ct)
{
	if (r == ARM64_REG_INVALID)
	{
		return nullptr;
	}

	if (r == ARM64_REG_PC)
	{
		return nullptr;
		// TODO: Check?
	}

	auto* rt = getRegisterType(r);
	auto pr = getParentRegister(r);
	auto* llvmReg = getRegister(pr);
	if (llvmReg == nullptr)
	{
		//throw Capstone2LlvmIrError("storeRegister() unhandled reg.");
	}

	val = generateTypeConversion(irb, val, llvmReg->getValueType(), ct);

	llvm::StoreInst* ret = nullptr;
	if (r == pr
			// Zext for 64-bit target llvmRegs & 32-bit source regs.
			|| (getRegisterBitSize(pr) == 64 && getRegisterBitSize(r) == 32))
	{
		ret = irb.CreateStore(val, llvmReg);
	}
	else
	{
		llvm::Value* l = irb.CreateLoad(llvmReg);
		if (!(l->getType()->isIntegerTy(16)
				|| l->getType()->isIntegerTy(32)
				|| l->getType()->isIntegerTy(64)))
		{
			//throw Capstone2LlvmIrError("Unexpected parent type.");
		}

		llvm::Value* andC = nullptr;
		if (rt->isIntegerTy(32))
		{
			if (l->getType()->isIntegerTy(64))
			{
				andC = irb.getInt64(0xffffffff00000000);
			}
		}
		assert(andC);
		l = irb.CreateAnd(l, andC);

		auto* o = irb.CreateOr(l, val);
		ret = irb.CreateStore(o, llvmReg);
	}

	return ret;
}

llvm::Instruction* Capstone2LlvmIrTranslatorArm64_impl::storeOp(
		cs_arm64_op& op,
		llvm::Value* val,
		llvm::IRBuilder<>& irb,
		eOpConv ct)
{
	switch (op.type)
	{
		case ARM64_OP_SYS:
		case ARM64_OP_REG:
		{
			return storeRegister(op.reg, val, irb, ct);
		}
		case ARM64_OP_MEM:
		{
			auto* addr = generateGetOperandMemAddr(op, irb);

			auto* pt = llvm::PointerType::get(val->getType(), 0);
			addr = irb.CreateIntToPtr(addr, pt);
			return irb.CreateStore(val, addr);
		}
		case ARM64_OP_INVALID: 
		case ARM64_OP_IMM: 
		case ARM64_OP_FP:  
		case ARM64_OP_CIMM: 
		case ARM64_OP_REG_MRS: 
		case ARM64_OP_REG_MSR: 
		case ARM64_OP_PSTATE: 
		case ARM64_OP_PREFETCH: 
		case ARM64_OP_BARRIER: 
		default:
		{
			assert(false && "stroreOp(): unhandled operand type.");
			return nullptr;
		}
	}
}

llvm::Value* Capstone2LlvmIrTranslatorArm64_impl::generateInsnConditionCode(
		llvm::IRBuilder<>& irb,
		cs_arm64* ai)
{
	switch (ai->cc)
	{
		// Equal = Zero set
		case ARM64_CC_EQ:
		{
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			return z;
		}
		// Not equal = Zero clear
		case ARM64_CC_NE:
		{
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			return generateValueNegate(irb, z);
		}
		// Unsigned higher or same = Carry set
		case ARM64_CC_HS:
		{
			auto* c = loadRegister(ARM64_REG_CPSR_C, irb);
			return c;
		}
		// Unsigned lower = Carry clear
		case ARM64_CC_LO:
		{
			auto* c = loadRegister(ARM64_REG_CPSR_C, irb);
			return generateValueNegate(irb, c);
		}
		// Negative = N set
		case ARM64_CC_MI:
		{
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			return n;
		}
		// Positive or zero = N clear
		case ARM64_CC_PL:
		{
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			return generateValueNegate(irb, n);
		}
		// Overflow = V set
		case ARM64_CC_VS:
		{
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			return v;
		}
		// No overflow = V clear
		case ARM64_CC_VC:
		{
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			return generateValueNegate(irb, v);
		}
		// Unsigned higher = Carry set & Zero clear
		case ARM64_CC_HI:
		{
			auto* c = loadRegister(ARM64_REG_CPSR_C, irb);
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			auto* nz = generateValueNegate(irb, z);
			return irb.CreateAnd(c, nz);
		}
		// Unsigned lower or same = Carry clear or Zero set
		case ARM64_CC_LS:
		{
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			auto* c = loadRegister(ARM64_REG_CPSR_C, irb);
			auto* nc = generateValueNegate(irb, c);
			return irb.CreateOr(z, nc);
		}
		// Greater than or equal = N set and V set || N clear and V clear
		// (N & V) || (!N & !V) == !(N xor V)
		case ARM64_CC_GE:
		{
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			auto* x = irb.CreateXor(n, v);
			return generateValueNegate(irb, x);
		}
		// Less than = N set and V clear || N clear and V set
		// (N & !V) || (!N & V) == (N xor V)
		case ARM64_CC_LT:
		{
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			return irb.CreateXor(n, v);
		}
		// Greater than = Z clear, and either N set and V set, or N clear and V set
		case ARM64_CC_GT:
		{
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			auto* xor1 = irb.CreateXor(n, v);
			auto* or1 = irb.CreateOr(z, xor1);
			return generateValueNegate(irb, or1);
		}
		// Less than or equal = Z set, or N set and V clear, or N clear and V set
		case ARM64_CC_LE:
		{
			auto* z = loadRegister(ARM64_REG_CPSR_Z, irb);
			auto* n = loadRegister(ARM64_REG_CPSR_N, irb);
			auto* v = loadRegister(ARM64_REG_CPSR_V, irb);
			auto* xor1 = irb.CreateXor(n, v);
			return irb.CreateOr(z, xor1);
		}
		case ARM64_CC_AL:
		case ARM64_CC_NV:
		case ARM64_CC_INVALID:
		default:
		{
			throw GenericError("should not be possible");
		}
	}
}


bool Capstone2LlvmIrTranslatorArm64_impl::isOperandRegister(cs_arm64_op& op)
{
	return op.type == ARM64_OP_REG;
}

uint8_t Capstone2LlvmIrTranslatorArm64_impl::getOperandAccess(cs_arm64_op& op)
{
	return op.access;
}


//
//==============================================================================
// ARM64 instruction translation methods.
//==============================================================================
//

/**
 * ARM64_INS_ADD
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAdd(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	// In case of 32bit reg, trunc the imm
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto *val = irb.CreateAdd(op1, op2);
	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_SUB
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateSub(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	// In case of 32bit reg, trunc the imm
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto *val = irb.CreateSub(op1, op2);
	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_MOV, ARM64_INS_MVN, ARM64_INS_MOVZ
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMov(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	if (ai->op_count != 2)
	{
		return;
	}

	op1 = loadOpBinaryOp1(ai, irb);
	if (i->id == ARM64_INS_MVN)
	{
		op1 = generateValueNegate(irb, op1);
	}

	// If S is specified, the MOV instruction:
	// - updates the N and Z flags according to the result
	// - can update the C flag during the calculation of Operand2 (shifts?)
	// - does not affect the V flag.
	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(op1->getType(), 0);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(op1, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(op1, zero), irb);
	}
	storeOp(ai->operands[0], op1, irb);
}

/**
 * ARM64_INS_STR
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateStr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	op0 = loadOp(ai->operands[0], irb);
	auto* dest = generateGetOperandMemAddr(ai->operands[1], irb);

	auto* pt = llvm::PointerType::get(op0->getType(), 0);
	auto* addr = irb.CreateIntToPtr(dest, pt);
	irb.CreateStore(op0, addr);

	uint32_t baseR = ARM64_REG_INVALID;
	if (ai->op_count == 2)
	{
		baseR = ai->operands[1].reg;
	}
	else if (ai->op_count == 3)
	{
		baseR = ai->operands[1].reg;

		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[2].imm);
		dest = irb.CreateAdd(dest, disp);
		// post-index -> always writeback
	}
	else
	{
		assert(false && "unsupported STR format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, dest, irb);
	}
}

/**
 * ARM64_INS_STP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateStp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	op0 = loadOp(ai->operands[0], irb);
	op1 = loadOp(ai->operands[1], irb);

	uint32_t baseR = ARM64_REG_INVALID;
	llvm::Value* newDest = nullptr;
	auto* dest = generateGetOperandMemAddr(ai->operands[2], irb);
	auto* registerSize = llvm::ConstantInt::get(getDefaultType(), getRegisterByteSize(ai->operands[0].reg));
	storeOp(ai->operands[2], op0, irb);
	if (ai->op_count == 3)
	{
		newDest = irb.CreateAdd(dest, registerSize);

		auto* pt = llvm::PointerType::get(op1->getType(), 0);
		auto* addr = irb.CreateIntToPtr(newDest, pt);
		irb.CreateStore(op1, addr);

		baseR = ai->operands[2].mem.base;
	}
	else if (ai->op_count == 4)
	{
		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[3].imm);
		newDest    = irb.CreateAdd(dest, registerSize);

		auto* pt = llvm::PointerType::get(op1->getType(), 0);
		auto* addr = irb.CreateIntToPtr(newDest, pt);
		irb.CreateStore(op1, addr);

		baseR = ai->operands[2].mem.base;

		newDest = irb.CreateAdd(dest, disp);
	}
	else
	{
		assert(false && "unsupported STP format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, newDest, irb);
	}
}

/**
 * ARM64_INS_LDR
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateLdr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	auto* regType = getRegisterType(ai->operands[0].reg);
	auto* dest = generateGetOperandMemAddr(ai->operands[1], irb);
	auto* pt = llvm::PointerType::get(regType, 0);
	auto* addr = irb.CreateIntToPtr(dest, pt);

	auto* newRegValue = irb.CreateLoad(addr);
	storeRegister(ai->operands[0].reg, newRegValue, irb);

	uint32_t baseR = ARM64_REG_INVALID;
	if (ai->op_count == 2)
	{
		baseR = ai->operands[1].reg;
	}
	else if (ai->op_count == 3)
	{
		baseR = ai->operands[1].reg;

		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[2].imm);
		dest = irb.CreateAdd(dest, disp);
		// post-index -> always writeback
	}
	else
	{
		assert(false && "unsupported LDR format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, dest, irb);
	}
}

/**
 * ARM64_INS_LDP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateLdp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	auto* regType = getRegisterType(ai->operands[0].reg);
	auto* dest = generateGetOperandMemAddr(ai->operands[2], irb);
	auto* pt = llvm::PointerType::get(regType, 0);
	auto* addr = irb.CreateIntToPtr(dest, pt);
	auto* registerSize = llvm::ConstantInt::get(getDefaultType(), getRegisterByteSize(ai->operands[0].reg));

	auto* newReg1Value = irb.CreateLoad(addr);

	llvm::Value* newDest = nullptr;
	llvm::Value* newReg2Value = nullptr;
	uint32_t baseR = ARM64_REG_INVALID;
	if (ai->op_count == 3)
	{
		storeRegister(ai->operands[0].reg, newReg1Value, irb);
		newDest = irb.CreateAdd(dest, registerSize);
		addr = irb.CreateIntToPtr(newDest, pt);
		newReg2Value = irb.CreateLoad(addr);
		storeRegister(ai->operands[1].reg, newReg2Value, irb);

		baseR = ai->operands[2].mem.base;
	}
	else if (ai->op_count == 4)
	{

		storeRegister(ai->operands[0].reg, newReg1Value, irb);
		newDest = irb.CreateAdd(dest, registerSize);
		addr = irb.CreateIntToPtr(newDest, pt);
		newReg2Value = irb.CreateLoad(addr);
		storeRegister(ai->operands[1].reg, newReg2Value, irb);

		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[3].imm);
		dest = irb.CreateAdd(dest, disp);
		baseR = ai->operands[2].mem.base;
	}
	else
	{
		assert(false && "unsupported LDP format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, dest, irb);
	}
}

/**
 * ARM64_INS_ADRP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAdrp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	auto* imm  = loadOpBinaryOp1(ai, irb);
	auto* base = llvm::ConstantInt::get(getDefaultType(), (((i->address + i->size) >> 12) << 12));

	auto* res  = irb.CreateAdd(base, imm);

	storeRegister(ai->operands[0].reg, res, irb);
}

/**
 * ARM64_INS_BR
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateBr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	op0 = loadOpUnary(ai, irb);
	generateBranchFunctionCall(irb, op0);
}

/**
 * ARM64_INS_BL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateBl(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	storeRegister(ARM64_REG_LR, getNextInsnAddress(i), irb);
	op0 = loadOpUnary(ai, irb);
	generateCallFunctionCall(irb, op0);
}

/**
 * ARM64_INS_RET
*/
void Capstone2LlvmIrTranslatorArm64_impl::translateRet(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	op0 = loadRegister(ARM64_REG_LR, irb);
	generateReturnFunctionCall(irb, op0);
}

} // namespace capstone2llvmir
} // namespace retdec