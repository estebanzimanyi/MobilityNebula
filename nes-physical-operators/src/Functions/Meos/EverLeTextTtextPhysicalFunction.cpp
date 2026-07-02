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

#include <Functions/Meos/EverLeTextTtextPhysicalFunction.hpp>

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

EverLeTextTtextPhysicalFunction::EverLeTextTtextPhysicalFunction(PhysicalFunction arg0Function,
                                                          PhysicalFunction valueFunction,
                                                          PhysicalFunction tsFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(arg0Function));
    parameterFunctions.push_back(std::move(valueFunction));
    parameterFunctions.push_back(std::move(tsFunction));
}

VarVal EverLeTextTtextPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto arg0 = parameterValues[0].cast<VariableSizedData>();
    auto value = parameterValues[1].cast<VariableSizedData>();
    auto ts = parameterValues[2].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](const char* arg0Ptr, uint32_t arg0Size,
            const char* valuePtr, uint32_t valueSize,
            uint64_t ts) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempVal(valuePtr, valueSize);
                while (!tempVal.empty() && (tempVal.front() == '\'' || tempVal.front() == '"')) tempVal.erase(tempVal.begin());
                while (!tempVal.empty() && (tempVal.back()  == '\'' || tempVal.back()  == '"')) tempVal.pop_back();
                std::string tempWkt = "'" + tempVal + "'@" + MEOS::Meos::convertEpochToTimestamp(ts);
                Temporal* temp = ttext_in(tempWkt.c_str());
                if (!temp) return 0.0;
                std::string arg0S(arg0Ptr, arg0Size);
                while (!arg0S.empty() && (arg0S.front() == '\'' || arg0S.front() == '"')) arg0S.erase(arg0S.begin());
                while (!arg0S.empty() && (arg0S.back()  == '\'' || arg0S.back()  == '"')) arg0S.pop_back();
                text* txt0 = cstring_to_text(arg0S.c_str());
                if (!txt0) { free(temp); return 0.0; }

                double r = ever_le_text_ttext(txt0, temp);
                free(temp);
                free(txt0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        arg0.getContent(), arg0.getContentSize(), value.getContent(), value.getContentSize(), ts);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEverLeTextTtextPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "EverLeTextTtextPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return EverLeTextTtextPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
