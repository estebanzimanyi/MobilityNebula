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

#include <Functions/Meos/JsonObjectFieldTextPhysicalFunction.hpp>

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
#include <meos_json.h>
}

namespace NES {

JsonObjectFieldTextPhysicalFunction::JsonObjectFieldTextPhysicalFunction(PhysicalFunction jsonFunction,
                                                          PhysicalFunction keyFunction)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(jsonFunction));
    parameterFunctions.push_back(std::move(keyFunction));
}

VarVal JsonObjectFieldTextPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto json = parameterValues[0].cast<VariableSizedData>();
    auto key = parameterValues[1].cast<VariableSizedData>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](const char* jsonPtr, uint32_t jsonSize,
            const char* keyPtr, uint32_t keySize,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                text* tres = ({std::string _sj(jsonPtr,jsonSize),_sk(keyPtr,keySize);text* _js=json_in(_sj.c_str());if(!_js)return 0u;text* _key=cstring_to_text(_sk.c_str());text* _res=json_object_field_text(_js,_key);free(_js);free(_key);_res;});
                if (!tres) return 0u;
                char* out = json_out(tres);
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
        json.getContent(), json.getContentSize(), key.getContent(), key.getContentSize(), outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterJsonObjectFieldTextPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "JsonObjectFieldTextPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return JsonObjectFieldTextPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
