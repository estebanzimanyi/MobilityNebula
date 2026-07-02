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

#include <Functions/Meos/EintersectsTpcpointGeoPhysicalFunction.hpp>

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
#include <meos_pointcloud.h>
}

namespace NES {

EintersectsTpcpointGeoPhysicalFunction::EintersectsTpcpointGeoPhysicalFunction(PhysicalFunction pt_hexwkbFunction,
                                                          PhysicalFunction tsFunction,
                                                          PhysicalFunction tgt_wktFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(pt_hexwkbFunction));
    parameterFunctions.push_back(std::move(tsFunction));
    parameterFunctions.push_back(std::move(tgt_wktFunction));
}

VarVal EintersectsTpcpointGeoPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto pt_hexwkb = parameterValues[0].cast<VariableSizedData>();
    auto ts = parameterValues[1].cast<nautilus::val<uint64_t>>();
    auto tgt_wkt = parameterValues[2].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* pt_hexwkbPtr, uint32_t pt_hexwkbSize,
            uint64_t ts,
            const char* tgt_wktPtr, uint32_t tgt_wktSize) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                double r = ({char* _hs=(char*)malloc(pt_hexwkbSize+1);if(!_hs)return 0.0;memcpy(_hs,pt_hexwkbPtr,pt_hexwkbSize);_hs[pt_hexwkbSize]='\0';Pcpoint* _pt=pcpoint_from_hexwkb(_hs);free(_hs);if(!_pt)return 0.0;TInstant* _inst=tpointcloudinst_make(_pt,(TimestampTz)ts);free(_pt);if(!_inst)return 0.0;char* _gs_str=(char*)malloc(tgt_wktSize+1);if(!_gs_str){free(_inst);return 0.0;}memcpy(_gs_str,tgt_wktPtr,tgt_wktSize);_gs_str[tgt_wktSize]='\0';GSERIALIZED* _gs=geom_in(_gs_str,-1);free(_gs_str);if(!_gs){free(_inst);return 0.0;}bool _r=eintersects_tpcpoint_geo((Temporal*)_inst,_gs);free(_inst);free(_gs);_r?1.0:0.0;});
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        pt_hexwkb.getContent(), pt_hexwkb.getContentSize(), ts, tgt_wkt.getContent(), tgt_wkt.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterEintersectsTpcpointGeoPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "EintersectsTpcpointGeoPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return EintersectsTpcpointGeoPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
