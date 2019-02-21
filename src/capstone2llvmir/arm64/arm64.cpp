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
	for (auto& p : _reg2type)
	{
		createRegister(p.first, _regLt);
	}
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

	//std::cout << i->mnemonic << " " << i->op_str << std::endl;

	auto fIt = _i2fm.find(i->id);
	if (fIt != _i2fm.end() && fIt->second != nullptr)
	{
		auto f = fIt->second;

		(this->*f)(i, ai, irb);
	}
	else
	{
		throwUnhandledInstructions(i);

		if (ai->cc == ARM64_CC_AL
		    || ai->cc == ARM64_CC_INVALID)
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
			throw GenericError("Arm64: generateOperandExtension(): Unsupported extension type");
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
	// TODO: Check the operand types
	// TODO: If the type is IMM return load of that value, or variable
	// TODO: name, maybe generateGetOperandValue?
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
		throw GenericError("loadRegister() unhandled reg.");
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

			if (lea)
			{
				return addr;
			}
			else
			{
				auto* lty = ty ? ty : getDefaultType();
				auto* pt = llvm::PointerType::get(lty, 0);
				addr = irb.CreateIntToPtr(addr, pt);
				return irb.CreateLoad(addr);
			}

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
			throw GenericError("Arm64: loadOp(): unhandled operand type");
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
		throw GenericError("storeRegister() unhandled reg.");
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
			throw GenericError("Unexpected parent type.");
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
			throw GenericError("storeOp(): unhandled operand type");
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
			throw GenericError("Probably wrong condition code.");
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

bool Capstone2LlvmIrTranslatorArm64_impl::isCondIns(cs_arm64 * i) {
    return (i->cc == ARM64_CC_AL || i->cc == ARM64_CC_INVALID) ? false : true;
}

//
//==============================================================================
// ARM64 instruction translation methods.
//==============================================================================
//

/**
 * ARM64_INS_ADC
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAdc(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());
	auto* carry = loadRegister(ARM64_REG_CPSR_C, irb);

	auto* val = irb.CreateAdd(op1, op2);
	val       = irb.CreateAdd(val, irb.CreateZExtOrTrunc(carry, val->getType()));

	storeOp(ai->operands[0], val, irb);

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateCarryAddC(op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowAddC(val, op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_ADD, ARM64_INS_CMN
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAdd(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto *val = irb.CreateAdd(op1, op2);
	if (i->id != ARM64_INS_CMN)
	{
		storeOp(ai->operands[0], val, irb);
	}

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateCarryAdd(val, op1, irb), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowAdd(val, op1, op2, irb), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_SUB, ARM64_INS_CMP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateSub(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto *val = irb.CreateSub(op1, op2);
	if (i->id != ARM64_INS_CMP)
	{
		storeOp(ai->operands[0], val, irb);
	}

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateValueNegate(irb, generateBorrowSub(op1, op2, irb)), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowSub(val, op1, op2, irb), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_NEG
 * ARM64_INS_NEGS for some reason capstone includes this instruction as alias.
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateNeg(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	auto* op2 = loadOpBinaryOp1(ai, irb);
	llvm::Value* zero = llvm::ConstantInt::get(op2->getType(), 0);

	auto* val = irb.CreateSub(zero, op2);
	storeOp(ai->operands[0], val, irb);

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateValueNegate(irb, generateBorrowSub(zero, op2, irb)), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowSub(val, zero, op2, irb), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_SBC
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateSbc(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);

	auto* carry = loadRegister(ARM64_REG_CPSR_C, irb);

	// NOT(OP2)
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());
	op2 = generateValueNegate(irb, op2);

	// OP1 + NOT(OP2) + CARRY
	auto* val = irb.CreateAdd(op1, op2);
	val       = irb.CreateAdd(val, irb.CreateZExtOrTrunc(carry, val->getType()));

	storeOp(ai->operands[0], val, irb);

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateCarryAddC(op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowAddC(val, op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_NGC, ARM64_INS_NGCS
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateNgc(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	auto* op2 = loadOpBinaryOp1(ai, irb);
	llvm::Value* op1 = llvm::ConstantInt::get(op2->getType(), 0);
	auto* carry = loadRegister(ARM64_REG_CPSR_C, irb);

	// NOT(OP2)
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());
	op2 = generateValueNegate(irb, op2);

	// OP1 + NOT(OP2) + CARRY
	auto* val = irb.CreateAdd(op1, op2);
	val       = irb.CreateAdd(val, irb.CreateZExtOrTrunc(carry, val->getType()));

	storeOp(ai->operands[0], val, irb);

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateCarryAddC(op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowAddC(val, op1, op2, irb, carry), irb);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
	}
}

/**
 * ARM64_INS_MOV, ARM64_INS_MVN, ARM64_INS_MOVZ
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMov(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	op1 = loadOpBinaryOp1(ai, irb);
	if (i->id == ARM64_INS_MVN)
	{
		op1 = generateValueNegate(irb, op1);
	}

	storeOp(ai->operands[0], op1, irb);
}

/**
 * ARM64_INS_STR, ARM64_INS_STRB, ARM64_INS_STRH
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateStr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	llvm::Type* ty = nullptr;
	switch (i->id)
	{
		case ARM64_INS_STR:
		{
			ty = getDefaultType();
			break;
		}
		case ARM64_INS_STRB:
		{
			ty = irb.getInt8Ty();
			break;
		}
		case ARM64_INS_STRH:
		{
			ty = irb.getInt16Ty();
			break;
		}
		default:
		{
			throw GenericError("Arm64: unhandled STR id");
		}
	}

	op0 = loadOp(ai->operands[0], irb);
	op0 = irb.CreateZExtOrTrunc(op0, ty);
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
		throw GenericError("STR: unsupported STR format");
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
	EXPECT_IS_EXPR(i, ai, irb, (2 <= ai->op_count && ai->op_count <= 4));

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
		throw GenericError("STR: unsupported STP format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, newDest, irb);
	}
}

/**
 * ARM64_INS_LDR
 * ARM64_INS_LDURB, ARM64_INS_LDUR, ARM64_INS_LDURH, ARM64_INS_LDURSB, ARM64_INS_LDURSH, ARM64_INS_LDURSW
 * ARM64_INS_LDRB, ARM64_INS_LDRH, ARM64_INS_LDRSB, ARM64_INS_LDRSH, ARM64_INS_LDRSW
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateLdr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	llvm::Type* ty = nullptr;
	bool sext = false;
	switch (i->id)
	{
		case ARM64_INS_LDR:
		case ARM64_INS_LDUR:
		{
			ty = irb.getInt32Ty();
			sext = false;
			break;
		}
		case ARM64_INS_LDRB:
		case ARM64_INS_LDURB:
		{
			ty = irb.getInt8Ty();
			sext = false;
			break;
		}
		case ARM64_INS_LDRH:
		case ARM64_INS_LDURH:
		{
			ty = irb.getInt16Ty();
			sext = false;
			break;
		}
		// Signed loads
		case ARM64_INS_LDRSB:
		case ARM64_INS_LDURSB:
		{
			ty = irb.getInt8Ty();
			sext = true;
			break;
		}
		case ARM64_INS_LDRSH:
		case ARM64_INS_LDURSH:
		{
			ty = irb.getInt16Ty();
			sext = true;
			break;
		}
		case ARM64_INS_LDRSW:
		case ARM64_INS_LDURSW:
		{
			ty = irb.getInt32Ty();
			sext = true;
			break;
		}
		default:
		{
			throw GenericError("Arm64: unhandled LDR id");
		}
	}

	auto* regType = getRegisterType(ai->operands[0].reg);
	auto* dest = loadOp(ai->operands[1], irb, nullptr, true);
	auto* pt = llvm::PointerType::get(ty, 0);
	auto* addr = irb.CreateIntToPtr(dest, pt);

	auto* loaded_value = irb.CreateLoad(addr);
	auto* ext_value    = sext
			? irb.CreateSExtOrTrunc(loaded_value, regType)
			: irb.CreateZExtOrTrunc(loaded_value, regType);

	storeRegister(ai->operands[0].reg, ext_value, irb);

	uint32_t baseR = ARM64_REG_INVALID;
	if (ai->op_count == 2)
	{
		baseR = ai->operands[1].reg;
	}
	else if (ai->op_count == 3) // POST-index
	{
		baseR = ai->operands[1].reg;

		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[2].imm);
		dest = irb.CreateAdd(dest, disp);
	}
	else
	{
		throw GenericError("Arm64: unsupported ldr format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, dest, irb);
	}
}

/**
 * ARM64_INS_LDP, ARM64_INS_LDPSW
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateLdp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_EXPR(i, ai, irb, (2 <= ai->op_count && ai->op_count <= 4));

	llvm::Value* data_size = nullptr;
	llvm::Type* ty = nullptr;
	eOpConv ct = eOpConv::THROW;
	if(i->id == ARM64_INS_LDP)
	{
		data_size = llvm::ConstantInt::get(getDefaultType(), getRegisterByteSize(ai->operands[0].reg));
		ty = getRegisterType(ai->operands[0].reg);
		ct = eOpConv::ZEXT_TRUNC;
	}
	else if(i->id == ARM64_INS_LDPSW)
	{
		data_size = llvm::ConstantInt::get(getDefaultType(), 4);
		ty = irb.getInt32Ty();
		ct = eOpConv::SEXT_TRUNC;
	}
	else
	{
		throw GenericError("ldp, ldpsw: Instruction id error");
	}

	auto* dest = loadOp(ai->operands[2], irb, nullptr, true);
	auto* pt = llvm::PointerType::get(ty, 0);
	auto* addr = irb.CreateIntToPtr(dest, pt);

	auto* newReg1Value = irb.CreateLoad(addr);

	llvm::Value* newDest = nullptr;
	llvm::Value* newReg2Value = nullptr;
	uint32_t baseR = ARM64_REG_INVALID;
	if (ai->op_count == 3)
	{
		storeRegister(ai->operands[0].reg, newReg1Value, irb, ct);
		newDest = irb.CreateAdd(dest, data_size);
		addr = irb.CreateIntToPtr(newDest, pt);
		newReg2Value = irb.CreateLoad(addr);
		storeRegister(ai->operands[1].reg, newReg2Value, irb, ct);

		baseR = ai->operands[2].mem.base;
	}
	else if (ai->op_count == 4)
	{

		storeRegister(ai->operands[0].reg, newReg1Value, irb, ct);
		newDest = irb.CreateAdd(dest, data_size);
		addr = irb.CreateIntToPtr(newDest, pt);
		newReg2Value = irb.CreateLoad(addr);
		storeRegister(ai->operands[1].reg, newReg2Value, irb, ct);

		auto* disp = llvm::ConstantInt::get(getDefaultType(), ai->operands[3].imm);
		dest = irb.CreateAdd(dest, disp);
		baseR = ai->operands[2].mem.base;
	}
	else
	{
		throw GenericError("ldp, ldpsw: Unsupported instruction format");
	}

	if (ai->writeback && baseR != ARM64_REG_INVALID)
	{
		storeRegister(baseR, dest, irb);
	}
}

/**
 * ARM64_INS_ADR, ARM64_INS_ADRP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAdr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	auto* imm  = loadOpBinaryOp1(ai, irb);

	// Even though the semantics for this instruction is
	// base = PC[]
	// X[t] = base + imm
	// It looks like capstone is already doing this work for us and
	// second operand has calculated value already
	/*
	auto* base = loadRegister(ARM64_REG_PC, irb);
	// ADRP loads address to 4KB page
	if (i->id == ARM64_INS_ADRP)
	{
		base = llvm::ConstantInt::get(getDefaultType(), (((i->address + i->size) >> 12) << 12));
	}
	auto* res  = irb.CreateAdd(base, imm);
	*/

	storeRegister(ai->operands[0].reg, imm, irb);
}

/**
 * ARM64_INS_AND, ARM64_INS_TST
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateAnd(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto* val = irb.CreateAnd(op1, op2);

	if (i->id != ARM64_INS_TST)
	{
		storeOp(ai->operands[0], val, irb);
	}

	if (ai->update_flags)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_N, irb.CreateICmpSLT(val, zero), irb);
		storeRegister(ARM64_REG_CPSR_Z, irb.CreateICmpEQ(val, zero), irb);
		// According to documentation carry and overflow should be
		// set to zero.
		storeRegister(ARM64_REG_CPSR_C, zero, irb);
		storeRegister(ARM64_REG_CPSR_V, zero, irb);
	}
}

/**
 * ARM64_INS_ASR
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateShifts(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	llvm::Value* val = nullptr;
	switch(i->id)
	{
		// TODO: Other shifts
		case ARM64_INS_ASR:
		{
			val = irb.CreateAShr(op1, op2);
			break;
		}
		case ARM64_INS_LSL:
		{
			val = irb.CreateShl(op1, op2);
			break;
		}
		case ARM64_INS_LSR:
		{
			val = irb.CreateLShr(op1, op2);
			break;
		}
		case ARM64_INS_ROR:
		{
			val = generateShiftRor(irb, op1, op2);
			break;
		}
		default:
		{
			throw GenericError("Shifts: unhandled insn ID");
		}
	}

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_BR, ARM64_INS_BRL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateBr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_UNARY(i, ai, irb);

	// Branch with link to register
	if (i->id == ARM64_INS_BLR)
	{
		storeRegister(ARM64_REG_LR, getNextInsnAddress(i), irb);
	}

	op0 = loadOpUnary(ai, irb);
	generateBranchFunctionCall(irb, op0);
}

/**
 * ARM64_INS_B
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateB(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_UNARY(i, ai, irb);
    
	op0 = loadOpUnary(ai, irb);

	if (isCondIns(ai)) {
		auto* cond = generateInsnConditionCode(irb, ai);
		generateCondBranchFunctionCall(irb, cond, op0);
	}
	else
	{
		generateBranchFunctionCall(irb, op0);
	}
}

/**
 * ARM64_INS_BL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateBl(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_UNARY(i, ai, irb);
    
	storeRegister(ARM64_REG_LR, getNextInsnAddress(i), irb);
	op0 = loadOpUnary(ai, irb);
	generateCallFunctionCall(irb, op0);
}

/**
 * ARM64_INS_CBNZ, ARM64_INS_CBZ
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCbnz(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	std::tie(op0, op1) = loadOpBinary(ai, irb);
	llvm::Value* cond = nullptr;
	if (i->id == ARM64_INS_CBNZ)
	{
		cond = irb.CreateICmpNE(op0, llvm::ConstantInt::get(op0->getType(), 0));
	}
	else if (i->id == ARM64_INS_CBZ)
	{
		cond = irb.CreateICmpEQ(op0, llvm::ConstantInt::get(op0->getType(), 0));
	}
	else
	{
		throw GenericError("cbnz, cbz: Instruction id error");
	}
	generateCondBranchFunctionCall(irb, cond, op1);
}

/**
 * ARM64_INS_CCMN, ARM64_INS_CCMP
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCondCompare(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	op1 = loadOp(ai->operands[0], irb);
	op2 = loadOp(ai->operands[1], irb);
	auto* nzvc = loadOp(ai->operands[2], irb);

	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	auto* cond = generateInsnConditionCode(irb, ai);
	auto irbP = generateIfThenElse(cond, irb);
	llvm::IRBuilder<>& bodyIf(irbP.first), bodyElse(irbP.second);

	//IF - condition holds
	llvm::Value* val = nullptr;
	if (i->id == ARM64_INS_CCMP)
	{
		val = bodyIf.CreateSub(op1, op2);
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateValueNegate(bodyIf, generateBorrowSub(op1, op2, bodyIf)), bodyIf);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowSub(val, op1, op2, bodyIf), bodyIf);
		storeRegister(ARM64_REG_CPSR_N, bodyIf.CreateICmpSLT(val, zero), bodyIf);
		storeRegister(ARM64_REG_CPSR_Z, bodyIf.CreateICmpEQ(val, zero), bodyIf);
	}
	else if (i->id == ARM64_INS_CCMN)
	{
		val = bodyIf.CreateAdd(op1, op2);
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		storeRegister(ARM64_REG_CPSR_C, generateCarryAdd(val, op1, bodyIf), bodyIf);
		storeRegister(ARM64_REG_CPSR_V, generateOverflowAdd(val, op1, op2, bodyIf), bodyIf);
		storeRegister(ARM64_REG_CPSR_N, bodyIf.CreateICmpSLT(val, zero), bodyIf);
		storeRegister(ARM64_REG_CPSR_Z, bodyIf.CreateICmpEQ(val, zero), bodyIf);
	}
	else
	{
		throw GenericError("Arm64 ccmp, ccmn: Instruction id error");
	}

	//ELSE - Set the flags from IMM
	// We only use shifts because the final value to be stored is truncated to i1.
	storeRegister(ARM64_REG_CPSR_N, bodyElse.CreateLShr(nzvc, llvm::ConstantInt::get(nzvc->getType(), 3)), bodyElse);
	storeRegister(ARM64_REG_CPSR_Z, bodyElse.CreateLShr(nzvc, llvm::ConstantInt::get(nzvc->getType(), 2)), bodyElse);
	storeRegister(ARM64_REG_CPSR_C, bodyElse.CreateLShr(nzvc, llvm::ConstantInt::get(nzvc->getType(), 1)), bodyElse);
	storeRegister(ARM64_REG_CPSR_V, nzvc, bodyElse);

}

/**
 * ARM64_INS_CSEL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCsel(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);

	auto* cond = generateInsnConditionCode(irb, ai);
	auto* val  = irb.CreateSelect(cond, op1, op2);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_CINC, ARM64_INS_CINV, ARM64_INS_CNEG
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCondOp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	op1 = loadOp(ai->operands[1], irb);

	auto* cond = generateInsnConditionCode(irb, ai);
	// Invert the condition
	cond = generateValueNegate(irb, cond);
	auto irbP = generateIfThenElse(cond, irb);
	llvm::IRBuilder<>& bodyIf(irbP.first), bodyElse(irbP.second);

	//IF - store first operand
	storeOp(ai->operands[0], op1, bodyIf);

	//ELSE
	llvm::Value *val = nullptr;
	switch(i->id)
	{
	case ARM64_INS_CINC:
		val = bodyElse.CreateAdd(op1, llvm::ConstantInt::get(op1->getType(), 1));
		break;
	case ARM64_INS_CINV:
		val = generateValueNegate(bodyElse, op1);
		break;
	case ARM64_INS_CNEG:
		val = generateValueNegate(bodyElse, op1);
		val = bodyElse.CreateAdd(val, llvm::ConstantInt::get(val->getType(), 1));
		//TODO: Express this as: (zero - op1) ?
		//llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		//val = irb.CreateSub(zero, val);
		break;
	default:
		throw GenericError("translateCondOp: Instruction id error");
		break;
	}
	storeOp(ai->operands[0], val, bodyElse);
	//ENDIF
}

/**
 * ARM64_INS_CSINC, ARM64_INS_CSINV, ARM64_INS_CSNEG
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCondSelOp(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	op1 = loadOp(ai->operands[1], irb);
	op2 = loadOp(ai->operands[2], irb);

	auto* cond = generateInsnConditionCode(irb, ai);
	auto irbP = generateIfThenElse(cond, irb);
	llvm::IRBuilder<>& bodyIf(irbP.first), bodyElse(irbP.second);

	//IF
	storeOp(ai->operands[0], op1, bodyIf);

	//ELSE
	llvm::Value *val = nullptr;
	switch(i->id)
	{
	case ARM64_INS_CSINC:
		val = bodyElse.CreateAdd(op2, llvm::ConstantInt::get(op2->getType(), 1));
		break;
	case ARM64_INS_CSINV:
		val = generateValueNegate(bodyElse, op2);
		break;
	case ARM64_INS_CSNEG:
		val = generateValueNegate(bodyElse, op2);
		val = bodyElse.CreateAdd(val, llvm::ConstantInt::get(val->getType(), 1));
		//TODO: Express this as: (zero - op2) ?
		//llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		//val = irb.CreateSub(zero, val);
		break;
	default:
		throw GenericError("translateCondSelOp: Instruction id error");
		break;
	}
	storeOp(ai->operands[0], val, bodyElse);
	//ENDIF
}

/**
 * ARM64_INS_CSET, ARM64_INS_CSETM
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateCset(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_UNARY(i, ai, irb);

	auto* rt = getRegisterType(ai->operands[0].reg);
	auto* zero = llvm::ConstantInt::get(rt, 0);
	llvm::Value* one = nullptr;
	if (i->id == ARM64_INS_CSET)
	{
		one = llvm::ConstantInt::get(rt, 1);
	}
	else if (i->id == ARM64_INS_CSETM)
	{
		one = llvm::ConstantInt::get(rt, ~0);
		// 0xffffffffffffffff - one in all bits
	}
	else
	{
		throw GenericError("cset, csetm: Instruction id error");
	}

	auto* cond = generateInsnConditionCode(irb, ai);
	auto* val  = irb.CreateSelect(cond, one, zero);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_EOR, ARM64_INS_EON
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateEor(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	if (i->id == ARM64_INS_EON)
	{
	    op2 = generateValueNegate(irb, op2);
	}

	auto* val = irb.CreateXor(op1, op2);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_SXTB, ARM64_INS_SXTH, ARM64_INS_SXTW
 * ARM64_INS_UXTB, ARM64_INS_UXTH
*/
void Capstone2LlvmIrTranslatorArm64_impl::translateExtensions(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY(i, ai, irb);

	auto* val = loadOp(ai->operands[1], irb);

	auto* i8  = llvm::IntegerType::getInt8Ty(_module->getContext());
	auto* i16 = llvm::IntegerType::getInt16Ty(_module->getContext());
	auto* i32 = llvm::IntegerType::getInt32Ty(_module->getContext());

	auto* ty  = getRegisterType(ai->operands[0].reg);

	llvm::Value* trunc = nullptr;
	switch(i->id)
	{
		case ARM64_INS_UXTB:
		{
			trunc = irb.CreateTrunc(val, i8);
			val   = irb.CreateZExt(trunc, ty);
			break;
		}
		case ARM64_INS_UXTH:
		{
			trunc = irb.CreateTrunc(val, i16);
			val   = irb.CreateZExt(trunc, ty);
			break;
		}
		/*
		case ARM64_INS_UXTW:
		{
			trunc = irb.CreateTrunc(val, i32);
			val   = irb.CreateZExt(trunc, ty);
			break;
		}
		*/
		case ARM64_INS_SXTB:
		{
			trunc = irb.CreateTrunc(val, i8);
			val   = irb.CreateSExt(trunc, ty);
			break;
		}
		case ARM64_INS_SXTH:
		{
			trunc = irb.CreateTrunc(val, i16);
			val   = irb.CreateSExt(trunc, ty);
			break;
		}
		case ARM64_INS_SXTW:
		{
			trunc = irb.CreateTrunc(val, i32);
			val   = irb.CreateSExt(trunc, ty);
			break;
		}
		default:
			throw GenericError("Arm64 translateExtension(): Unsupported extension type");
	}

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_EXTR
*/
void Capstone2LlvmIrTranslatorArm64_impl::translateExtr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_QUATERNARY(i, ai, irb);

	op1 = loadOp(ai->operands[1], irb);
	op2 = loadOp(ai->operands[2], irb);
	auto* lsb1 = loadOp(ai->operands[3], irb);
	lsb1 = irb.CreateZExtOrTrunc(lsb1, op1->getType());
	llvm::Value* lsb2 = llvm::ConstantInt::get(op1->getType(), llvm::cast<llvm::IntegerType>(op2->getType())->getBitWidth());
	lsb2 = irb.CreateSub(lsb2, lsb1);

	auto* left_val  = irb.CreateLShr(op2, lsb1);
	auto* right_val = irb.CreateShl(op1, lsb2);

	auto* val = irb.CreateOr(left_val, right_val);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_ORR, ARM64_INS_ORN
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateOrr(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_BINARY_OR_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);
	op2 = irb.CreateZExtOrTrunc(op2, op1->getType());

	if (i->id == ARM64_INS_ORN)
	{
	    op2 = generateValueNegate(irb, op2);
	}

	auto* val = irb.CreateOr(op1, op2);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_UDIV, ARM64_INS_SDIV
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateDiv(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	std::tie(op1, op2) = loadOpBinaryOrTernaryOp1Op2(ai, irb);

	// Zero division yelds zero as result in this case we
	// don't want undefined behaviour so we
	// check for zero division and manualy set the result, for now.
	llvm::Value* zero = llvm::ConstantInt::get(op1->getType(), 0);
	auto* cond = irb.CreateICmpEQ(op2, zero);
	auto irbP = generateIfThenElse(cond, irb);
	llvm::IRBuilder<>& bodyIf(irbP.first), bodyElse(irbP.second);

	//IF - store zero
	storeOp(ai->operands[0], zero, bodyIf);

	//ELSE - store result of division
	llvm::Value *val = nullptr;
	if (i->id == ARM64_INS_UDIV)
	{
		val = bodyElse.CreateUDiv(op1, op2);
	}
	else if (i->id == ARM64_INS_SDIV)
	{
		val = bodyElse.CreateSDiv(op1, op2);
	}

	storeOp(ai->operands[0], val, bodyElse);
	//ENDIF
}

/**
 * ARM64_INS_UMULH, ARM64_INS_SMULH
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMulh(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	bool sext = true;
	if (i->id == ARM64_INS_UMULH)
	{
		sext = false;
	}
	else if (i->id == ARM64_INS_SMULH)
	{
		sext = true;
	}
	else
	{
		throw GenericError("Mulh: Unhandled instruction ID");
	}

	auto* res_type = llvm::IntegerType::getInt128Ty(_module->getContext());
	auto* op1 = loadOp(ai->operands[1], irb);
	auto* op2 = loadOp(ai->operands[2], irb);
	if (sext)
	{
		op1 = irb.CreateSExtOrTrunc(op1, res_type);
		op2 = irb.CreateSExtOrTrunc(op2, res_type);
	}
	else
	{
		op1 = irb.CreateZExtOrTrunc(op1, res_type);
		op2 = irb.CreateZExtOrTrunc(op2, res_type);
	}

	auto *val = irb.CreateMul(op1, op2);

	// Get the high bits of the result
	val = irb.CreateAShr(val, llvm::ConstantInt::get(val->getType(), 64));

	val = irb.CreateSExtOrTrunc(val, getDefaultType());

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_UMULL, ARM64_INS_SMULL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMull(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	bool sext = true;
	if (i->id == ARM64_INS_UMULL)
	{
		sext = false;
	}
	else if (i->id == ARM64_INS_SMULL)
	{
		sext = true;
	}
	else
	{
		throw GenericError("Mull: Unhandled instruction ID");
	}

	auto* res_type = getDefaultType();
	auto* op1 = loadOp(ai->operands[1], irb);
	auto* op2 = loadOp(ai->operands[2], irb);
	if (sext)
	{
		op1 = irb.CreateSExtOrTrunc(op1, res_type);
		op2 = irb.CreateSExtOrTrunc(op2, res_type);
	}
	else
	{
		op1 = irb.CreateZExtOrTrunc(op1, res_type);
		op2 = irb.CreateZExtOrTrunc(op2, res_type);
	}

	auto *val = irb.CreateMul(op1, op2);

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_UMADDL, ARM64_INS_SMADDL
 * ARM64_INS_UMSUBL, ARM64_INS_SMSUBL
 * ARM64_INS_UMNEGL, ARM64_INS_SMNEGL
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMulOpl(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_EXPR(i, ai, irb, (3 <= ai->op_count && ai->op_count <= 4));

	bool sext = true;
	bool add_operation = true;
	bool op3_zero = false;
	switch(i->id) {
	case ARM64_INS_UMADDL:
		sext = false;
		add_operation = true;
		break;
	case ARM64_INS_SMADDL:
		sext = true;
		add_operation = true;
		break;
	case ARM64_INS_UMSUBL:
		sext = false;
		add_operation = false;
		break;
	case ARM64_INS_SMSUBL:
		sext = true;
		add_operation = false;
		break;
	case ARM64_INS_UMNEGL:
		sext = false;
		add_operation = false;
		op3_zero = true;
		break;
	case ARM64_INS_SMNEGL:
		sext = true;
		add_operation = false;
		op3_zero = true;
		break;
	default:
		throw GenericError("Maddl: Unhandled instruction ID");
	}

	auto* res_type = getDefaultType();

	auto* op1 = loadOp(ai->operands[1], irb);
	auto* op2 = loadOp(ai->operands[2], irb);
	if (sext)
	{
		op1 = irb.CreateSExtOrTrunc(op1, res_type);
		op2 = irb.CreateSExtOrTrunc(op2, res_type);
	}
	else
	{
		op1 = irb.CreateZExtOrTrunc(op1, res_type);
		op2 = irb.CreateZExtOrTrunc(op2, res_type);
	}

	auto *val = irb.CreateMul(op1, op2);

	llvm::Value* op3;
	if (op3_zero)
	{
		op3 = llvm::ConstantInt::get(res_type, 0);
	}
	else
	{
		op3 = loadOp(ai->operands[3], irb);
	}

	if (add_operation)
	{
		val = irb.CreateAdd(op3, val);
	}
	else
	{
		val = irb.CreateSub(op3, val);
	}

	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_MUL, ARM64_INS_MADD, ARM64_INS_MSUB, ARM64_INS_MNEG
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateMul(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_EXPR(i, ai, irb, (3 <= ai->op_count && ai->op_count <= 4));

	auto* op1 = loadOp(ai->operands[1], irb);
	auto* op2 = loadOp(ai->operands[2], irb);

	auto *val = irb.CreateMul(op1, op2);
	if (i->id == ARM64_INS_MADD)
	{
		auto* op3 = loadOp(ai->operands[3], irb);
		val = irb.CreateAdd(val, op3);
	}
	else if (i->id == ARM64_INS_MSUB)
	{
		auto* op3 = loadOp(ai->operands[3], irb);
		val = irb.CreateSub(op3, val);
	}

	if (i->id == ARM64_INS_MNEG)
	{
		llvm::Value* zero = llvm::ConstantInt::get(val->getType(), 0);
		val = irb.CreateSub(zero, val);
	}
	storeOp(ai->operands[0], val, irb);
}

/**
 * ARM64_INS_TBNZ, ARM64_INS_TBZ
 */
void Capstone2LlvmIrTranslatorArm64_impl::translateTbnz(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_TERNARY(i, ai, irb);

	std::tie(op0, op1, op2) = loadOpTernary(ai, irb);

	// Get the needed bit
	auto* ext_imm = irb.CreateZExtOrTrunc(op1, op0->getType());
	auto* shifted_one = irb.CreateShl(llvm::ConstantInt::get(op0->getType(), 1), ext_imm);
	auto* test_bit = irb.CreateAnd(shifted_one, op0);

	llvm::Value* cond = nullptr;
	if (i->id == ARM64_INS_TBNZ)
	{
		cond = irb.CreateICmpNE(test_bit, llvm::ConstantInt::get(op0->getType(), 0));
	}
	else if (i->id == ARM64_INS_TBZ)
	{
		cond = irb.CreateICmpEQ(test_bit, llvm::ConstantInt::get(op0->getType(), 0));
	}
	else
	{
		throw GenericError("cbnz, cbz: Instruction id error");
	}
	generateCondBranchFunctionCall(irb, cond, op2);
}

/**
 * ARM64_INS_RET
*/
void Capstone2LlvmIrTranslatorArm64_impl::translateRet(cs_insn* i, cs_arm64* ai, llvm::IRBuilder<>& irb)
{
	EXPECT_IS_NULLARY_OR_UNARY(i, ai, irb);

	// If the register operand is present
	if (ai->op_count == 1)
	{
		op0 = loadOp(ai->operands[0], irb);
	}
	else
	{
		// Default use x30
		op0 = loadRegister(ARM64_REG_LR, irb);
	}
	generateReturnFunctionCall(irb, op0);
}

} // namespace capstone2llvmir
} // namespace retdec
