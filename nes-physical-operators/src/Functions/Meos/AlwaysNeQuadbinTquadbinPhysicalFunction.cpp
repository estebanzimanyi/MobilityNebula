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

#include <Functions/Meos/AlwaysNeQuadbinTquadbinPhysicalFunction.hpp>

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
#include <utility>
#include <val.hpp>

extern "C" {
#include <meos.h>
#include <meos_quadbin.h>
}

namespace NES {

AlwaysNeQuadbinTquadbinPhysicalFunction::AlwaysNeQuadbinTquadbinPhysicalFunction(PhysicalFunction arg0Function,
                                                          PhysicalFunction cell1Function,
                                                          PhysicalFunction ts1Function)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(arg0Function));
    parameterFunctions.push_back(std::move(cell1Function));
    parameterFunctions.push_back(std::move(ts1Function));
}

VarVal AlwaysNeQuadbinTquadbinPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto arg0 = parameterValues[0].cast<nautilus::val<uint64_t>>();
    auto cell1 = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto ts1 = parameterValues[2].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](uint64_t arg0,
            uint64_t cell1,
            uint64_t ts1) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                Temporal* inst1 = (Temporal*)tquadbininst_make((Quadbin)cell1, (TimestampTz)ts1);
                if (!inst1) { return 0.0; }

                double r = always_ne_quadbin_tquadbin((Quadbin)arg0, inst1);
                free(inst1);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        arg0, cell1, ts1);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterAlwaysNeQuadbinTquadbinPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "AlwaysNeQuadbinTquadbinPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return AlwaysNeQuadbinTquadbinPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
