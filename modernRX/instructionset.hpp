#pragma once

/*
* Superscalar instruction set information: https://github.com/tevador/RandomX/blob/master/doc/specs.md#61-instructions
*/

#include <algorithm>
#include <array>
#include <optional>

namespace modernRX {
    // Defines simulated CPU execution ports with all possible configurations.
    enum class ExecutionPort : uint8_t {
        NONE = 0, P5 = 1, P0 = 2, P1 = 4,
        P01 = P0 | P1, P05 = P0 | P5, P15 = P1 | P5, P015 = P0 | P1 | P5,
    };

    // Defines all instruction types used in superscalar programs. Order must be preserved.
    enum class SuperscalarInstructionType : uint8_t {
                                                                //uOPs (decode)   execution ports         latency       code size
        ISUB_R = 0,                                             //1               p015                    1             3 (vsub)
        IXOR_R = 1,                                             //1               p015                    1             3 (xor)
        IADD_RS = 2,                                            //1               p01                     1             4 (lea)
        IMUL_R = 3,                                             //1               p1                      3             4 (imul)
        IROR_C = 4,                                             //1               p05                     1             4 (ror)
        IADD_C7 = 5,                                            //1               p015                    1             7 (vadd)
        IXOR_C7 = 6,                                            //1               p015                    1             7 (xor)
        IADD_C8 = 7,                                            //1+0             p015                    1             7+1 (vadd+nop)
        IXOR_C8 = 8,                                            //1+0             p015                    1             7+1 (xor+nop)
        IADD_C9 = 9,                                            //1+0             p015                    1             7+2 (vadd+nop)
        IXOR_C9 = 10,                                           //1+0             p015                    1             7+2 (xor+nop)
        IMULH_R = 11,                                           //1+2+1           0+(p1,p5)+0             3             3+3+3 (mov+vmul+mov)
        ISMULH_R = 12,                                          //1+2+1           0+(p1,p5)+0             3             3+3+3 (mov+imul+mov)
        IMUL_RCP = 13,                                          //1+1             p015+p1                 4             10+4 (mov+imul)

        INVALID = 14,
    };

    // Holds information about single macro operation.
    struct MacroOp {
        std::array<ExecutionPort, 2> ports{ ExecutionPort::NONE, ExecutionPort::NONE }; // If MacroOp consists of 2 uOps, second execution port will not be NONE.
        uint8_t size{ 0 }; // Size in bytes.
        uint8_t latency{ 0 }; // Latency in CPU clock cycles.
        bool dependent{ false }; // Is dependent on previous macro op.

        // Returns true if operation needs to be scheduled at any ExectionPort, false otherwise (eg. eliminated MOV instructions does not require any port).
        [[nodiscard]] bool requiresPort() const noexcept {
            return ports[0] != ExecutionPort::NONE;
        }
        
        // Returns true if macro-op is fused from two uOps.
        [[nodiscard]] bool fused() const noexcept {
            return ports[1] != ExecutionPort::NONE;
        }
    };

    // Holds common information about single instruction.
    struct SuperscalarInstructionInfo {
        std::array<MacroOp, 4> ops{}; // Macro operations instruction consists of.
        SuperscalarInstructionType type{ SuperscalarInstructionType::INVALID }; // Superscalar instruction type.
        SuperscalarInstructionType group{ SuperscalarInstructionType::INVALID }; // Superscalar instruction group type.

        std::optional<uint8_t> src_op_index{ std::nullopt }; // Defines which macro op requires source register (nullopt if source not required).
        uint8_t dst_op_index{ 0 }; // Defines which macro op requires destination register.
        uint8_t result_op_index{ 0 }; // Defines which macro op requires to vstore result (update register).

        bool src_register_as_src_value{ false }; // Specifies whether source register should be used as a source value.
        bool dst_register_as_src_register{ false }; // Defines whether destination register can be used as a source register.
    };

    // Holds superscalar instruction templates.
    using SuperscalarInstructionSet = std::array<SuperscalarInstructionInfo, 15>;

    // Superscalar instruction set.
    inline constexpr SuperscalarInstructionSet isa = []() consteval {
        constexpr SuperscalarInstructionSet isa_{
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::ISUB_R },
                .group{ SuperscalarInstructionType::IADD_RS },
                .src_op_index{ 0 },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IXOR_R },
                .group{ SuperscalarInstructionType::IXOR_R },
                .src_op_index{ 0 },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P01, ExecutionPort::NONE },
                        .size{ 4 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IADD_RS },
                .group{ SuperscalarInstructionType::IADD_RS },
                .src_op_index{ 0 },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ false }, // According to specification this should be true, but original implementation does not support that.
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P1, ExecutionPort::NONE },
                        .size{ 4 },
                        .latency{ 3 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IMUL_R },
                .group{ SuperscalarInstructionType::IMUL_R },
                .src_op_index{ 0 },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P05, ExecutionPort::NONE },
                        .size{ 4 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IROR_C },
                .group{ SuperscalarInstructionType::IROR_C },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 7 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IADD_C7 },
                .group{ SuperscalarInstructionType::IADD_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 7 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IXOR_C7 },
                .group{ SuperscalarInstructionType::IXOR_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 8 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IADD_C8 },
                .group{ SuperscalarInstructionType::IADD_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 8 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IXOR_C8 },
                .group{ SuperscalarInstructionType::IXOR_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 9 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IADD_C9 },
                .group{ SuperscalarInstructionType::IADD_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 9 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IXOR_C9 },
                .group{ SuperscalarInstructionType::IXOR_C7 },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::NONE, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 0 },
                        .dependent{ false },
                    },
                    MacroOp{
                        .ports{ ExecutionPort::P1, ExecutionPort::P5 },
                        .size{ 3 },
                        .latency{ 4 },
                        .dependent{ false },
                    },
                    MacroOp{
                        .ports{ ExecutionPort::NONE, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 0 },
                        .dependent{ false },
                    },
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IMULH_R },
                .group{ SuperscalarInstructionType::ISMULH_R },
                .src_op_index{ 1 },
                .dst_op_index{ 0 },
                .result_op_index{ 1 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ true },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::NONE, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 0 },
                        .dependent{ false },
                    },
                    MacroOp{
                        .ports{ ExecutionPort::P1, ExecutionPort::P5 },
                        .size{ 3 },
                        .latency{ 4 },
                        .dependent{ false },
                    },
                    MacroOp{
                        .ports{ ExecutionPort::NONE, ExecutionPort::NONE },
                        .size{ 3 },
                        .latency{ 0 },
                        .dependent{ false },
                    },
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::ISMULH_R },
                .group{ SuperscalarInstructionType::ISMULH_R },
                .src_op_index{ 1 },
                .dst_op_index{ 0 },
                .result_op_index{ 1 },
                .src_register_as_src_value{ true },
                .dst_register_as_src_register{ true },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{
                        .ports{ ExecutionPort::P015, ExecutionPort::NONE },
                        .size{ 10 },
                        .latency{ 1 },
                        .dependent{ false },
                    },
                    MacroOp{
                        .ports{ ExecutionPort::P1, ExecutionPort::NONE },
                        .size{ 4 },
                        .latency{ 3 },
                        .dependent{ true },
                    },
                    MacroOp{},
                    MacroOp{},
                },
                .type{ SuperscalarInstructionType::IMUL_RCP },
                .group{ SuperscalarInstructionType::IMUL_RCP },
                .src_op_index{ std::nullopt },
                .dst_op_index{ 1 },
                .result_op_index{ 1 },
                .src_register_as_src_value{ false },
                .dst_register_as_src_register{ false },
            },
            SuperscalarInstructionInfo{
                .ops = std::array<MacroOp, 4>{
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                    MacroOp{},
                },
                .type = SuperscalarInstructionType::INVALID,
                .src_op_index{ 0 },
                .dst_op_index{ 0 },
                .result_op_index{ 0 },
            },
        };

        // This is just to ensure that enum values points to proper instruction templates.
        for (uint32_t i = 0; i < isa_.size(); ++i) {
            if (isa_[i].type != static_cast<SuperscalarInstructionType>(i)) {
                throw "Array index must have equal value to underlying instruction type.";
            }
        }

        return isa_;
    }();

    // Returns true for all types that indicate multiplications.
    constexpr bool isMultiplication(const SuperscalarInstructionType type) {
        return type == SuperscalarInstructionType::IMUL_R || type == SuperscalarInstructionType::IMULH_R
            || type == SuperscalarInstructionType::ISMULH_R || type == SuperscalarInstructionType::IMUL_RCP;
    }

    // Finds maximum latency of all operations in the instruction set.
    // Must be 4 for reference CPU (Ivy Bridge).
    consteval uint8_t maxOpLatency(const SuperscalarInstructionSet& isa) {
        uint8_t max_latency{ 0 };

        for (const auto& info : isa) {
            for (const auto& op : info.ops) {
                max_latency = std::max(max_latency, op.latency);
            }
        };

        if (max_latency != 4) {
            throw "Maximum latency of all operations in the instruction set must be 4 for reference CPU (Ivy Bridge).";
        }

        return max_latency;
    }
}
