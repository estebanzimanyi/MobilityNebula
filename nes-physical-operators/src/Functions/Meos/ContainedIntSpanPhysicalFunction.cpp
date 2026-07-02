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

#include <Functions/Meos/ContainedIntSpanPhysicalFunction.hpp>

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
}

namespace NES {

ContainedIntSpanPhysicalFunction::ContainedIntSpanPhysicalFunction(PhysicalFunction arg0Function,
                                                          PhysicalFunction spFunction)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(arg0Function));
    parameterFunctions.push_back(std::move(spFunction));
}

VarVal ContainedIntSpanPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto arg0 = parameterValues[0].cast<nautilus::val<double>>();
    auto sp = parameterValues[1].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](double arg0,
            const char* spPtr, uint32_t spSize) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(spPtr, spSize);
                Span* temp = intspan_in(tempS.c_str());
                if (!temp) return 0.0;

                double r = contained_int_span((int)arg0, temp);
                free(temp);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        arg0, sp.getContent(), sp.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterContainedIntSpanPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "ContainedIntSpanPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return ContainedIntSpanPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
