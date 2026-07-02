/*
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <Functions/Meos/Th3indexCellToParentPhysicalFunction.hpp>

#include <Functions/PhysicalFunction.hpp>
#include <MEOSWrapper.hpp>
#include <Nautilus/DataTypes/VarVal.hpp>
#include <Nautilus/DataTypes/VariableSizedData.hpp>
#include <Nautilus/Interface/Record.hpp>
#include <PhysicalFunctionRegistry.hpp>
#include <ErrorHandling.hpp>
#include <ExecutionContext.hpp>
#include <fmt/format.h>
#include <function.hpp>
#include <string>
#include <string.h>
#include <utility>
#include <val.hpp>

extern "C" {
#include <meos.h>
#include <meos_h3.h>
}

namespace NES {

Th3indexCellToParentPhysicalFunction::Th3indexCellToParentPhysicalFunction(PhysicalFunction cellFunction,
                                                          PhysicalFunction tsFunction,
                                                          PhysicalFunction arg0Function)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(cellFunction));
    parameterFunctions.push_back(std::move(tsFunction));
    parameterFunctions.push_back(std::move(arg0Function));
}

VarVal Th3indexCellToParentPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto cell = parameterValues[0].cast<nautilus::val<uint64_t>>();
    auto ts = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto arg0 = parameterValues[2].cast<nautilus::val<double>>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](uint64_t cell,
            uint64_t ts,
            double arg0,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                Temporal* temp = th3indexinst_make((H3Index)cell, (TimestampTz)ts);
                if (!temp) return 0u;

                Temporal* tres = th3index_cell_to_parent(temp, (int32_t)arg0);
                free(temp);
                if (!tres) return 0u;
                H3Index hcell = th3index_start_value(tres);
                free(tres);
                char* hex = h3index_out(hcell);
                if (!hex) return 0u;
                uint32_t len = static_cast<uint32_t>(strlen(hex));
                if (len > bufMax) len = bufMax;
                memcpy(buf, hex, len);
                free(hex);
                return len;
            }
            catch (const std::exception&)
            {
                return 0u;
            }
        },
        cell, ts, arg0, outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterTh3indexCellToParentPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "Th3indexCellToParentPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return Th3indexCellToParentPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
