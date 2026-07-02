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

#include <Functions/Meos/TextLowerPhysicalFunction.hpp>

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
#include <string.h>
#include <utility>
#include <val.hpp>

extern "C" {
#include <meos.h>
}

namespace NES {

TextLowerPhysicalFunction::TextLowerPhysicalFunction(PhysicalFunction strFunction)
{
    parameterFunctions.reserve(1);
    parameterFunctions.push_back(std::move(strFunction));
}

VarVal TextLowerPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto str = parameterValues[0].cast<VariableSizedData>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](const char* strPtr, uint32_t strSize,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempRawStr(strPtr, strSize);
                while (!tempRawStr.empty() && (tempRawStr.front() == '\'' || tempRawStr.front() == '"')) tempRawStr.erase(tempRawStr.begin());
                while (!tempRawStr.empty() && (tempRawStr.back()  == '\'' || tempRawStr.back()  == '"')) tempRawStr.pop_back();
                text* temp = cstring_to_text(tempRawStr.c_str());
                if (!temp) return 0u;

                text* tres = text_lower(temp);
                free(temp);
                if (!tres) return 0u;
                char* out = text_to_cstring(tres);
                free(tres);
                if (!out) return 0u;
                uint32_t len = static_cast<uint32_t>(strlen(out));
                if (len > bufMax) len = bufMax;
                memcpy(buf, out, len);
                free(out);
                return len;
            }
            catch (const std::exception&)
            {
                return 0u;
            }
        },
        str.getContent(), str.getContentSize(), outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterTextLowerPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 1,
                 "TextLowerPhysicalFunction requires 1 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    return TextLowerPhysicalFunction(std::move(arg0));
}

} // namespace NES
