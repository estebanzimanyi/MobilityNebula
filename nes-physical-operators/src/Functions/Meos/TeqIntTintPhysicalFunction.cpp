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

#include <Functions/Meos/TeqIntTintPhysicalFunction.hpp>

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
#include <meos_geo.h>
}

namespace NES {

TeqIntTintPhysicalFunction::TeqIntTintPhysicalFunction(PhysicalFunction arg0Function,
                                                          PhysicalFunction valueFunction,
                                                          PhysicalFunction tsFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(arg0Function));
    parameterFunctions.push_back(std::move(valueFunction));
    parameterFunctions.push_back(std::move(tsFunction));
}

VarVal TeqIntTintPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto arg0 = parameterValues[0].cast<nautilus::val<double>>();
    auto value = parameterValues[1].cast<nautilus::val<int32_t>>();
    auto ts = parameterValues[2].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](double arg0,
            int32_t value,
            uint64_t ts) -> bool {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempWkt = fmt::format("{}@{}", value, MEOS::Meos::convertEpochToTimestamp(ts));
                Temporal* temp = tint_in(tempWkt.c_str());
                if (!temp) return false;

                Temporal* res = teq_int_tint(static_cast<int>(arg0), temp);
                free(temp);
                if (!res) return false;
                bool r = tbool_start_value(res);
                free(res);
                return r;
            }
            catch (const std::exception&)
            {
                return false;
            }
        },
        arg0, value, ts);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterTeqIntTintPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "TeqIntTintPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return TeqIntTintPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
