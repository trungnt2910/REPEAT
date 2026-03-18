#ifndef REPEAT_ARCH_ARCH_BACKEND_FACTORY_H
#define REPEAT_ARCH_ARCH_BACKEND_FACTORY_H

#include <memory>
#include <type_traits>

#include <llvm/TargetParser/Triple.h>

#include "repeat/arch/aarch64/aarch64_backend.h"
#include "repeat/arch/arm/arm_backend.h"
#include "repeat/arch/i386/i386_backend.h"
#include "repeat/arch/x86_64/x86_64_backend.h"

namespace repeat
{

class ArchBackendFactory
{
public:
    template <typename ELFT>
    static std::unique_ptr<ArchBackend<ELFT>> Create(llvm::Triple::ArchType arch)
    {
        if constexpr (std::is_same_v<ELFT, llvm::object::ELF64LE>)
        {
            if (arch == llvm::Triple::x86_64)
            {
                return std::make_unique<X8664Backend>();
            }
            else if (arch == llvm::Triple::aarch64)
            {
                return std::make_unique<Aarch64Backend>();
            }
        }
        else if constexpr (std::is_same_v<ELFT, llvm::object::ELF32LE>)
        {
            if (arch == llvm::Triple::x86)
            {
                return std::make_unique<I386Backend>();
            }
            else if (arch == llvm::Triple::arm)
            {
                return std::make_unique<ArmBackend>();
            }
        }
        return nullptr;
    }
};

} // namespace repeat

#endif // REPEAT_ARCH_ARCH_BACKEND_FACTORY_H
