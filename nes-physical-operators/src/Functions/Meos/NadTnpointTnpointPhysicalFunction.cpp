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

#include <Functions/Meos/NadTnpointTnpointPhysicalFunction.hpp>

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
#include <meos_npoint.h>
}

namespace NES {

NadTnpointTnpointPhysicalFunction::NadTnpointTnpointPhysicalFunction(PhysicalFunction ridFunction,
                                                          PhysicalFunction fracFunction,
                                                          PhysicalFunction tsFunction,
                                                          PhysicalFunction rid0Function,
                                                          PhysicalFunction frac0Function,
                                                          PhysicalFunction ts0Function)
{
    parameterFunctions.reserve(6);
    parameterFunctions.push_back(std::move(ridFunction));
    parameterFunctions.push_back(std::move(fracFunction));
    parameterFunctions.push_back(std::move(tsFunction));
    parameterFunctions.push_back(std::move(rid0Function));
    parameterFunctions.push_back(std::move(frac0Function));
    parameterFunctions.push_back(std::move(ts0Function));
}

VarVal NadTnpointTnpointPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto rid = parameterValues[0].cast<nautilus::val<int64_t>>();
    auto frac = parameterValues[1].cast<nautilus::val<double>>();
    auto ts = parameterValues[2].cast<nautilus::val<uint64_t>>();
    auto rid0 = parameterValues[3].cast<nautilus::val<uint64_t>>();
    auto frac0 = parameterValues[4].cast<nautilus::val<double>>();
    auto ts0 = parameterValues[5].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](int64_t rid,
            double frac,
            uint64_t ts,
            uint64_t rid0,
            double frac0,
            uint64_t ts0) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                if (frac < 0.0 || frac > 1.0) return 0.0;
                std::string tempWkt = fmt::format("NPoint({},{})@{}", rid, frac, MEOS::Meos::convertEpochToTimestamp(ts));
                Temporal* temp = tnpoint_in(tempWkt.c_str());
                if (!temp) return 0.0;
                Npoint* np0 = npoint_make((int64_t)rid0, frac0);
                if (!np0) { free(temp); return 0.0; }
                Temporal* inst0 = (Temporal*)tnpointinst_make(np0, (TimestampTz)ts0);
                free(np0);
                if (!inst0) { free(temp); return 0.0; }

                double r = nad_tnpoint_tnpoint(temp, inst0);
                free(temp);
                free(inst0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        rid, frac, ts, rid0, frac0, ts0);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterNadTnpointTnpointPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 6,
                 "NadTnpointTnpointPhysicalFunction requires 6 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    auto arg3 = std::move(arguments.childFunctions[3]);
    auto arg4 = std::move(arguments.childFunctions[4]);
    auto arg5 = std::move(arguments.childFunctions[5]);
    return NadTnpointTnpointPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4), std::move(arg5));
}

} // namespace NES
