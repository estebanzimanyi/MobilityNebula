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

#include <Functions/Meos/GeoIsUnitaryPhysicalFunction.hpp>

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

GeoIsUnitaryPhysicalFunction::GeoIsUnitaryPhysicalFunction(PhysicalFunction wktFunction)
{
    parameterFunctions.reserve(1);
    parameterFunctions.push_back(std::move(wktFunction));
}

VarVal GeoIsUnitaryPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto wkt = parameterValues[0].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* wktPtr, uint32_t wktSize) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempS(wktPtr, wktSize);
                GSERIALIZED* temp = geom_in(tempS.c_str(), -1);
                if (!temp) return 0.0;

                double r = geo_is_unitary(temp);
                free(temp);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        wkt.getContent(), wkt.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterGeoIsUnitaryPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 1,
                 "GeoIsUnitaryPhysicalFunction requires 1 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    return GeoIsUnitaryPhysicalFunction(std::move(arg0));
}

} // namespace NES
