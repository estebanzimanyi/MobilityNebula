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

#include <Functions/Meos/JsonbExistsPhysicalFunction.hpp>

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

JsonbExistsPhysicalFunction::JsonbExistsPhysicalFunction(PhysicalFunction jbFunction,
                                                          PhysicalFunction arg0Function)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(jbFunction));
    parameterFunctions.push_back(std::move(arg0Function));
}

VarVal JsonbExistsPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto jb = parameterValues[0].cast<VariableSizedData>();
    auto arg0 = parameterValues[1].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* jbPtr, uint32_t jbSize,
            const char* arg0Ptr, uint32_t arg0Size) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(jbPtr, jbSize);
                Jsonb* temp = jsonb_in(tempS.c_str());
                if (!temp) return 0.0;
                std::string arg0S(arg0Ptr, arg0Size);
                while (!arg0S.empty() && (arg0S.front() == '\'' || arg0S.front() == '"')) arg0S.erase(arg0S.begin());
                while (!arg0S.empty() && (arg0S.back()  == '\'' || arg0S.back()  == '"')) arg0S.pop_back();
                text* txt0 = cstring_to_text(arg0S.c_str());
                if (!txt0) { free(temp); return 0.0; }

                double r = jsonb_exists(temp, txt0);
                free(temp);
                free(txt0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        jb.getContent(), jb.getContentSize(), arg0.getContent(), arg0.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterJsonbExistsPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "JsonbExistsPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return JsonbExistsPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
