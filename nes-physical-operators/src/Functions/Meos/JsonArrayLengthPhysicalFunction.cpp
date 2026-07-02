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

#include <Functions/Meos/JsonArrayLengthPhysicalFunction.hpp>

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

JsonArrayLengthPhysicalFunction::JsonArrayLengthPhysicalFunction(PhysicalFunction jsonFunction)
{
    parameterFunctions.reserve(1);
    parameterFunctions.push_back(std::move(jsonFunction));
}

VarVal JsonArrayLengthPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto json = parameterValues[0].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* jsonPtr, uint32_t jsonSize) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                double r = ({std::string _s(jsonPtr,jsonSize);text* _js=json_in(_s.c_str());if(!_js)return 0.0;int _n=json_array_length(_js);free(_js);(double)_n;});
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        json.getContent(), json.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterJsonArrayLengthPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 1,
                 "JsonArrayLengthPhysicalFunction requires 1 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    return JsonArrayLengthPhysicalFunction(std::move(arg0));
}

} // namespace NES
