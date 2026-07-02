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

#include <Functions/Meos/AcoversGeoTgeoPhysicalFunction.hpp>

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

AcoversGeoTgeoPhysicalFunction::AcoversGeoTgeoPhysicalFunction(PhysicalFunction wktFunction,
                                                          PhysicalFunction arg0Function)
{
    parameterFunctions.reserve(2);
    parameterFunctions.push_back(std::move(wktFunction));
    parameterFunctions.push_back(std::move(arg0Function));
}

VarVal AcoversGeoTgeoPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
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
            const char* arg0Ptr, uint32_t arg0Size) -> int {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(wktPtr, wktSize);
                GSERIALIZED* temp = geom_in(tempS.c_str(), -1);
                if (!temp) return 0;
                std::string arg0Hex(arg0Ptr, arg0Size);
                Temporal* arg0T = temporal_from_hexwkb(arg0Hex.c_str());
                if (!arg0T) { free(temp); return 0; }

                int r = acovers_geo_tgeo(temp, arg0T);
                free(temp);
                free(arg0T);
                return r;
            }
            catch (const std::exception&)
            {
                return 0;
            }
        },
        wkt.getContent(), wkt.getContentSize(), arg0.getContent(), arg0.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterAcoversGeoTgeoPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 2,
                 "AcoversGeoTgeoPhysicalFunction requires 2 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    return AcoversGeoTgeoPhysicalFunction(std::move(arg0), std::move(arg1));
}

} // namespace NES
