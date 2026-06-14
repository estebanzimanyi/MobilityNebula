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

#include <Functions/Meos/EcoversTgeoGeoPhysicalFunction.hpp>
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

EcoversTgeoGeoPhysicalFunction::EcoversTgeoGeoPhysicalFunction(PhysicalFunction lonFn, PhysicalFunction latFn,
                                              PhysicalFunction tsFn, PhysicalFunction wktFn)
{
    paramFns.reserve(4);
    paramFns.push_back(std::move(lonFn));
    paramFns.push_back(std::move(latFn));
    paramFns.push_back(std::move(tsFn));
    paramFns.push_back(std::move(wktFn));
}

VarVal EcoversTgeoGeoPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    auto lon = paramFns[0].execute(record, arena).cast<nautilus::val<double>>();
    auto lat = paramFns[1].execute(record, arena).cast<nautilus::val<double>>();
    auto ts  = paramFns[2].execute(record, arena).cast<nautilus::val<uint64_t>>();
    auto wkt = paramFns[3].execute(record, arena).cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](double lon_v, double lat_v, uint64_t ts_v,
            const char* gw, uint32_t gwsz) -> double {
            try {
                MEOS::Meos::ensureMeosInitialized();
                std::string ts_str = MEOS::Meos::convertEpochToTimestamp(ts_v);
                std::string tgeo_wkt = fmt::format("SRID=4326;Point({} {})@{}", lon_v, lat_v, ts_str);
                Temporal* temp = tgeompoint_in(tgeo_wkt.c_str());
                if (!temp) return 0.0;
                std::string geom_str(gw, gwsz);
                GSERIALIZED* gs = geom_in(geom_str.c_str(), -1);
                if (!gs) { free(temp); return 0.0; }
                int r = ecovers_tgeo_geo(temp, gs);
                free(temp); free(gs);
                return r > 0 ? 1.0 : 0.0;
            } catch (const std::exception&) { return 0.0; }
        },
        lon, lat, ts, wkt);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEcoversTgeoGeoPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 4,
                 "EcoversTgeoGeoPhysicalFunction requires 4 children but got {}",
                 arguments.childFunctions.size());
    return EcoversTgeoGeoPhysicalFunction(
        std::move(arguments.childFunctions[0]),
        std::move(arguments.childFunctions[1]),
        std::move(arguments.childFunctions[2]),
        std::move(arguments.childFunctions[3]));
}

} // namespace NES
