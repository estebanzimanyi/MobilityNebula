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

#include <Functions/Meos/QuadbinPointToCellPhysicalFunction.hpp>

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
#include <meos_quadbin.h>
}

namespace NES {

QuadbinPointToCellPhysicalFunction::QuadbinPointToCellPhysicalFunction(PhysicalFunction lonFunction,
                                                          PhysicalFunction latFunction,
                                                          PhysicalFunction resFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(lonFunction));
    parameterFunctions.push_back(std::move(latFunction));
    parameterFunctions.push_back(std::move(resFunction));
}

VarVal QuadbinPointToCellPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto lon = parameterValues[0].cast<nautilus::val<double>>();
    auto lat = parameterValues[1].cast<nautilus::val<double>>();
    auto res = parameterValues[2].cast<nautilus::val<uint64_t>>();

    constexpr uint32_t MAX_LEN = 4096;
    auto outBuf = arena.allocateVariableSizedData(nautilus::val<uint32_t>(MAX_LEN));

    const auto actualLen = nautilus::invoke(
        +[](double lon,
            double lat,
            uint64_t res,
            char* buf,
            uint32_t bufMax) -> uint32_t {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                Quadbin qcell = quadbin_point_to_cell(lon, lat, (uint32_t)res);
                char* qstr = quadbin_index_to_string(qcell);
                if (!qstr) return 0u;
                uint32_t len = static_cast<uint32_t>(strlen(qstr));
                if (len > bufMax) len = bufMax;
                memcpy(buf, qstr, len);
                free(qstr);
                return len;
            }
            catch (const std::exception&)
            {
                return 0u;
            }
        },
        lon, lat, res, outBuf.getContent(), nautilus::val<uint32_t>(MAX_LEN));

    VarVal(actualLen).writeToMemory(outBuf.getReference());
    return outBuf;
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterQuadbinPointToCellPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "QuadbinPointToCellPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return QuadbinPointToCellPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
