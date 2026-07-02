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

#include <Functions/Meos/FloatspanMakePhysicalFunction.hpp>

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

FloatspanMakePhysicalFunction::FloatspanMakePhysicalFunction(PhysicalFunction lowerFunction,
                                                          PhysicalFunction upperFunction,
                                                          PhysicalFunction lower_incFunction,
                                                          PhysicalFunction upper_incFunction)
{
    parameterFunctions.reserve(4);
    parameterFunctions.push_back(std::move(lowerFunction));
    parameterFunctions.push_back(std::move(upperFunction));
    parameterFunctions.push_back(std::move(lower_incFunction));
    parameterFunctions.push_back(std::move(upper_incFunction));
}

VarVal FloatspanMakePhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto lower = parameterValues[0].cast<nautilus::val<double>>();
    auto upper = parameterValues[1].cast<nautilus::val<double>>();
    auto lower_inc = parameterValues[2].cast<nautilus::val<double>>();
    auto upper_inc = parameterValues[3].cast<nautilus::val<double>>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](double lower,
            double upper,
            double lower_inc,
            double upper_inc,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                char* out = ({Span* _sp=floatspan_make(lower,upper,lower_inc!=0.0,upper_inc!=0.0);if(!_sp)return 0u;char* _out=floatspan_out(_sp,-1);free(_sp);_out;});
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
        lower, upper, lower_inc, upper_inc, outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterFloatspanMakePhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 4,
                 "FloatspanMakePhysicalFunction requires 4 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    auto arg3 = std::move(arguments.childFunctions[3]);
    return FloatspanMakePhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3));
}

} // namespace NES
