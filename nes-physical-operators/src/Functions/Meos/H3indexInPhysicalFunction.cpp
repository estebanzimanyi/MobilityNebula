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

#include <Functions/Meos/H3indexInPhysicalFunction.hpp>

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
#include <meos_h3.h>
}

namespace NES {

H3indexInPhysicalFunction::H3indexInPhysicalFunction(PhysicalFunction hexFunction)
{
    parameterFunctions.reserve(1);
    parameterFunctions.push_back(std::move(hexFunction));
}

VarVal H3indexInPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto hex = parameterValues[0].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* hexPtr, uint32_t hexSize) -> uint64_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                uint64_t r = (uint64_t)h3index_in(std::string(hexPtr,hexSize).c_str());
                return r;
            }
            catch (const std::exception&)
            {
                return 0ULL;
            }
        },
        hex.getContent(), hex.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterH3indexInPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 1,
                 "H3indexInPhysicalFunction requires 1 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    return H3indexInPhysicalFunction(std::move(arg0));
}

} // namespace NES
