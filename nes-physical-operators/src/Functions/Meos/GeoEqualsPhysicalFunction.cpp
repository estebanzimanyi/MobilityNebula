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

#include <Functions/Meos/GeoEqualsPhysicalFunction.hpp>

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
#include <meos_geo.h>
}

namespace NES {

GeoEqualsPhysicalFunction::GeoEqualsPhysicalFunction(PhysicalFunction wktFunction,
                                                          PhysicalFunction arg0Function)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(wktFunction));
    parameterFunctions.push_back(std::move(arg0Function));
}

VarVal GeoEqualsPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto wkt = parameterValues[0].cast<VariableSizedData>();
    auto arg0 = parameterValues[1].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* wktPtr, uint32_t wktSize,
            const char* arg0Ptr, uint32_t arg0Size) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(wktPtr, wktSize);
                GSERIALIZED* temp = geom_in(tempS.c_str(), -1);
                if (!temp) return 0.0;
                std::string arg0S(arg0Ptr, arg0Size);
                GSERIALIZED* gs0 = geom_in(arg0S.c_str(), -1);
                if (!gs0) { free(temp); return 0.0; }

                double r = geom_equals(temp, gs0);
                free(temp);
                free(gs0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        wkt.getContent(), wkt.getContentSize(), arg0.getContent(), arg0.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterGeoEqualsPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "GeoEqualsPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return GeoEqualsPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
