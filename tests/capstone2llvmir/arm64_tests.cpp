/**
 * @file tests/capstone2llvmir/arm64_tests.cpp
 * @brief Capstone2LlvmIrTranslatorArm64 unit tests.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <llvm/IR/InstIterator.h>

#include "capstone2llvmir/capstone2llvmir_tests.h"
#include "retdec/capstone2llvmir/arm64/arm64.h"

using namespace ::testing;
using namespace llvm;

namespace retdec {
namespace capstone2llvmir {
namespace tests {

class Capstone2LlvmIrTranslatorArm64Tests :
		public Capstone2LlvmIrTranslatorTests,
		public ::testing::WithParamInterface<cs_mode>
{
	protected:
		virtual void initKeystoneEngine() override
		{
			ks_mode mode = KS_MODE_ARM;
			switch(GetParam())
			{
				// Basic modes.
				case CS_MODE_ARM: mode = KS_MODE_LITTLE_ENDIAN; break;
				// Extra modes.
				case CS_MODE_MCLASS: mode = KS_MODE_LITTLE_ENDIAN; break; // Missing in Keystone.
				case CS_MODE_V8: mode = KS_MODE_V8; break;
				// Unhandled modes.
				default: throw std::runtime_error("ERROR: unknown mode.\n");
			}
			if (ks_open(KS_ARCH_ARM64, mode, &_assembler) != KS_ERR_OK)
			{
				throw std::runtime_error("ERROR: failed on ks_open().\n");
			}
		}

		virtual void initCapstone2LlvmIrTranslator() override
		{
			switch(GetParam())
			{
				case CS_MODE_ARM:
					_translator = Capstone2LlvmIrTranslator::createArm64(&_module);
					break;
				default:
					throw std::runtime_error("ERROR: unknown mode.\n");
			}
		}

	protected:
		Capstone2LlvmIrTranslatorArm64* getArm64Translator()
		{
			return dynamic_cast<Capstone2LlvmIrTranslatorArm64*>(_translator.get());
		}

	// Some of these (or their parts) might be moved to abstract parent class.
	//
	protected:
		uint32_t getParentRegister(uint32_t reg)
		{
			return getArm64Translator()->getParentRegister(reg);
		}

		virtual llvm::GlobalVariable* getRegister(uint32_t reg) override
		{
			return _translator->getRegister(getParentRegister(reg));
		}

		virtual uint64_t getRegisterValueUnsigned(uint32_t reg) override
		{
			auto preg = getParentRegister(reg);
			auto* gv = getRegister(preg);
			auto val = _emulator->getGlobalVariableValue(gv).IntVal.getZExtValue();

			if (reg == preg)
			{
				return val;
			}

			switch (_translator->getRegisterBitSize(reg))
			{
				case 32: return static_cast<uint32_t>(val);
				case 64: return static_cast<uint64_t>(val);
				default: throw std::runtime_error("Unknown reg bit size.");
			}
		}

		virtual void setRegisterValueUnsigned(uint32_t reg, uint64_t val) override
		{
			auto preg = getParentRegister(reg);
			auto* gv = getRegister(preg);
			auto* t = cast<llvm::IntegerType>(gv->getValueType());

			GenericValue v = _emulator->getGlobalVariableValue(gv);

			if (reg == preg)
			{
				bool isSigned = false;
				v.IntVal = APInt(t->getBitWidth(), val, isSigned);
				_emulator->setGlobalVariableValue(gv, v);
				return;
			}

			uint64_t old = v.IntVal.getZExtValue();

			switch (_translator->getRegisterBitSize(reg))
			{
			case 32:
			    val = val & 0x00000000ffffffff;
			    old = old & 0xffffffff00000000;
			    break;
			case 64:
			    val = val & 0xffffffffffffffff;
			    old = old & 0x0000000000000000;
			    break;
			default:
			    throw std::runtime_error("Unknown reg bit size.");
			}

			val = old | val;
			bool isSigned = false;
			v.IntVal = APInt(t->getBitWidth(), val, isSigned);
			_emulator->setGlobalVariableValue(gv, v);
			return;
		}

};

struct PrintCapstoneModeToString_Arm64
{
	template <class ParamType>
	std::string operator()(const TestParamInfo<ParamType>& info) const
	{
		switch (info.param)
		{
			case CS_MODE_ARM: return "CS_MODE_ARM";
			case CS_MODE_MCLASS: return "CS_MODE_MCLASS";
			case CS_MODE_V8: return "CS_MODE_V8";
			default: return "UNHANDLED CS_MODE";
		}
	}
};

INSTANTIATE_TEST_CASE_P(
		InstantiateArm64WithAllModes,
		Capstone2LlvmIrTranslatorArm64Tests,
		::testing::Values(CS_MODE_ARM),
		 PrintCapstoneModeToString_Arm64());

//
// ARM64_INS_ADC
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_r_r_r_false)
{
	setRegisters({
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x4},
	});

	emulate("adc x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1234},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_r_r_r_true)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x4},
	});

	emulate("adc x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1235},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_s_r_r_r_false)
{
	setRegisters({
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x4},
	});

	emulate("adcs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1234},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC32_r_r_r_true)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x4},
	});

	emulate("adc w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1235},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC32_s_r_r_r_false)
{
	setRegisters({
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x4},
	});

	emulate("adcs w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1234},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC32_flags)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0xfffffffffffffffe},
		{ARM64_REG_X2, 0x1},
	});

	emulate("adcs w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}


TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_flags)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0xfffffffffffffffe},
		{ARM64_REG_X2, 0x1},
	});

	emulate("adcs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_flags1)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0xfffffffffffffffe},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("adcs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffe},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADC_flags2)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0xfffffffffffffffe},
		{ARM64_REG_X2, 0x0},
	});

	emulate("adcs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ADD
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
	});

	emulate("add x0, x1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1233},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD32_r_r_i)
{
	setRegisters({
		{ARM64_REG_W1, 0x1230},
	});

	emulate("add w0, w1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1233},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD32_r_r_ishift)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
	});

	// Valid shifts are: LSL #0 and LSL #12
	emulate("add x0, x1, #1, LSL #12");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x2230_qw},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD32_r_r_i_extend_test)
{
	// Value should be Zero extended into 64bit register
	setRegisters({
		{ARM64_REG_X0, 0xcafebabecafebabe},
		{ARM64_REG_W1, 0xf0000000},
	});

	emulate("add w0, w1, #1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xf0000001},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// Extended registers
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_UXTB)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
		{ARM64_REG_X2, 0x123456789abcdef0},
	});

	emulate("add x0, x1, w2, UXTB");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x10f0},});
	// 0x1000 + 0x00000000000000f0
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_UXTH)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
		{ARM64_REG_X2, 0x123456789abcdef0},
	});

	emulate("add x0, x1, w2, UXTH");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xeef0},});
	// 0x1000 + 0x000000000000def0
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_UXTW)
{
	// This means no extend just the optional shift, used in instruction aliases
	setRegisters({
		{ARM64_REG_X1, 0x1000000000000000},
		{ARM64_REG_X2, 0x123456789abcdef0},
	});

	emulate("add x0, x1, w2, UXTW");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x100000009abcdef0_qw},});
	// 0x1000000000000000 + 0x000000009abcdef0
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_SXTB)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x123456789abcdef0}, // -16
	});

	emulate("add x0, x1, w2, SXTB");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffffffffffef},});
	// 0xffffffffffffffff + 0xfffffffffffffff0 = -17
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_SXTH)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x123456789abcfffb}, // -5
	});

	emulate("add x0, x1, w2, SXTH");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xfffffffffffffffa},});
	// 0xffffffffffffffff + 0xfffffffffffffffb = -6
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_r_r_w_SXTW)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x12345678fffffffb}, // -5
	});

	emulate("add x0, x1, w2, SXTW");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xfffffffffffffffa},});
	// 0xffffffffffffffff + 0xfffffffffffffffb = -6
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_UXTB)
{
	setRegisters({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1000000},
		{ARM64_REG_X2, 0x1234567800000123},
	});

	emulate("add w0, w1, w2, UXTB");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1000023},});
	// 0x1000000 + 0x00000023
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_UXTH)
{
	setRegisters({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1000000},
		{ARM64_REG_X2, 0x1234567800000123},
	});

	emulate("add w0, w1, w2, UXTH");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1000123},});
	// 0x1000000 + 0x00000123
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_UXTW)
{
	setRegisters({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1000000},
		{ARM64_REG_X2, 0x1234567812345678},
	});

	emulate("add w0, w1, w2, UXTW");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x13345678},});
	// 0x1000000 + 0x12345678
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_SXTB)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff}, // -1
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x123456789abcdef0}, // -16
	});

	emulate("add w0, w1, w2, SXTB");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x00000000ffffffef},});
	// 0x00000000ffffffff + 0x00000000fffffff0 = -17
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_SXTH)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff}, // -1
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x123456789abcfffb}, // -5
	});

	emulate("add w0, w1, w2, SXTH");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x00000000fffffffa},});
	// 0x00000000ffffffff + 0x00000000fffffffb = -6
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_w_w_w_SXTW)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff}, // -1
		{ARM64_REG_X1, 0xffffffffffffffff}, // -1
		{ARM64_REG_X2, 0x12345678fffffffb}, // -5
	});

	emulate("add w0, w1, w2, SXTW");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x00000000fffffffa},});
	// 0x00000000ffffffff + 0x00000000fffffffb = -6
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_s_zero_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x0},
	});

	emulate("adds x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_s_negative_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffff000000000000},
		{ARM64_REG_X2, 0x1234},
	});

	emulate("adds x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffff000000001234},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_s_carry_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x1},
	});

	emulate("adds x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADD_s_overflow_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0fffffffffffffff},
		{ARM64_REG_X2, 0x7408089100000000},
	});

	emulate("adds x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x84080890ffffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ADR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADR)
{
	emulate("test:; adr x0, test", 0x40578);

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x40578},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ADRP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ADRP)
{
	emulate("test:; adrp x0, test", 0x41578);

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x82000},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_AND
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_AND_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567890abcdef},
	});

	emulate("and x0, x1, #0xf0");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000000000e0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_AND_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567890abcdef},
		{ARM64_REG_X2, 0xff00ff00ff00ff00},
	});

	emulate("and x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x120056009000cd00},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_AND32_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567890abcdef},
	});

	emulate("and w0, w1, #0x0f");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x000000000000000f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_AND_s_zero_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x12345678},
		{ARM64_REG_X2, 0x0},
	});

	emulate("ands x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_AND32_s_negative_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567880abcdef},
		{ARM64_REG_X2, 0xf0000000},
	});

	emulate("ands w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x80000000},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_EOR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EOR_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x00000000ffffffff},
	});

	emulate("eor x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffff00000000},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EOR_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("eor x0, x1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xfffffffffffffffc},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EOR32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffff0000},
		{ARM64_REG_X2, 0xffffffff},
	});

	emulate("eor w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_EON
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EON_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("eon x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x00000000ffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EON32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffff},
		{ARM64_REG_X2, 0x0000ffff},
	});

	emulate("eon w0, w1, w2, LSL #16");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffff0000},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ORR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ORR_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x00000000ffffffff},
	});

	emulate("orr x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffffffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ORR_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("orr x0, x1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffffffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ORR32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffff0000},
		{ARM64_REG_X2, 0xffffffff},
	});

	emulate("orr w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ORN
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ORN_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("orn x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x00000000ffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ORN32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffff},
		{ARM64_REG_X2, 0x0000ffff},
	});

	emulate("orn w0, w1, w2, LSL #16");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xffffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_EXTR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr x0, x1, x2, #63");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2222222222222223},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr x0, x1, x2, #48");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1111111111119999},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr x0, x1, x2, #16");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1111999999999999},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr x0, x1, x2, #10");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x4466666666666666},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i_5)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr x0, x1, x2, #0");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x9999999999999999},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR_r_r_r_i_6)
{
	setRegisters({
		{ARM64_REG_X2, 0x1234567890abcdef},
	});

	emulate("extr x0, x2, x2, #32");
	// alias ROR x0, x2, #32

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x90abcdef12345678},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR32_r_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr w0, w1, w2, #31");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000022222223},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_EXTR32_r_r_r_i_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1111111111111111},
		{ARM64_REG_X2, 0x9999999999999999},
	});

	emulate("extr w0, w1, w2, #16");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000011119999},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_ASR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ASR_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000000000000000},
		{ARM64_REG_X2, 0x20},
	});

	emulate("asr x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000010000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ASR_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
	});

	emulate("asr x0, x1, #63");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ASR32_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x0000000080000000},
	});

	emulate("asr w0, w1, #31");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CMN
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMN_zero_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x0},
	});

	emulate("cmn x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMN_negative_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffff00000000},
		{ARM64_REG_X2, 0x12345678},
	});

	emulate("cmn x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMN_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x1},
	});

	emulate("cmn x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMN_carry_overflow_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
		{ARM64_REG_X2, 0x8000000000000000},
	});

	emulate("cmn x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMN_overflow_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0fffffffffffffff},
		{ARM64_REG_X2, 0x7408089100000000},
	});

	emulate("cmn x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CCMP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #1, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #2, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #4, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c_4)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #8, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_r_r_r_c_5)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmp x1, x2, #15, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_negative_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffff0000000fffff},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmp x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_carry_zero_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmp x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP_overflow_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
		{ARM64_REG_X2, 0x7ffffffffffffffe},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmp x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMP32_overflow_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x80000000},
		{ARM64_REG_X2, 0x7ffffffe},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmp w1, w2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CCMN
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #3, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #7, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #10, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c_4)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #12, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_r_r_r_c_5)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_Z, false}
	});

	emulate("ccmn x1, x2, #14, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_negative_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xfffffffffffffffa},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmn x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_carry_negative_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmn x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN_overflow_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
		{ARM64_REG_X2, 0x8000000000000000},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmn x1, x2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CCMN32_overflow_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x80000000},
		{ARM64_REG_X2, 0x80000000},
		{ARM64_REG_CPSR_Z, true}
	});

	emulate("ccmn w1, w2, #0, eq");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CMP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMP_zero_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1234},
	});

	emulate("cmp x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMP_negative_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffff0000000fffff},
		{ARM64_REG_X2, 0x1234},
	});

	emulate("cmp x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMP_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x1},
	});

	emulate("cmp x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CMP_overflow_carry_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
		{ARM64_REG_X2, 0x7ffffffffffffffe},
	});

	emulate("cmp x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SUB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
	});

	emulate("sub x0, x1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x122d},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB32_r_r_i)
{
	setRegisters({
		{ARM64_REG_W1, 0x1230},
	});

	emulate("sub w0, w1, #3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x122d},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB32_r_r_ishift)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
	});

	// Valid shifts are: LSL #0 and LSL #12
	emulate("sub x0, x1, #1, LSL #12");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x0230_qw},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB32_r_r_i_extend_test)
{
	// Value should be Zero extended into 64bit register
	setRegisters({
		{ARM64_REG_X0, 0xcafebabecafebabe},
		{ARM64_REG_W1, 0xf0000000},
	});

	emulate("sub w0, w1, #1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0xefffffff},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB_s_zero_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x0},
	});

	emulate("subs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB_s_negative_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffff0000000fffff},
		{ARM64_REG_X2, 0x1234},
	});

	emulate("subs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffff0000000fedcb},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB_s_carry_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0x1},
	});

	emulate("subs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SUB_s_overflow_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0fffffffffffffff},
		{ARM64_REG_X2, 0x7408089100000000},
	});

	emulate("subs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x9bf7f76effffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_NEG
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEG_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
	});

	emulate("neg x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEG_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
	});

	emulate("neg x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEG_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("neg x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEG32_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
	});

	emulate("neg w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffedcc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEGS_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
	});

	emulate("negs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, true},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEGS_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("negs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEGS_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
	});

	emulate("negs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcc},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NEGS32_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0x1},
	});

	emulate("negs w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_NGC
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGC_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("ngc x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGC_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, false},
	});

	emulate("ngc x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGC32_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("ngc w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffedcc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGCS_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_CPSR_C, false},
	});

	emulate("ngcs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGCS_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("ngcs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGCS_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("ngcs x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcc},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NGCS32_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("ngcs w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000fffffffe},
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SBC
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SBC_r_r_r_false)
{
	setRegisters({
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x4},
	});

	emulate("sbc x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x122f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SBC_r_r_r_true)
{
	setRegisters({
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_X1, 0x1235},
		{ARM64_REG_X2, 0x4},
	});

	emulate("sbc x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1231},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SBC_s_r_r_r_false)
{
	setRegisters({
		{ARM64_REG_CPSR_C, false},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x4},
	});

	emulate("sbcs x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x122f},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_V, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MOV
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MOV_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xcafebabecafebabe},
	});

	emulate("mov x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xcafebabecafebabe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MOV32_r_r_extend_test)
{
	setRegisters({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_W1, 0xf0000000},
	});

	emulate("mov w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_W0, 0xf0000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MOV32_r_r)
{
	setRegisters({
		{ARM64_REG_W1, 0xcafebabe},
	});

	emulate("mov w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_W0, 0xcafebabe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MOVZ
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MOVZ_r_i)
{
	setRegisters({
		{ARM64_REG_X0, 0xcafebabecafebabe},
	});

	emulate("mov x0, #0xa");

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xa},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MVN
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MVN_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0123456789abcdef},
	});

	emulate("mvn x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfedcba9876543210},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MVN32_r_r)
{
	setRegisters({
		{ARM64_REG_W1, 0x89abcdef},
	});

	emulate("mvn w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_W0, 0x76543210},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_NOP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_NOP)
{
	emulate("nop");

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_STR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STR_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xcafebabecafebabe},
		{ARM64_REG_X1, 0x1234},
	});

	emulate("str x0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X0, ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0xcafebabecafebabe}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STR32_r_r)
{
	setRegisters({
		{ARM64_REG_W0, 0xcafebabe},
		{ARM64_REG_X1, 0x1234},
	});

	emulate("str w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0xcafebabe}
	});
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_STRB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STRB_r_r)
{
	setRegisters({
		{ARM64_REG_W0, 0xcafebabe},
		{ARM64_REG_X1, 0x1234},
	});

	emulate("strb w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0xbe}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STRB_r_r_r)
{
	setRegisters({
		{ARM64_REG_W0, 0xcafebabe},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x10},
	});

	emulate("strb w0, [x1, x2]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1244, 0xbe}
	});
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_STRH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STRH_r_r)
{
	setRegisters({
		{ARM64_REG_W0, 0xcafebabe},
		{ARM64_REG_X1, 0x1234},
	});

	emulate("strh w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0xbabe}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STRH_r_r_i)
{
	setRegisters({
		{ARM64_REG_W0, 0xcafebabe},
		{ARM64_REG_X1, 0x1234},
	});

	emulate("strh w0, [x1, #0x10]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1244, 0xbabe}
	});
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_STP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STP_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0x0123456789abcdef},
		{ARM64_REG_X2, 0xfedcba9876543210},
		{ARM64_REG_SP, 0x1234},
	});

	emulate("stp x0, x2, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X0, ARM64_REG_X2, ARM64_REG_SP});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0x0123456789abcdef_qw},
		{0x123c, 0xfedcba9876543210_qw}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STP32_r_r_r)
{
	setRegisters({
		{ARM64_REG_W0, 0x01234567},
		{ARM64_REG_W2, 0xfedcba98},
		{ARM64_REG_SP, 0x1234},
	});

	emulate("stp w0, w2, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_W0, ARM64_REG_W2, ARM64_REG_SP});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0x01234567_dw},
		{0x1238, 0xfedcba98_dw}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STP_r_r_mw)
{
	setRegisters({
		{ARM64_REG_X0, 0x0123456789abcdef},
		{ARM64_REG_X2, 0xfedcba9876543210},
		{ARM64_REG_SP, 0x1234},
	});

	emulate("stp x0, x2, [sp, #-0x20]!");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X0, ARM64_REG_X2, ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_SP, 0x121c}
	});
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1214, 0x0123456789abcdef_qw},
		{0x121c, 0xfedcba9876543210_qw}
	});
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_STP_r_r_m_i)
{
	setRegisters({
		{ARM64_REG_X0, 0x0123456789abcdef},
		{ARM64_REG_X2, 0xfedcba9876543210},
		{ARM64_REG_SP, 0x1234},
	});

	emulate("stp x0, x2, [sp], #-0x20");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X0, ARM64_REG_X2, ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_SP, 0x1214}
	});
	EXPECT_NO_MEMORY_LOADED();
	EXPECT_JUST_MEMORY_STORED({
		{0x1234, 0x0123456789abcdef_qw},
		{0x123c, 0xfedcba9876543210_qw}
	});
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR32)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
		{ARM64_REG_X0, 0xcafebabecafebabe},
	});
	setMemory({
		{0x1000, 0x12345678_dw},
	});

	emulate("ldr w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_W0, 0x12345678},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_plus_imm)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, #8]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_minus_imm)
{
	setRegisters({
		{ARM64_REG_X1, 0x1010},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, #-8]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_plus_reg)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
		{ARM64_REG_X2, 0x8},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, x2]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_minus_reg)
{
	setRegisters({
		{ARM64_REG_X1, 0x1010},
		{ARM64_REG_X2, -0x8},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, x2]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_plus_imm_preindexed_writeback)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, #8]!");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1008},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_minus_imm_preindexed_writeback)
{
	setRegisters({
		{ARM64_REG_X1, 0x1010},
	});
	setMemory({
		{0x1008, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1, #-8]!");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1008},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_plus_imm_postindexed_writeback)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1], #8");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0x1008},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_minus_imm_postindexed_writeback)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
	});

	emulate("ldr x0, [x1], #-8");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0xff8},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDR_label)
{
	// Load the memory at given label, or imm in this case
	setMemory({
		{0x15000, 0x123456789abcdef0_qw},
	});
	emulate("ldr x0, #0x15000");

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0_qw},
	});
	EXPECT_JUST_MEMORY_LOADED({0x15000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDRB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDRB)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0xf1_b},
	});

	emulate("ldrb w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xf1},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDRSB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDRSB)
{
	setRegisters({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x80_b},
	});

	emulate("ldrsb w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffff80},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDRH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDRH)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x8182_w},
	});

	emulate("ldrh w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x8182},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDRSH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDRSH)
{
	setRegisters({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x8182_w},
	});

	emulate("ldrsh w0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffff8182},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDRSW
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDRSW)
{
	setRegisters({
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0x1000},
	});
	setMemory({
		{0x1000, 0x81828384_dw},
	});

	emulate("ldrsw x0, [x1]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffff81828384},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDP
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDP_r_r_r)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
		{0x1008, 0xfedcba9876543210_qw},
	});

	emulate("ldp x0, x1, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0xfedcba9876543210},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDP32_r_r_r)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
	});
	setMemory({
		{0x1000, 0x12345678_dw},
		{0x1004, 0x9abcdef0_dw},
	});

	emulate("ldp w0, w1, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_W0, 0x12345678},
		{ARM64_REG_W1, 0x9abcdef0},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1004});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDP_r_r_mw)
{
	setRegisters({
		{ARM64_REG_SP, 0x1020},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
		{0x1008, 0xfedcba9876543210_qw},
	});

	emulate("ldp x0, x1, [sp, #-32]!");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0xfedcba9876543210},
		{ARM64_REG_SP, 0x1000},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDP_r_r_r_i)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
	});
	setMemory({
		{0x1000, 0x123456789abcdef0_qw},
		{0x1008, 0xfedcba9876543210_qw},
	});

	emulate("ldp x0, x1, [sp], #32");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x123456789abcdef0},
		{ARM64_REG_X1, 0xfedcba9876543210},
		{ARM64_REG_SP, 0x1020},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1008});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LDPSW
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDPSW_r_r_r)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x0},
	});
	setMemory({
		{0x1000, 0x12345678_dw},
		{0x1004, 0xfedcba98_dw},
	});

	emulate("ldpsw x0, x1, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x12345678},
		{ARM64_REG_X1, 0xfffffffffedcba98},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1004});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDPSW1_r_r_r)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
		{ARM64_REG_X0, 0x0},
		{ARM64_REG_X1, 0xffffffffffffffff},
	});
	setMemory({
		{0x1000, 0x12345678_dw},
		{0x1004, 0xfedcba98_dw},
	});

	emulate("ldpsw x1, x0, [sp]");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffedcba98},
		{ARM64_REG_X1, 0x12345678},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1004});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LDPSW_r_r_r_i)
{
	setRegisters({
		{ARM64_REG_SP, 0x1000},
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x0},
	});
	setMemory({
		{0x1000, 0x12345678_dw},
		{0x1004, 0xfedcba98_dw},
	});

	emulate("ldpsw x0, x1, [sp], #32");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_SP});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x12345678},
		{ARM64_REG_X1, 0xfffffffffedcba98},
		{ARM64_REG_SP, 0x1020},
	});
	EXPECT_JUST_MEMORY_LOADED({0x1000, 0x1004});
	EXPECT_NO_MEMORY_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LSL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSL_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffff00000001},
		{ARM64_REG_X2, 0x20},
	});

	emulate("lsl x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000100000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSL_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x0000000000000001},
	});

	emulate("lsl x0, x1, #63");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x8000000000000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSL32_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x0000000000000001},
		{ARM64_REG_X2, 31},
	});

	emulate("lsl w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000080000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_LSR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSR_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1000000000000000},
		{ARM64_REG_X2, 0x20},
	});

	emulate("lsr x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000010000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSR_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
	});

	emulate("lsr x0, x1, #63");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000000001},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_LSR32_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x0000000080000000},
	});

	emulate("lsr w0, w1, #31");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000000001},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_B
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_B)
{
	emulate("b #0x110d8", 0x1107C);

	EXPECT_NO_REGISTERS_LOADED_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getBranchFunction(), {0x110d8}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_B_cond)
{
	emulate("b.ne #0x110d8", 0x1107C);

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	// TODO: How to test this?
	//EXPECT_JUST_VALUES_CALLED({
	//	{_translator->getCondBranchFunction(), {0x110d8}},
	//});
}

//
// ARM64_INS_BL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_BL)
{
	emulate("bl #0x110d8", 0x1107C);

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_LR, 0x11080},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCallFunction(), {0x110d8}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_BL_label)
{
	emulate("label_test:; bl label_test", 0x1000);

	EXPECT_NO_REGISTERS_LOADED();
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_LR, 0x1004},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCallFunction(), {0x1000}},
	});
}

//
// ARM64_INS_BR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_BR)
{
	setRegisters({
		{ARM64_REG_X1, 0xcafebabecafebabe},
	});

	emulate("br x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getBranchFunction(), {0xcafebabecafebabe}},
	});
}

//
// ARM64_INS_BLR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_BLR)
{
	setRegisters({
		{ARM64_REG_X2, 0x123456789abcdef0},
	});

	emulate("blr x2", 0x2000);

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_LR, 0x2004},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getBranchFunction(), {0x123456789abcdef0}},
	});
}

//
// ARM64_INS_CBNZ
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBNZ_true)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("cbnz x1, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBNZ_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
	});

	emulate("cbnz x1, #0x1234");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1234}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBNZ32_true)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("cbnz w1, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

//
// ARM64_INS_CBZ
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBZ_true)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("cbz x1, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBZ_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
	});

	emulate("cbz x1, #0x1234");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1234}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CBZ32_true)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
	});

	emulate("cbz w1, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

//
// ARM64_INS_CSEL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSEL_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x0000000000000001},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("csel x0, x1, x2, ne");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_Z});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSEL_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x0000000000000001},
		{ARM64_REG_CPSR_V, false},
	});

	emulate("csel x0, x1, x2, vs");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_CPSR_V});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSEL32_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x0000000000000001},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("csel w0, w1, w2, lt");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2,
				      ARM64_REG_CPSR_N, ARM64_REG_CPSR_V});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CSET
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSET_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("cset x0, hi");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSET_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("cset x0, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSET32_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("cset w0, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CSETM
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSETM_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("csetm x0, hi");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSETM_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("csetm x0, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSETM32_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
	});

	emulate("csetm w0, hs");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_C});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CSINC
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSINC_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("csinc x0, x1, x2, hi");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSINC_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1},
		{ARM64_REG_X2, 0x1234},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("csinc x0, x1, x2, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1235},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CSINV
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSINV_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("csinv x0, x1, x2, hi");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSINV_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("csinv x0, x1, x2, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffe},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CSNEG
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSNEG_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("csneg x0, x1, x2, hi");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CSNEG_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0x5},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("csneg x0, x1, x2, ge");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffb},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CINC
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CINC_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("cinc x0, x1, ls");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CINC_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("cinc x0, x1, lt");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CINV
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CINV_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("cinv x0, x1, ls");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CINV_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("cinv x0, x1, lt");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcb},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_CNEG
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CNEG_true)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_C, true},
		{ARM64_REG_CPSR_Z, false},
	});

	emulate("cneg x0, x1, ls");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_Z, ARM64_REG_CPSR_C, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_CNEG_false)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_V, true},
	});

	emulate("cneg x0, x1, lt");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_CPSR_N, ARM64_REG_CPSR_V, ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	    });
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MUL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
	});

	emulate("mul x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x4},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mul x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mul x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mul x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0x50},
	});

	emulate("mul w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xa0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MUL32_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mul w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000fffffffe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MADD
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_X3, 0x100},
	});

	emulate("madd x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x104},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0x123},
	});

	emulate("madd x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x124},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0xffffffffffffffff},
	});

	emulate("madd x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0x2},
	});

	emulate("madd x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0x50},
		{ARM64_REG_X3, 0xffffffffffffffff},
	});

	emulate("madd w0, w1, w2, w3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x9f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MADD32_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0x3},
	});

	emulate("madd w0, w1, w2, w3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UMADDL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMADDL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0x100},
	});

	emulate("umaddl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2000000fe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMADDL_r_w_w_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0x100000000},
	});

	emulate("umaddl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2fffffffe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SMADDL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMADDL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0xfffffffffffffffb},
	});

	emulate("smaddl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffff9},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UMSUBL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMSUBL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0x100},
	});

	emulate("umsubl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffe00000102},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMSUBL_r_w_w_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0x11fffffffe},
	});

	emulate("umsubl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1000000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SMSUBL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMSUBL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
		{ARM64_REG_X3, 0xfffffffffffffffb},
	});

	emulate("smsubl x0, w1, w2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffd},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UMNEGL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMNEGL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
	});

	emulate("umnegl x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffe00000002},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SMNEGL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMNEGL_r_w_w_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x00000000ffffffff},
		{ARM64_REG_X2, 0x2},
	});

	emulate("smnegl x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UMULL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMULL_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
	});

	emulate("umull x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x4},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMULL_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0xffffffff},
	});

	emulate("umull x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x3fffffffc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SMULL
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMULL_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
	});

	emulate("smull x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x4},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMULL_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0xffffffff},
	});

	emulate("smull x0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UMULH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UMULH_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
		//{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("umulh x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SMULH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMULH_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
	});

	emulate("smulh x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SMULH_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("smulh x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MNEG
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
	});

	emulate("mneg x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xfffffffffffffffc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mneg x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mneg x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("mneg x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0x50},
	});

	emulate("mneg w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffff60},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MNEG32_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0x1},
	});

	emulate("mneg w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000fffffffe},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_MSUB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x4},
		{ARM64_REG_X2, 0x1},
		{ARM64_REG_X3, 0x3},
	});

	emulate("msub x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0x123},
	});

	emulate("msub x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x122},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0x0},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0xffffffffffffffff},
	});

	emulate("msub x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0xfffffffffffffffe},
	});

	emulate("msub x0, x1, x2, x3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0x50},
		{ARM64_REG_X3, 0xffffffffffffffff},
	});

	emulate("msub w0, w1, w2, w3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffff5f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_MSUB32_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x2},
		{ARM64_REG_X2, 0xffffffffffffffff},
		{ARM64_REG_X3, 0x3},
	});

	emulate("msub w0, w1, w2, w3");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2, ARM64_REG_X3});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x5},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SXTB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTB_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x80},
	});

	emulate("sxtb w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffff80},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTB_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x7f},
	});

	emulate("sxtb w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x000000000000007f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SXTH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTH_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000},
	});

	emulate("sxth w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffff8000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTH_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x7fff},
	});

	emulate("sxth w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000007fff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SXTW
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTW_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x80000000},
	});

	emulate("sxtw x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffff80000000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SXTW_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x7fffffff},
	});

	emulate("sxtw x0, x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x000000007fffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UXTB
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UXTB_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x80},
	});

	emulate("uxtb w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000000080},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UXTB_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x7f},
	});

	emulate("uxtb w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x000000000000007f},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_UXTH
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UXTH_r_r_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000},
	});

	emulate("uxth w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000008000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UXTH_r_r_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x7fff},
	});

	emulate("uxth w0, w1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000007fff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}


//
// ARM64_INS_TBNZ
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBNZ_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x000000000000000f},
	});

	emulate("tbnz x1, #0, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBNZ_false)
{
	setRegisters({
		{ARM64_REG_X1, 0xfffffffffffffff0},
	});

	emulate("tbnz x1, #0, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBNZ_63_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
	});

	emulate("tbnz x1, #63, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBNZ_32_true)
{
	setRegisters({
		{ARM64_REG_X1, 0x100000000},
	});

	emulate("tbnz x1, #32, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

//
// ARM64_INS_TBZ
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBZ_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x000000000000000f},
	});

	emulate("tbz x1, #0, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBZ_true)
{
	setRegisters({
		{ARM64_REG_X1, 0xfffffffffffffff0},
	});

	emulate("tbz x1, #0, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {true, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBZ_63_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x8000000000000000},
	});

	emulate("tbz x1, #63, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TBZ_32_false)
{
	setRegisters({
		{ARM64_REG_X1, 0x100000000},
	});

	emulate("tbz x1, #32, #0x1000");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getCondBranchFunction(), {false, 0x1000}},
	});
}

//
// ARM64_INS_RET
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_RET)
{
	setRegisters({
		{ARM64_REG_LR, 0xcafebabe},
	});

	emulate("ret");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_LR});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getReturnFunction(), {0xcafebabe}},
	});
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_RET_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xcafebabe},
	});

	emulate("ret x1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_NO_REGISTERS_STORED();
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_JUST_VALUES_CALLED({
		{_translator->getReturnFunction(), {0xcafebabe}},
	});
}

//
// ARM64_INS_ROR
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ROR_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x0000000000000001},
		{ARM64_REG_X2, 63},
	});

	emulate("ror x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000000000002},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ROR_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffff00000000},
	});

	emulate("ror x0, x1, #32");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000ffffffff},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_ROR32_r_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffff00001234},
		{ARM64_REG_X2, 16},
	});

	emulate("ror w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000012340000},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_SDIV
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x1230},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0xffffffffffffedcc},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffedcc},
		{ARM64_REG_X2, 0xffffffffffffedcc},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x5},
		{ARM64_REG_X2, 0x2},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r_4)
{
	setRegisters({
		{ARM64_REG_X1, 0xfffffffffffffffc},
		{ARM64_REG_X2, 0xfffffffffffffffe},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0xa},
		{ARM64_REG_X2, 0x00000000fffffffe},
	});

	emulate("sdiv w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x00000000fffffffb},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_SDIV_r_r_r_zero_div)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x0},
	});

	emulate("sdiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x0},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x1230},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x1},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r_1)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234},
		{ARM64_REG_X2, 0xffffffffffffffff},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r_2)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffedcc},
		{ARM64_REG_X2, 0xffffffffffffedcc},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r_3)
{
	setRegisters({
		{ARM64_REG_X1, 0x5},
		{ARM64_REG_X2, 0x2},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x2},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r_4)
{
	setRegisters({
		{ARM64_REG_X1, 0xfffffffffffffffe},
		{ARM64_REG_X2, 0xfffffffffffffffc},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x1},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV32_r_r_r)
{
	setRegisters({
		{ARM64_REG_X0, 0xffffffffffffffff},
		{ARM64_REG_X1, 0x00000000fffffffe},
		{ARM64_REG_X2, 0xa},
	});

	emulate("udiv w0, w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_X0, 0x0000000019999999},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_UDIV_r_r_r_zero_div)
{
	setRegisters({
		{ARM64_REG_X1, 0x1230},
		{ARM64_REG_X2, 0x0},
	});

	emulate("udiv x0, x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({{ARM64_REG_X0, 0x0},});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

//
// ARM64_INS_TST
//

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TST_zero_r_i)
{
	setRegisters({
		{ARM64_REG_X1, 0x12345678},
	});

	emulate("tst x1, #1");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}


TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TST_zero_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x12345678},
		{ARM64_REG_X2, 0x0},
	});

	emulate("tst x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TST_minus_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0xffffffffffffffff},
		{ARM64_REG_X2, 0x8000000000000000},
	});

	emulate("tst x1, x2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TST32_zero_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567880abcdef},
		{ARM64_REG_X2, 0x00000000},
	});

	emulate("tst w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, false},
		{ARM64_REG_CPSR_Z, true},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

TEST_P(Capstone2LlvmIrTranslatorArm64Tests, ARM64_INS_TST32_negative_r_r)
{
	setRegisters({
		{ARM64_REG_X1, 0x1234567880abcdef},
		{ARM64_REG_X2, 0xf0000000},
	});

	emulate("tst w1, w2");

	EXPECT_JUST_REGISTERS_LOADED({ARM64_REG_X1, ARM64_REG_X2});
	EXPECT_JUST_REGISTERS_STORED({
		{ARM64_REG_CPSR_N, true},
		{ARM64_REG_CPSR_Z, false},
		{ARM64_REG_CPSR_V, false},
		{ARM64_REG_CPSR_C, false},
	});
	EXPECT_NO_MEMORY_LOADED_STORED();
	EXPECT_NO_VALUE_CALLED();
}

} // namespace tests
} // namespace capstone2llvmir
} // namespace retdec