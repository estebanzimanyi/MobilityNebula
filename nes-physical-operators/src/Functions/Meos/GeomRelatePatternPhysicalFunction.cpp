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

#include <Functions/Meos/GeomRelatePatternPhysicalFunction.hpp>

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

GeomRelatePatternPhysicalFunction::GeomRelatePatternPhysicalFunction(PhysicalFunction wkt1Function,
                                                          PhysicalFunction wkt2Function,
                                                          PhysicalFunction patternFunction)
{
    parameterFunctions.reserve(3);
    parameterFunctions.push_back(std::move(wkt1Function));
    parameterFunctions.push_back(std::move(wkt2Function));
    parameterFunctions.push_back(std::move(patternFunction));
}

VarVal GeomRelatePatternPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto wkt1 = parameterValues[0].cast<VariableSizedData>();
    auto wkt2 = parameterValues[1].cast<VariableSizedData>();
    auto pattern = parameterValues[2].cast<VariableSizedData>();

    const auto result = nautilus::invoke(
        +[](const char* wkt1Ptr, uint32_t wkt1Size,
            const char* wkt2Ptr, uint32_t wkt2Size,
            const char* patternPtr, uint32_t patternSize) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();

                double r = ({std::string _s1(wkt1Ptr,wkt1Size),_s2(wkt2Ptr,wkt2Size),_sp(patternPtr,patternSize);GSERIALIZED* _gs1=geom_in(_s1.c_str(),-1);if(!_gs1)return 0.0;GSERIALIZED* _gs2=geom_in(_s2.c_str(),-1);if(!_gs2){free(_gs1);return 0.0;}bool _r=geom_relate_pattern(_gs1,_gs2,const_cast<char*>(_sp.c_str()));free(_gs1);free(_gs2);_r?1.0:0.0;});
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        wkt1.getContent(), wkt1.getContentSize(), wkt2.getContent(), wkt2.getContentSize(), pattern.getContent(), pattern.getContentSize());

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterGeomRelatePatternPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 3,
                 "GeomRelatePatternPhysicalFunction requires 3 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    return GeomRelatePatternPhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
