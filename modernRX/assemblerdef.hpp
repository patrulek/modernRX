#pragma once

/*
* Defines types, constants and helper functions used by assembler.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <bit>

namespace modernRX::assembler {
    using reg_idx_t = uint8_t;

    enum class RegisterType : uint8_t {
        GPR = 0, XMM = 1, YMM = 2, ZMM = 4,
        DUMMY = 255
    };

    enum class PP : uint8_t {
        PP0x00 = 0, PP0x66 = 1, PP0xF3 = 2, PP0xF2 = 3
    };

    enum class MOD : uint8_t {
        MOD00 = 0, MOD01 = 1, MOD10 = 2, MOD11 = 3
    };

    enum class MM : uint8_t {
        MM0x0F = 1, MM0x0F38 = 2, MM0x0F3A = 3
    };

    enum class SCALE : uint8_t {
        SS1 = 0, SS2 = 1, SS4 = 2, SS8 = 3
    };

    struct Opcode {
        uint8_t code;
        int8_t mod{ -1 }; // -1 - no value; cannot be std::optional because of language limitations.
    };

    struct Memory {
        reg_idx_t reg;
        reg_idx_t index_reg{ 0xff };
        int32_t offset;
        bool rip{ false };

        // Should be used only when mem.reg is RBP.
        [[nodiscard]] static constexpr Memory RIP(const Memory& mem) noexcept {
            return Memory{ mem.reg, mem.index_reg, mem.offset, true };
        }

        [[nodiscard]] constexpr bool isLow() const noexcept {
            return reg < 8;
        }

        [[nodiscard]] constexpr bool isHigh() const noexcept {
            return reg >= 8;
        }

        [[nodiscard]] constexpr reg_idx_t lowIdx() const noexcept {
            return reg % 8;
        }

        [[nodiscard]] constexpr bool operator==(const Memory& rhs) const noexcept {
            return reg == rhs.reg && offset == rhs.offset && index_reg == rhs.index_reg;
        }
    };

    struct Register {
        RegisterType type;
        reg_idx_t idx;

        [[nodiscard]] static constexpr Register GPR(const reg_idx_t idx) noexcept {
            return Register{ RegisterType::GPR, idx };
        }

        [[nodiscard]] static constexpr Register YMM(const reg_idx_t idx) noexcept {
            return Register{ RegisterType::YMM, idx };
        }

        [[nodiscard]] static constexpr Register XMM(const reg_idx_t idx) noexcept {
            return Register{ RegisterType::XMM, idx };
        }

        [[nodiscard]] constexpr bool isLow() const noexcept {
            return idx < 8;
        }

        [[nodiscard]] constexpr bool isHigh() const noexcept {
            return idx >= 8;
        }

        [[nodiscard]] constexpr reg_idx_t lowIdx() const noexcept {
            return idx % 8;
        }

        [[nodiscard]] constexpr int32_t size() const noexcept {
            if (type == RegisterType::GPR) {
                return 8;
            } else if (type == RegisterType::XMM) {
                return 16;
            } else if (type == RegisterType::YMM) {
                return 32;
            } else if (type == RegisterType::ZMM) {
                return 64;
            }

            std::unreachable();
        }

        [[nodiscard]] constexpr bool operator==(const Register& rhs) const noexcept {
            return type == rhs.type && idx == rhs.idx;
        }

        [[nodiscard]] constexpr const Memory operator[](const int32_t offset) const noexcept {
            return Memory{ idx, 0xff, offset, false };
        }

        [[nodiscard]] constexpr const Memory operator[](const Memory& offset) const noexcept {
            return Memory{ idx, offset.reg, offset.offset, offset.rip };
        }
    };

    namespace registers {
        inline constexpr Register DUMMY{ RegisterType::DUMMY, 0xff };

        inline constexpr Register RAX{ RegisterType::GPR, 0 };
        inline constexpr Register RCX{ RegisterType::GPR, 1 };
        inline constexpr Register RDX{ RegisterType::GPR, 2 };
        inline constexpr Register RBX{ RegisterType::GPR, 3 };
        inline constexpr Register RSP{ RegisterType::GPR, 4 };
        inline constexpr Register RBP{ RegisterType::GPR, 5 };
        inline constexpr Register RSI{ RegisterType::GPR, 6 };
        inline constexpr Register RDI{ RegisterType::GPR, 7 };
        inline constexpr Register R08{ RegisterType::GPR, 8 };
        inline constexpr Register R09{ RegisterType::GPR, 9 };
        inline constexpr Register R10{ RegisterType::GPR, 10 };
        inline constexpr Register R11{ RegisterType::GPR, 11 };
        inline constexpr Register R12{ RegisterType::GPR, 12 };
        inline constexpr Register R13{ RegisterType::GPR, 13 };
        inline constexpr Register R14{ RegisterType::GPR, 14 };
        inline constexpr Register R15{ RegisterType::GPR, 15 };

        inline constexpr Register XMM0{ RegisterType::XMM, 0 };
        inline constexpr Register XMM1{ RegisterType::XMM, 1 };
        inline constexpr Register XMM2{ RegisterType::XMM, 2 };
        inline constexpr Register XMM3{ RegisterType::XMM, 3 };
        inline constexpr Register XMM4{ RegisterType::XMM, 4 };
        inline constexpr Register XMM5{ RegisterType::XMM, 5 };
        inline constexpr Register XMM6{ RegisterType::XMM, 6 };
        inline constexpr Register XMM7{ RegisterType::XMM, 7 };
        inline constexpr Register XMM8{ RegisterType::XMM, 8 };
        inline constexpr Register XMM9{ RegisterType::XMM, 9 };
        inline constexpr Register XMM10{ RegisterType::XMM, 10 };
        inline constexpr Register XMM11{ RegisterType::XMM, 11 };
        inline constexpr Register XMM12{ RegisterType::XMM, 12 };
        inline constexpr Register XMM13{ RegisterType::XMM, 13 };
        inline constexpr Register XMM14{ RegisterType::XMM, 14 };
        inline constexpr Register XMM15{ RegisterType::XMM, 15 };

        inline constexpr Register YMM0{ RegisterType::YMM, 0 };
        inline constexpr Register YMM1{ RegisterType::YMM, 1 };
        inline constexpr Register YMM2{ RegisterType::YMM, 2 };
        inline constexpr Register YMM3{ RegisterType::YMM, 3 };
        inline constexpr Register YMM4{ RegisterType::YMM, 4 };
        inline constexpr Register YMM5{ RegisterType::YMM, 5 };
        inline constexpr Register YMM6{ RegisterType::YMM, 6 };
        inline constexpr Register YMM7{ RegisterType::YMM, 7 };
        inline constexpr Register YMM8{ RegisterType::YMM, 8 };
        inline constexpr Register YMM9{ RegisterType::YMM, 9 };
        inline constexpr Register YMM10{ RegisterType::YMM, 10 };
        inline constexpr Register YMM11{ RegisterType::YMM, 11 };
        inline constexpr Register YMM12{ RegisterType::YMM, 12 };
        inline constexpr Register YMM13{ RegisterType::YMM, 13 };
        inline constexpr Register YMM14{ RegisterType::YMM, 14 };
        inline constexpr Register YMM15{ RegisterType::YMM, 15 };
    }

    // generates 3rd byte of VEX prefix.
    // Len = 1 - 256-bit instruction.
    // Len = 0 - 128-bit instruction.
    // In case when using 2-byte VEX prefix, this function may be used to generate 2nd byte.
    // In such case, W is always ignored and the w param is in fact an R.
    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret vex3(const reg_idx_t src1, const PP pp, const int w = 0, const uint8_t len = 1) noexcept {
        // bit        7    6    5    4    3    2    1    0
        //            W/R    v̅3 v̅2 v̅1 v̅0 L    p1    p0
        uint8_t src_val = ~src1; // 1's complement; 15 -> ymm0, 14 -> ymm1 ... 0 -> ymm15
        src_val &= 0x0f; // zero out upper bits
        return static_cast<Ret>((w << 7) | (src_val << 3) | (len << 2) | static_cast<uint8_t>(pp));
    }

    // generates 2rd byte of VEX prefix.
    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret vex2(const MM mm, const int r = 0, const int x = 0, const int b = 0) noexcept {
        // bit        7    6    5    4    3    2    1    0
        //            R̅     X̅     B̅     m4     m3     m2     m1     m0

        return static_cast<Ret>((r << 7) | (x << 6) | (b << 5) | static_cast<uint8_t>(mm));
    }

    // generates 2rd byte of VEX prefix.
    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret vex2(const reg_idx_t src1, const PP pp, const int r = 0, const uint8_t len = 1) noexcept {
        // bit        7    6    5    4    3    2    1    0
        //            R    v̅3 v̅2 v̅1 v̅0 L    p1    p0
        uint8_t src_val = ~src1; // 1's complement; 15 -> ymm0, 14 -> ymm1 ... 0 -> ymm15
        src_val &= 0x0f; // zero out upper bits
        return static_cast<Ret>((r << 7) | (src_val << 3) | (len << 2) | static_cast<uint8_t>(pp));
    }

    // generates 1rd byte of VEX prefix.
    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret vex1(const bool three_byte = true) noexcept {
        return static_cast<Ret>(three_byte ? 0xc4 : 0xc5);
    }

    // by default uses MOD11 (register addressing)
    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret modregrm(const reg_idx_t reg, const reg_idx_t rm, const MOD mod = MOD::MOD11) noexcept {
        // MOD = 11 (register addressing), REG = dst_reg, R/M = src_reg
        return static_cast<Ret>((static_cast<uint8_t>(mod) << 6) | (reg << 3) | rm);
    }

    template<typename Ret = std::byte>
    [[nodiscard]] constexpr Ret rex(const int w = 0, const int r = 0, const int x = 0, const int b = 0) noexcept {
        // bit        7    6    5    4    3    2    1    0
        //            0   1   0   0   W   R   X   B
        return static_cast<Ret>((0b0100 << 4) | (w << 3) | (r << 2) | (x << 1) | b);
    }

    // Index cannot be 4.
    template <typename Ret = std::byte>
    [[nodiscard]] constexpr Ret sib(const SCALE scale, const Register index, const Register base) noexcept {

        // bit        7    6    5    4    3    2    1    0
        //            S   S      I   I   I   B   B   B  ; S - scale; I - index; B - base
        return static_cast<Ret>((static_cast<uint8_t>(scale) << 6) | (index.lowIdx() << 3) | base.lowIdx());
    }

    template<int Byte, typename Val>
    [[nodiscard]] constexpr std::byte byte(const Val& v) noexcept {
        static_assert(Byte < sizeof(Val));
        return *(reinterpret_cast<const std::byte*>(&v) + Byte);
    }
}
