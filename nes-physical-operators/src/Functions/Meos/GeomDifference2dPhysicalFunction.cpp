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

#include <Functions/Meos/GeomDifference2dPhysicalFunction.hpp>

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
#include <meos_geo.h>
}

namespace NES {

GeomDifference2dPhysicalFunction::GeomDifference2dPhysicalFunction(PhysicalFunction wktFunction,
                                                          PhysicalFunction arg0Function)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(wktFunction));
    parameterFunctions.push_back(std::move(arg0Function));
}

VarVal GeomDifference2dPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto wkt = parameterValues[0].cast<VariableSizedData>();
    auto arg0 = parameterValues[1].cast<VariableSizedData>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](const char* wktPtr, uint32_t wktSize,
            const char* arg0Ptr, uint32_t arg0Size,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(wktPtr, wktSize);
                GSERIALIZED* temp = geom_in(tempS.c_str(), -1);
                if (!temp) return 0u;
                std::string arg0S(arg0Ptr, arg0Size);
                GSERIALIZED* gs0 = geom_in(arg0S.c_str(), -1);
                if (!gs0) { free(temp); return 0u; }

                GSERIALIZED* gres = geom_difference2d(temp, gs0);
                free(temp);
                free(gs0);
                if (!gres) return 0u;
                char* out = geo_as_text(gres, -1);
                free(gres);
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
        wkt.getContent(), wkt.getContentSize(), arg0.getContent(), arg0.getContentSize(), outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterGeomDifference2dPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "GeomDifference2dPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return GeomDifference2dPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
