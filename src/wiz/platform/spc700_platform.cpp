#include <string>
#include <cstddef>
#include <unordered_map>
#include <memory>
#include <tuple>

#include <wiz/ast/expression.h>
#include <wiz/ast/statement.h>
#include <wiz/compiler/bank.h>
#include <wiz/compiler/builtins.h>
#include <wiz/compiler/compiler.h>
#include <wiz/compiler/definition.h>
#include <wiz/compiler/instruction.h>
#include <wiz/compiler/symbol_table.h>
#include <wiz/utility/report.h>
#include <wiz/platform/spc700_platform.h>

namespace wiz {
    Spc700Platform::Spc700Platform() {}

    Spc700Platform::~Spc700Platform() {}

    void Spc700Platform::reserveDefinitions(Builtins& builtins) {
		// http://emureview.ztnet.com/developerscorner/SoundCPU/spc.htm
		// https://wiki.superfamicom.org/spc700-reference

        builtins.addDefineBoolean("__cpu_spc700"_sv, true);

        auto stringPool = builtins.getStringPool();
        auto scope = builtins.getBuiltinScope();
        const auto decl = builtins.getBuiltinDeclaration();
        const auto u8Type = builtins.getDefinition(Builtins::DefinitionType::U8);
        const auto u16Type = builtins.getDefinition(Builtins::DefinitionType::U16);
        const auto u24Type = builtins.getDefinition(Builtins::DefinitionType::U24);
        const auto boolType = builtins.getDefinition(Builtins::DefinitionType::Bool);

        pointerSizedType = u16Type;
        farPointerSizedType = u24Type;

        // Registers.
        a = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("a"), decl);
        x = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("x"), decl);
        y = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("y"), decl);
        ya = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("ya"), decl);

        builtins.addRegisterDecomposition(ya, {a, y});

        const auto patternA = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(a));
        const auto patternX = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(x));
        const auto patternY = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(y));
        const auto patternSP = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("sp"), decl)));
        const auto patternPSW = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("psw"), decl)));
        const auto patternYA = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(ya));

        negative = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("negative"), decl);
        overflow = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("overflow"), decl);
        direct_page = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("direct_page"), decl);
        break_flag = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("break_flag"), decl);
        half_carry = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("half_carry"), decl);
        interrupt = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("interrupt"), decl);
        zero = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("zero"), decl);
        carry = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("carry"), decl);

        const auto patternNegative = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(negative));
        const auto patternOverflow = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(overflow));
        const auto patternDirectPage = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(direct_page));
        const auto patternInterrupt = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(interrupt));
        const auto patternZero = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(zero));
        const auto patternCarry = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(carry));

        // Intrinsics.
        const auto push = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("push"), decl);
        const auto pop = scope->createDefinition(nullptr, Definition::BuiltinLoadIntrinsic(u8Type), stringPool->intern("pop"), decl);
        const auto irqcall = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("irqcall"), decl);
        const auto nop = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("nop"), decl);
        const auto sleep = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("sleep"), decl);
        const auto stop = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("stop"), decl);
        const auto swap_digits = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("swap_digits"), decl);
        const auto test_and_set = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("test_and_set"), decl);
        const auto test_and_clear = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("test_and_clear"), decl);
        const auto divmod = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("divmod"), decl);
        const auto decimal_adjust_add = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("decimal_adjust_add"), decl);
        const auto decimal_adjust_sub = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("decimal_adjust_sub"), decl);
        cmp = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp"), decl);
        cmp_branch_not_equal = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp_branch_not_equal"), decl);
        dec_branch_not_zero = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("dec_branch_not_zero"), decl);

        // Non-register operands.
        const auto patternFalse = builtins.createInstructionOperandPattern(InstructionOperandPattern::Boolean(false));
        const auto patternTrue = builtins.createInstructionOperandPattern(InstructionOperandPattern::Boolean(true));
        const auto patternAtLeast0 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerAtLeast(Int128(0)));
        const auto patternAtLeast1 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerAtLeast(Int128(1)));
        const auto patternImmBitSubscript = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(7)));
        const auto patternImmU8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(0xFF)));
        const auto patternImmU16 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(0xFFFF)));
        const auto patternImmHighPageAddress = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0xFF00), Int128(0xFFFF)));
        const auto patternTableCall0 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFDE))), 2));
        const auto patternTableCall1 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFDC))), 2));
        const auto patternTableCall2 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFDA))), 2));
        const auto patternTableCall3 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFD8))), 2));
        const auto patternTableCall4 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFD6))), 2));
        const auto patternTableCall5 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFD4))), 2));
        const auto patternTableCall6 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFD2))), 2));
        const auto patternTableCall7 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFD0))), 2));
        const auto patternTableCall8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFCE))), 2));
        const auto patternTableCall9 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFCC))), 2));
        const auto patternTableCallA = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFCA))), 2));
        const auto patternTableCallB = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFC8))), 2));
        const auto patternTableCallC = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFC6))), 2));
        const auto patternTableCallD = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFC4))), 2));
        const auto patternTableCallE = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFC2))), 2));
        const auto patternTableCallF = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::IntegerRange(Int128(0xFFC0))), 2));
        const auto patternDirectU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU8->clone())),
                1));
        const auto patternDirectU16
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU8->clone())),
                2));
        const auto patternDirectIndexedByXU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU8->clone())),
                patternX->clone(),
                1, 1));
        const auto patternDirectIndexedByYU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU8->clone())),
                patternY->clone(),
                1, 1));
        const auto patternDirectU8BitIndex
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::BitIndex(
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Dereference(
                    false,
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmU8->clone())),
                    1)),
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmBitSubscript->clone()))));
        const auto patternDirectIndexedByXIndirectU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Index(
                    false,
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmU16->clone())),
                    patternX->clone(),
                    1, 2)),
                1));
        const auto patternDirectIndirectIndexedByYU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Dereference(
                    false,
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmU16->clone())),
                    2)),
                patternY->clone(),
                1, 1));
        const auto patternAbsoluteU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                1));
        const auto patternAbsoluteIndexedByXU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                patternX->clone(),
                1, 1));
        const auto patternAbsoluteIndexedByYU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                patternY->clone(),
                1, 1));
        const auto patternAbsoluteU8BitIndex
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::BitIndex(
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Dereference(
                    false,
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmU16->clone())),
                    1)),
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmBitSubscript->clone()))));
        const auto patternAbsoluteU8BitIndexNot
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Unary(
                UnaryOperatorKind::LogicalNegation,                 
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::BitIndex(
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Dereference(
                        false,
                        makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                            patternImmU16->clone())),
                        1)),
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmBitSubscript->clone()))))));

        const auto patternXIndirectU8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternX->clone(), 1));        
        const auto patternYIndirectU8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternY->clone(), 1));
        const auto patternXPostIncrementIndirectU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Unary(
                    UnaryOperatorKind::PostIncrement,
                    patternX->clone())),
                1));
        const auto patternAbsoluteIndexedByXIndirectU16
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                2));

        // Instruction encodings.
        const auto encodingImplicit = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size();
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);
                static_cast<void>(captureLists);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                return true;
            });
        const auto encodingU8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value));
                return true;
            });
        const auto encodingU16Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());

                const auto value = static_cast<std::uint16_t>(captureLists[options.parameter[0]][0]->integer.value);
                buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
                buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
                return true;
            });
        const auto encodingPCRelativeI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());

                const auto base = static_cast<std::int32_t>(bank->getAddress().absolutePosition.get() & 0xFFFF);
                const auto dest = static_cast<std::int32_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto offset = dest - base - 2;
                if (offset >= -128 && offset <= 127) {
                    buffer.push_back(offset < 0
                        ? (static_cast<std::uint8_t>(-offset) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(offset));
                    return true;
                } else {
                    buffer.push_back(0);
                    report->error("pc-relative offset is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
            });
        const auto encodingU8OperandPCRelativeI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value));

                const auto base = static_cast<int>(bank->getAddress().absolutePosition.get());
                const auto dest = static_cast<int>(captureLists[options.parameter[1]][0]->integer.value);
                const auto offset = dest - base - 3;
                if (offset >= -128 && offset <= 127) {
                    buffer.push_back(offset < 0
                        ? (static_cast<std::uint8_t>(-offset) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(offset));
                    return true;
                } else {
                    buffer.push_back(0);
                    report->error("pc-relative offset is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
            });
        const auto encodingU8OperandBitIndex = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                // *(dp) $ n bit-wise access.
                // dp = 0th capture of $0
                // n = $2th capture of $1
                const auto zp = static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto n = static_cast<std::uint8_t>(captureLists[options.parameter[1]][options.parameter[2]]->integer.value);
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer[buffer.size() - 1] |= static_cast<std::uint8_t>(n << 5);
                buffer.push_back(zp);
                return true;
            });
        const auto encodingU8OperandBitIndexBranch = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                // goto dest if *(zp) $ n
                // zp = 0th capture of $0
                // n = $2th capture of $1
                // dest = 0th capture of $3
                const auto zp = static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto n = static_cast<std::uint8_t>(captureLists[options.parameter[1]][options.parameter[2]]->integer.value);
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer[0] |= static_cast<std::uint8_t>(n << 4);
                buffer.push_back(zp);

                const auto base = static_cast<int>(bank->getAddress().absolutePosition.get());
                const auto dest  = static_cast<int>(captureLists[options.parameter[3]][0]->integer.value);
                const auto offset = dest - base - 3;
                if (offset >= -128 && offset <= 127) {
                    buffer.push_back(offset < 0
                        ? (static_cast<std::uint8_t>(-offset) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(offset));
                    return true;
                } else {
                    buffer.push_back(0);
                    report->error("pc-relative offset is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
            });
        const auto encodingU13OperandBitIndex = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(bank);

                // *(abs) $ n bit-wise access.
                // abs = 0th capture of $0
                // n = $2th capture of $1
                const auto abs = static_cast<std::uint16_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto n = static_cast<std::uint16_t>(captureLists[options.parameter[1]][options.parameter[2]]->integer.value);

                if (abs < 0x2000) {
                    const auto value = static_cast<std::uint16_t>((abs & 0x1FFF) | (n << 13U));
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                    buffer.push_back(value & 0xFF);
                    buffer.push_back((value >> 8) & 0xFF);
                    return true;
                } else {
                    buffer.push_back(0);
                    report->error("absolute address is outside representable unsigned 13-bit range 0x0000..0x1FFF used by single-bit instruction", location);
                    return false;
                }
            });
        const auto encodingRepeatedImplicit = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                return static_cast<std::size_t>(captureLists[options.parameter[0]][0]->integer.value) * options.opcode.size();
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                const auto count = static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value);
                for (std::size_t i = 0; i != count; ++i) {
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                }
                return true;
            });
        const auto encodingRepeatedU8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                return static_cast<std::size_t>(captureLists[options.parameter[1]][0]->integer.value) * (options.opcode.size() + 1);
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                const auto value = static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto count = static_cast<std::uint8_t>(captureLists[options.parameter[1]][0]->integer.value);
                for (std::size_t i = 0; i != count; ++i) {
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                    buffer.push_back(value);
                }
                return true;
            });
        const auto encodingRepeatedU16Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                return static_cast<std::size_t>(captureLists[options.parameter[1]][0]->integer.value) * (options.opcode.size() + 2);
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                const auto value = static_cast<std::uint16_t>(captureLists[options.parameter[0]][0]->integer.value);
                const auto count = static_cast<std::uint8_t>(captureLists[options.parameter[1]][0]->integer.value);
                for (std::size_t i = 0; i != count; ++i) {
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                    buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
                    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
                }
                return true;
            });
        const auto encodingU8OperandU8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->integer.value));
                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[1]][0]->integer.value));
                return true;
            });

        // Instructions.
        // a = mem
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternImmU8}), encodingU8Operand, InstructionOptions({0xE8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternXIndirectU8}), encodingImplicit, InstructionOptions({0xE6}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternDirectU8}), encodingU8Operand, InstructionOptions({0xE4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternDirectIndexedByXU8}), encodingU8Operand, InstructionOptions({0xF4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0xE5}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternAbsoluteIndexedByXU8}), encodingU16Operand, InstructionOptions({0xF5}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternAbsoluteIndexedByYU8}), encodingU16Operand, InstructionOptions({0xF6}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternDirectIndexedByXIndirectU8}), encodingU8Operand, InstructionOptions({0xE7}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternDirectIndirectIndexedByYU8}), encodingU8Operand, InstructionOptions({0xF7}, {1}, {}));
        // a = *(x++)
        // *(x++) = a
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternXPostIncrementIndirectU8}), encodingImplicit, InstructionOptions({0xBF}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternXPostIncrementIndirectU8, patternA}), encodingImplicit, InstructionOptions({0xAF}, {}, {}));
        // mem = a
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternXIndirectU8, patternA}), encodingImplicit, InstructionOptions({0xC6}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8, patternA}), encodingU8Operand, InstructionOptions({0xC4}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectIndexedByXU8, patternA}), encodingU8Operand, InstructionOptions({0xD4}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteU8, patternA}), encodingU16Operand, InstructionOptions({0xC5}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteIndexedByXU8, patternA}), encodingU16Operand, InstructionOptions({0xD5}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteIndexedByYU8, patternA}), encodingU16Operand, InstructionOptions({0xD6}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectIndexedByXIndirectU8, patternA}), encodingU8Operand, InstructionOptions({0xC7}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectIndirectIndexedByYU8, patternA}), encodingU8Operand, InstructionOptions({0xD7}, {0}, {}));
        {
            const std::pair<InstructionType, std::vector<std::uint8_t>> arithmeticOperators[] {
                {BinaryOperatorKind::BitwiseOr, {0x00}},
                {BinaryOperatorKind::BitwiseAnd, {0x20}},
                {BinaryOperatorKind::BitwiseXor, {0x40}},
                {BinaryOperatorKind::AdditionWithCarry, {0x80}},
                {BinaryOperatorKind::Addition, {0x60, 0x80}},
                {InstructionType::VoidIntrinsic(cmp), {0x60}},
                {BinaryOperatorKind::SubtractionWithCarry, {0xA0}},
                {BinaryOperatorKind::Subtraction, {0x80, 0xA0}},
            };

            using ArithmeticOperandSignature = std::tuple<const InstructionOperandPattern*, const InstructionOperandPattern*, const InstructionEncoding*, std::uint8_t, std::vector<std::size_t>>;
            const ArithmeticOperandSignature arithmeticOperandSignatures[] {
                ArithmeticOperandSignature {patternA, patternImmU8, encodingU8Operand, 0x08, {1}},
                ArithmeticOperandSignature {patternA, patternXIndirectU8, encodingImplicit, 0x06, {}},
                ArithmeticOperandSignature {patternA, patternDirectU8, encodingU8Operand, 0x04, {1}},
                ArithmeticOperandSignature {patternA, patternDirectIndexedByXU8, encodingU8Operand, 0x14, {1}},
                ArithmeticOperandSignature {patternA, patternAbsoluteU8, encodingU16Operand, 0x05, {1}},
                ArithmeticOperandSignature {patternA, patternAbsoluteIndexedByXU8, encodingU16Operand, 0x15, {1}},
                ArithmeticOperandSignature {patternA, patternAbsoluteIndexedByYU8, encodingU16Operand, 0x16, {1}},
                ArithmeticOperandSignature {patternA, patternDirectIndexedByXIndirectU8, encodingU8Operand, 0x07, {1}},
                ArithmeticOperandSignature {patternA, patternDirectIndirectIndexedByYU8, encodingU8Operand, 0x17, {1}},
                ArithmeticOperandSignature {patternXIndirectU8, patternYIndirectU8, encodingImplicit, 0x19, {0, 1}},
                ArithmeticOperandSignature {patternDirectU8, patternImmU8, encodingU8OperandU8Operand, 0x18, {1, 0}},
                ArithmeticOperandSignature {patternDirectU8, patternDirectU8, encodingU8OperandU8Operand, 0x09, {1, 0}},
            };
            //  arithmetic (a, mem) and (mem, mem)
            for (const auto& op : arithmeticOperators) {
                for (const auto& sig : arithmeticOperandSignatures) {
                    std::vector<std::uint8_t> opcode = op.second;
                    opcode[opcode.size() - 1] |= std::get<3>(sig);
                    builtins.createInstruction(InstructionSignature(op.first, 0, {std::get<0>(sig), std::get<1>(sig)}), std::get<2>(sig), InstructionOptions(std::move(opcode), std::get<4>(sig), {}));
                }
            }
        }
        // x = mem
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternImmU8}), encodingU8Operand, InstructionOptions({0xCD}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternDirectU8}), encodingU8Operand, InstructionOptions({0xF8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternDirectIndexedByYU8}), encodingU8Operand, InstructionOptions({0xF9}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0xE9}, {1}, {}));
        // mem = x
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8, patternX}), encodingU8Operand, InstructionOptions({0xD8}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectIndexedByYU8, patternX}), encodingU8Operand, InstructionOptions({0xD9}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteU8, patternX}), encodingU16Operand, InstructionOptions({0xC9}, {0}, {}));
        // y = mem
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternY, patternImmU8}), encodingU8Operand, InstructionOptions({0x8D}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternY, patternDirectU8}), encodingU8Operand, InstructionOptions({0xEB}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternY, patternDirectIndexedByXU8}), encodingU8Operand, InstructionOptions({0xFB}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternY, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0xEC}, {1}, {}));
        // mem = y
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8, patternY}), encodingU8Operand, InstructionOptions({0xCB}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectIndexedByXU8, patternY}), encodingU8Operand, InstructionOptions({0xDB}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteU8, patternY}), encodingU16Operand, InstructionOptions({0xCC}, {0}, {}));
        // r = r
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternX}), encodingImplicit, InstructionOptions({0x7D}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternA, patternY}), encodingImplicit, InstructionOptions({0xDD}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternA}), encodingImplicit, InstructionOptions({0x5D}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternY, patternA}), encodingImplicit, InstructionOptions({0xFD}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternX, patternSP}), encodingImplicit, InstructionOptions({0x9D}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternSP, patternX}), encodingImplicit, InstructionOptions({0xBD}, {}, {}));
        // mem = mem
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8, patternImmU8}), encodingU8OperandU8Operand, InstructionOptions({0x8F}, {1, 0}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8, patternDirectU8}), encodingU8OperandU8Operand, InstructionOptions({0xFA}, {1, 0}, {}));
        // cmp x, mem
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternX, patternImmU8}), encodingU8Operand, InstructionOptions({0xC8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternX, patternDirectU8}), encodingU8Operand, InstructionOptions({0x3E}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternX, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0x1E}, {1}, {}));
        // cmp y, mem
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternY, patternImmU8}), encodingU8Operand, InstructionOptions({0xAD}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternY, patternDirectU8}), encodingU8Operand, InstructionOptions({0x7E}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternY, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0x5E}, {1}, {}));
        // increment
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternA}), encodingImplicit, InstructionOptions({0xBC}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternDirectU8}), encodingU8Operand, InstructionOptions({0xAB}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternDirectIndexedByXU8}), encodingU8Operand, InstructionOptions({0xBB}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0xAC}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternX}), encodingImplicit, InstructionOptions({0x3D}, {}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternY}), encodingImplicit, InstructionOptions({0xFC}, {}, {zero}));
        // decrement
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternA}), encodingImplicit, InstructionOptions({0x9C}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternDirectU8}), encodingU8Operand, InstructionOptions({0x8B}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternDirectIndexedByXU8}), encodingU8Operand, InstructionOptions({0x9B}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0x8C}, {0}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternX}), encodingImplicit, InstructionOptions({0x1D}, {}, {zero}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternY}), encodingImplicit, InstructionOptions({0xDC}, {}, {zero}));
        // bitwise negation
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::BitwiseNegation, 0, {patternA}), encodingImplicit, InstructionOptions({0x48, 0xFF}, {}, {}));
        // signed negation
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::BitwiseNegation, 0, {patternA}), encodingImplicit, InstructionOptions({0x48, 0xFF, 0x60, 0x88, 0x01}, {}, {}));
        // bitshifts
        {
            const std::pair<InstructionType, std::uint8_t> shiftOperators[] {
                {BinaryOperatorKind::LeftShift, 0x00},
                {BinaryOperatorKind::LogicalLeftShift, 0x00},
                {BinaryOperatorKind::LeftRotateWithCarry, 0x20},
                {BinaryOperatorKind::LogicalRightShift, 0x40},
                {BinaryOperatorKind::RightRotateWithCarry, 0x60},
            };
            for (const auto& op : shiftOperators) {
                builtins.createInstruction(InstructionSignature(op.first, 0, {patternA, patternImmU8}), encodingRepeatedImplicit, InstructionOptions({static_cast<std::uint8_t>(op.second | 0x1C)}, {1}, {}));
                builtins.createInstruction(InstructionSignature(op.first, 0, {patternDirectU8, patternImmU8}), encodingRepeatedU8Operand, InstructionOptions({static_cast<std::uint8_t>(op.second | 0x0B)}, {0, 1}, {}));
                builtins.createInstruction(InstructionSignature(op.first, 0, {patternDirectIndexedByXU8, patternImmU8}), encodingRepeatedU8Operand, InstructionOptions({static_cast<std::uint8_t>(op.second | 0x1B)}, {0, 1}, {}));
                builtins.createInstruction(InstructionSignature(op.first, 0, {patternAbsoluteU8, patternImmU8}), encodingRepeatedU16Operand, InstructionOptions({static_cast<std::uint8_t>(op.second | 0x0C)}, {0, 1}, {}));
            }
        }
        // xcn (swap nybbles in a)
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap_digits)), 0, {patternA}), encodingImplicit, InstructionOptions({0x9F}, {}, {}));
        // ya = dp16
        // dp16 = ya
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternYA, patternDirectU16}), encodingU8Operand, InstructionOptions({0xBA}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU16, patternYA}), encodingU8Operand, InstructionOptions({0xDA}, {0}, {}));
        // ++dp16
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreIncrement, 0, {patternDirectU16}), encodingU8Operand, InstructionOptions({0x3A}, {0}, {zero}));
        // --dp16
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::PreDecrement, 0, {patternDirectU16}), encodingU8Operand, InstructionOptions({0x1A}, {0}, {zero}));
        // ya += dp16
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Addition, 0, {patternYA, patternDirectU16}), encodingU8Operand, InstructionOptions({0x7A}, {1}, {}));
        // ya -= dp16
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Subtraction, 0, {patternYA, patternDirectU16}), encodingU8Operand, InstructionOptions({0x9A}, {1}, {}));
        // cmp ya, dp16
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp), 0, {patternYA, patternDirectU16}), encodingU8Operand, InstructionOptions({0x5A}, {1}, {}));
        // ya = y * a
        // ya = a * y;
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Multiplication, 0, {patternYA, patternY, patternA}), encodingImplicit, InstructionOptions({0xCF}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Multiplication, 0, {patternYA, patternA, patternY}), encodingImplicit, InstructionOptions({0xCF}, {}, {}));
        // divmod(ya, x) // div ya, x -> y = result_mod, a = result_div
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(divmod), 0, {patternYA, patternX}), encodingImplicit, InstructionOptions({0x9E}, {}, {}));
        // daa
        // das
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(decimal_adjust_add), 0, {}), encodingImplicit, InstructionOptions({0xDF}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(decimal_adjust_sub), 0, {}), encodingImplicit, InstructionOptions({0xBE}, {}, {}));
        // jump / branch instructions
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16}), encodingPCRelativeI8Operand, InstructionOptions({0x2F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16}), encodingU16Operand, InstructionOptions({0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternAbsoluteIndexedByXIndirectU16}), encodingU16Operand, InstructionOptions({0x1F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternCarry, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0x90}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternCarry, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0xB0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternZero, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0xD0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternZero, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0xF0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternNegative, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0x10}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternNegative, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0x30}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0x50}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0x70}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternDirectU8BitIndex, patternFalse}), encodingU8OperandBitIndexBranch, InstructionOptions({0x13}, {0, 0, 1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternDirectU8BitIndex, patternTrue}), encodingU8OperandBitIndexBranch, InstructionOptions({0x03}, {0, 0, 1}, {}));
        // long branch instructions
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternCarry, patternFalse}), encodingU16Operand, InstructionOptions({0xB0, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternCarry, patternTrue}), encodingU16Operand, InstructionOptions({0x90, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternZero, patternFalse}), encodingU16Operand, InstructionOptions({0xF0, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternZero, patternTrue}), encodingU16Operand, InstructionOptions({0xD0, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternNegative, patternFalse}), encodingU16Operand, InstructionOptions({0x30, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternNegative, patternTrue}), encodingU16Operand, InstructionOptions({0x10, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternOverflow, patternFalse}), encodingU16Operand, InstructionOptions({0x70, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternOverflow, patternTrue}), encodingU16Operand, InstructionOptions({0x50, 3, 0x5F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternDirectU8BitIndex, patternFalse}), encodingU8OperandBitIndexBranch, InstructionOptions({0x03, 3, 0x5F}, {0, 0, 1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternDirectU8BitIndex, patternTrue}), encodingU8OperandBitIndexBranch, InstructionOptions({0x13, 3, 0x5F}, {0, 0, 1}, {}));
        // compare branch not equal
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp_branch_not_equal), 0, {patternA, patternDirectU8, patternImmU16}), encodingU8OperandPCRelativeI8Operand, InstructionOptions({0x2E}, {1, 2}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(cmp_branch_not_equal), 0, {patternA, patternDirectIndexedByXU8, patternImmU16}), encodingU8OperandPCRelativeI8Operand, InstructionOptions({0xDE}, {1, 2}, {}));
        // decrement and branch
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(dec_branch_not_zero), 0, {patternY, patternImmU16}), encodingPCRelativeI8Operand, InstructionOptions({0xFE}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(dec_branch_not_zero), 0, {patternDirectU8, patternImmU16}), encodingU8OperandPCRelativeI8Operand, InstructionOptions({0x6E}, {0, 1}, {}));
        // call instructions
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmHighPageAddress}), encodingU8Operand, InstructionOptions({0x4F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16}), encodingU16Operand, InstructionOptions({0x3F}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall0}), encodingImplicit, InstructionOptions({0x01}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall1}), encodingImplicit, InstructionOptions({0x11}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall2}), encodingImplicit, InstructionOptions({0x21}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall3}), encodingImplicit, InstructionOptions({0x31}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall4}), encodingImplicit, InstructionOptions({0x41}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall5}), encodingImplicit, InstructionOptions({0x51}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall6}), encodingImplicit, InstructionOptions({0x61}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall7}), encodingImplicit, InstructionOptions({0x71}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall8}), encodingImplicit, InstructionOptions({0x81}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCall9}), encodingImplicit, InstructionOptions({0x91}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallA}), encodingImplicit, InstructionOptions({0xA1}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallB}), encodingImplicit, InstructionOptions({0xB1}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallC}), encodingImplicit, InstructionOptions({0xC1}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallD}), encodingImplicit, InstructionOptions({0xD1}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallE}), encodingImplicit, InstructionOptions({0xE1}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternTableCallF}), encodingImplicit, InstructionOptions({0xF1}, {}, {}));
        // return instructions
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0}), encodingImplicit, InstructionOptions({0x6F}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::IrqReturn, 0, {patternAtLeast0}), encodingImplicit, InstructionOptions({0x7F}, {}, {}));
        // brk (push pc, push psw, jmp [0xFFDE])
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(irqcall), 0, {}), encodingImplicit, InstructionOptions({0x0F}, {}, {}));
        // push
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(push), 0, {patternA}), encodingImplicit, InstructionOptions({0x2D}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(push), 0, {patternX}), encodingImplicit, InstructionOptions({0x4D}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(push), 0, {patternY}), encodingImplicit, InstructionOptions({0x6D}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(push), 0, {patternPSW}), encodingImplicit, InstructionOptions({0x0D}, {}, {}));
        // pop
        builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(pop), 0, {patternA}), encodingImplicit, InstructionOptions({0xAE}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(pop), 0, {patternX}), encodingImplicit, InstructionOptions({0xCE}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(pop), 0, {patternY}), encodingImplicit, InstructionOptions({0xEE}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(pop), 0, {patternPSW}), encodingImplicit, InstructionOptions({0x8E}, {}, {}));
        // carry - clrc/setc, notc
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternCarry, patternFalse}), encodingImplicit, InstructionOptions({0x60}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternCarry, patternTrue}), encodingImplicit, InstructionOptions({0x80}, {}, {}));
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::LogicalNegation, 0, {patternCarry}), encodingImplicit, InstructionOptions({0xED}, {}, {}));
        // overflow - clrv
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternOverflow, patternFalse}), encodingImplicit, InstructionOptions({0xE0}, {}, {}));
        // direct_page - clrp/setp
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectPage, patternFalse}), encodingImplicit, InstructionOptions({0x20}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectPage, patternTrue}), encodingImplicit, InstructionOptions({0x40}, {}, {}));
        // interrupt - di/ei
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternInterrupt, patternFalse}), encodingImplicit, InstructionOptions({0xC0}, {}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternInterrupt, patternTrue}), encodingImplicit, InstructionOptions({0xA0}, {}, {}));
        // clr1 dp$bit
        // set1 dp$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8BitIndex, patternFalse}), encodingU8OperandBitIndex, InstructionOptions({0x12}, {0, 0, 1}, {}));
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternDirectU8BitIndex, patternTrue}), encodingU8OperandBitIndex, InstructionOptions({0x02}, {0, 0, 1}, {}));
        // tclr1 abs
        // tset1 abs
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(test_and_clear), 0, {patternA, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0x4E}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(test_and_set), 0, {patternA, patternAbsoluteU8}), encodingU16Operand, InstructionOptions({0x0E}, {1}, {}));
        // carry &= mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::BitwiseAnd, 0, {patternCarry, patternAbsoluteU8BitIndex}), encodingU13OperandBitIndex, InstructionOptions({0x4A}, {1, 1, 1}, {}));
        // carry &= !mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::BitwiseAnd, 0, {patternCarry, patternAbsoluteU8BitIndexNot}), encodingU13OperandBitIndex, InstructionOptions({0x6A}, {1, 1, 1}, {}));
        // carry |= mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::BitwiseOr, 0, {patternCarry, patternAbsoluteU8BitIndex}), encodingU13OperandBitIndex, InstructionOptions({0x0A}, {1, 1, 1}, {}));
        // carry |= !mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::BitwiseOr, 0, {patternCarry, patternAbsoluteU8BitIndexNot}), encodingU13OperandBitIndex, InstructionOptions({0x2A}, {1, 1, 1}, {}));
        // carry ^= mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::BitwiseXor, 0, {patternCarry, patternAbsoluteU8BitIndex}), encodingU13OperandBitIndex, InstructionOptions({0x8A}, {1, 1, 1}, {}));
        // mem$bit = !mem$bit
        builtins.createInstruction(InstructionSignature(UnaryOperatorKind::LogicalNegation, 0, {patternAbsoluteU8BitIndex}), encodingU13OperandBitIndex, InstructionOptions({0xEA}, {0, 0, 1}, {}));
        // carry = mem$bit
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternCarry, patternAbsoluteU8BitIndex}), encodingU13OperandBitIndex, InstructionOptions({0xAA}, {1, 1, 1}, {}));
        // mem$bit = carry
        builtins.createInstruction(InstructionSignature(BinaryOperatorKind::Assignment, 0, {patternAbsoluteU8BitIndex, patternCarry}), encodingU13OperandBitIndex, InstructionOptions({0xCA}, {0, 0, 1}, {}));
        // nop
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(nop), 0, {}), encodingImplicit, InstructionOptions({0x00}, {}, {}));
        // sleep
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(sleep), 0, {}), encodingImplicit, InstructionOptions({0xEF}, {}, {}));
        // stop
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(stop), 0, {}), encodingImplicit, InstructionOptions({0xFF}, {}, {}));
    }

    Definition* Spc700Platform::getPointerSizedType() const {
        return pointerSizedType;
    }

    Definition* Spc700Platform::getFarPointerSizedType() const {
        return farPointerSizedType;
    }

    std::unique_ptr<PlatformTestAndBranch> Spc700Platform::getTestAndBranch(const Compiler& compiler, const Definition* type, BinaryOperatorKind op, const Expression* left, const Expression* right, std::size_t distanceHint) const {
        switch (op) {
            case BinaryOperatorKind::Equal:
            case BinaryOperatorKind::NotEqual: {
                if (const auto leftUnary = left->tryGet<Expression::UnaryOperator>()) {
                    const auto innerOp = leftUnary->op;
                    const auto innerOperand = leftUnary->operand.get();

                    // --operand != 0 -> decrement_branch_not_zero(operand, dest)
                    if (innerOp == UnaryOperatorKind::PreDecrement) {
                        if (const auto outerRightImmediate = right->tryGet<Expression::IntegerLiteral>()) {
                            if (op == BinaryOperatorKind::NotEqual && outerRightImmediate->value.isZero()) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(dec_branch_not_zero),
                                    std::vector<const Expression*> {innerOperand},
                                    std::vector<PlatformBranch> {}
                                );
                            }
                        }
                    }
                }

                // a != right -> compare_branch(a, right, dest)
                if (distanceHint == 0 && op == BinaryOperatorKind::NotEqual) {
                    if (const auto leftRegister = left->tryGet<Expression::ResolvedIdentifier>()) {
                        if (leftRegister->definition == a) {
                            std::vector<InstructionOperandRoot> operandRoots;
                            operandRoots.push_back(InstructionOperandRoot(left, makeFwdUnique<InstructionOperand>(InstructionOperand::Register(a))));
                            operandRoots.push_back(InstructionOperandRoot(right, compiler.createOperandFromExpression(right, true)));
                            operandRoots.push_back(InstructionOperandRoot(nullptr, makeFwdUnique<InstructionOperand>(InstructionOperand::Integer(Int128(0x1234))))); 

                            if (compiler.getBuiltins().selectInstruction(InstructionType::VoidIntrinsic(cmp_branch_not_equal), 0, operandRoots)) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp_branch_not_equal),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> {}
                                );
                            }
                        }
                    }
                }

                // left == right -> { cmp(left, right); } && zero
                return std::make_unique<PlatformTestAndBranch>(
                    InstructionType::VoidIntrinsic(cmp),
                    std::vector<const Expression*> {left, right},
                    std::vector<PlatformBranch> { PlatformBranch(zero, op == BinaryOperatorKind::Equal, true) }
                );
            }
            case BinaryOperatorKind::LessThan: 
            case BinaryOperatorKind::GreaterThanOrEqual: {
                if (const auto integerType = type->tryGet<Definition::BuiltinIntegerType>()) {                    
                    if (integerType->min.isNegative()) {
                        // left < 0 -> { cmp(left, right); } && negative
                        // left >= 0 -> { cmp(left, right); } && !negative
                        if (const auto rightImmediate = right->tryGet<Expression::IntegerLiteral>()) {
                            if (rightImmediate->value.isZero()) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> { PlatformBranch(negative, op == BinaryOperatorKind::LessThan, true) }
                                );
                            }
                        }
                    } else {
                        // left < right -> { cmp(left, right); } && !carry
                        // left >= right -> { cmp(left, right); } && carry
                        return std::make_unique<PlatformTestAndBranch>(
                            InstructionType::VoidIntrinsic(cmp),
                            std::vector<const Expression*> {left, right},
                            std::vector<PlatformBranch> { PlatformBranch(carry, op == BinaryOperatorKind::GreaterThanOrEqual, true) }
                        );
                    }
                }

                return nullptr;
            }
            case BinaryOperatorKind::LessThanOrEqual: {
                if (const auto integerType = type->tryGet<Definition::BuiltinIntegerType>()) {
                    if (integerType->min.isNegative()) {
                        // left <= 0 -> { cmp(left, right); } && (zero || negative)
                        if (const auto rightImmediate = right->tryGet<Expression::IntegerLiteral>()) {
                            if (rightImmediate->value.isZero()) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> {
                                        PlatformBranch(zero, true, true),
                                        PlatformBranch(negative, true, true)
                                    }
                                );
                            }
                        }
                    } else {
                        // left <= 0 -> { cmp(left, right); } && (zero || !carry)
                        return std::make_unique<PlatformTestAndBranch>(
                            InstructionType::VoidIntrinsic(cmp),
                            std::vector<const Expression*> {left, right},
                            std::vector<PlatformBranch> {
                                PlatformBranch(zero, true, true),
                                PlatformBranch(carry, false, true)
                            }
                        );
                    }
                }

                return nullptr;
            }
            case BinaryOperatorKind::GreaterThan: {
                if (const auto integerType = type->tryGet<Definition::BuiltinIntegerType>()) {
                    if (integerType->min.isNegative()) {
                        // left > 0 -> { cmp(left, right); } && !zero && !negative
                        if (const auto rightImmediate = right->tryGet<Expression::IntegerLiteral>()) {
                            if (rightImmediate->value.isZero()) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> {
                                        PlatformBranch(zero, true, false),
                                        PlatformBranch(negative, false, true)
                                    }
                                );
                            }
                        }
                    } else {
                        // left > right -> { cmp(left, right); } && !zero && carry
                        return std::make_unique<PlatformTestAndBranch>(
                            InstructionType::VoidIntrinsic(cmp),
                            std::vector<const Expression*> {left, right},
                            std::vector<PlatformBranch> {
                                PlatformBranch(zero, true, false),
                                PlatformBranch(carry, true, true)
                            }
                        );
                    }
                }

                return nullptr;
            }
            default: return nullptr;
        }
    }

    Definition* Spc700Platform::getZeroFlag() const {
        return zero;
    }

    Int128 Spc700Platform::getPlaceholderValue() const {
        return Int128(UINT64_C(0xCCCCCCCCCCCCCCCC));
    }
}
