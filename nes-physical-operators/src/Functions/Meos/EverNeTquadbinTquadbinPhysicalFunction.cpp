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

#include <Functions/Meos/EverNeTquadbinTquadbinPhysicalFunction.hpp>

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

EverNeTquadbinTquadbinPhysicalFunction::EverNeTquadbinTquadbinPhysicalFunction(PhysicalFunction cellFunction,
                                                          PhysicalFunction tsFunction,
                                                          PhysicalFunction cell0Function,
                                                          PhysicalFunction ts0Function)
{
    parameterFunctions.reserve(4);
    parameterFunctions.push_back(std::move(cellFunction));
    parameterFunctions.push_back(std::move(tsFunction));
    parameterFunctions.push_back(std::move(cell0Function));
    parameterFunctions.push_back(std::move(ts0Function));
}

VarVal EverNeTquadbinTquadbinPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto cell = parameterValues[0].cast<nautilus::val<uint64_t>>();
    auto ts = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto cell0 = parameterValues[2].cast<nautilus::val<uint64_t>>();
    auto ts0 = parameterValues[3].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](uint64_t cell,
            uint64_t ts,
            uint64_t cell0,
            uint64_t ts0) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                Temporal* temp = (Temporal*)tquadbininst_make((Quadbin)cell, (TimestampTz)ts);
                if (!temp) return 0.0;
                Temporal* inst0 = (Temporal*)tquadbininst_make((Quadbin)cell0, (TimestampTz)ts0);
                if (!inst0) { free(temp); return 0.0; }

                double r = ever_ne_tquadbin_tquadbin(temp, inst0);
                free(temp);
                free(inst0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        cell, ts, cell0, ts0);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEverNeTquadbinTquadbinPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 4,
                 "EverNeTquadbinTquadbinPhysicalFunction requires 4 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    auto arg3 = std::move(arguments.childFunctions[3]);
    return EverNeTquadbinTquadbinPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3));
}

} // namespace NES
