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

#include <Functions/Meos/EverEqTjsonbTjsonbPhysicalFunction.hpp>

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
#include <meos_json.h>
}

namespace NES {

EverEqTjsonbTjsonbPhysicalFunction::EverEqTjsonbTjsonbPhysicalFunction(PhysicalFunction json_strFunction,
                                                          PhysicalFunction tsFunction,
                                                          PhysicalFunction json0Function,
                                                          PhysicalFunction ts0Function)
{
    parameterFunctions.reserve(4);
    parameterFunctions.push_back(std::move(json_strFunction));
    parameterFunctions.push_back(std::move(tsFunction));
    parameterFunctions.push_back(std::move(json0Function));
    parameterFunctions.push_back(std::move(ts0Function));
}

VarVal EverEqTjsonbTjsonbPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto json_str = parameterValues[0].cast<VariableSizedData>();
    auto ts = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto json0 = parameterValues[2].cast<VariableSizedData>();
    auto ts0 = parameterValues[3].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](const char* json_strPtr, uint32_t json_strSize,
            uint64_t ts,
            const char* json0Ptr, uint32_t json0Size,
            uint64_t ts0) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(json_strPtr, json_strSize);
                Jsonb* tempJb = jsonb_in(tempS.c_str());
                if (!tempJb) return 0.0;
                Temporal* temp = (Temporal*)tjsonbinst_make(tempJb, (TimestampTz)ts);
                free(tempJb);
                if (!temp) return 0.0;
                std::string json0S(json0Ptr, json0Size);
                Jsonb* jb0 = jsonb_in(json0S.c_str());
                if (!jb0) { free(temp); return 0.0; }
                Temporal* inst0 = (Temporal*)tjsonbinst_make(jb0, (TimestampTz)ts0);
                free(jb0);
                if (!inst0) { free(temp); return 0.0; }

                double r = ever_eq_tjsonb_tjsonb(temp, inst0);
                free(temp);
                free(inst0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        json_str.getContent(), json_str.getContentSize(), ts, json0.getContent(), json0.getContentSize(), ts0);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEverEqTjsonbTjsonbPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 4,
                 "EverEqTjsonbTjsonbPhysicalFunction requires 4 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    auto arg3 = std::move(arguments.childFunctions[3]);
    return EverEqTjsonbTjsonbPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3));
}

} // namespace NES
