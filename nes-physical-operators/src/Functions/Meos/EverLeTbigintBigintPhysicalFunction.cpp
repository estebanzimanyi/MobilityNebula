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

#include <Functions/Meos/EverLeTbigintBigintPhysicalFunction.hpp>

#include <Functions/PhysicalFunction.hpp>
#include <MEOSWrapper.hpp>
#include <Nautilus/DataTypes/VarVal.hpp>
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
}

namespace NES {

EverLeTbigintBigintPhysicalFunction::EverLeTbigintBigintPhysicalFunction(PhysicalFunction valueFunction,
                                                          PhysicalFunction timestampFunction,
                                                          PhysicalFunction scalarFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(valueFunction));
    parameterFunctions.push_back(std::move(timestampFunction));
    parameterFunctions.push_back(std::move(scalarFunction));
}

VarVal EverLeTbigintBigintPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto value     = parameterValues[0].cast<nautilus::val<int64_t>>();
    auto timestamp = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto scalar    = parameterValues[2].cast<nautilus::val<int64_t>>();

    const auto result = nautilus::invoke(
        +[](int64_t valueValue,
            uint64_t timestampValue,
            int64_t scalarValue) -> int {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                const std::string tsString = MEOS::Meos::convertEpochToTimestamp(timestampValue);
                std::string wkt = fmt::format("{}@{}", valueValue, tsString);
                Temporal* temp = tbigint_in(wkt.c_str());
                if (!temp) return 0;
                int r = ever_le_tbigint_bigint(temp, scalarValue);
                free(temp);
                return r;
            }
            catch (const std::exception&)
            {
                return 0;
            }
        },
        value, timestamp, scalar);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEverLeTbigintBigintPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "EverLeTbigintBigintPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return EverLeTbigintBigintPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
