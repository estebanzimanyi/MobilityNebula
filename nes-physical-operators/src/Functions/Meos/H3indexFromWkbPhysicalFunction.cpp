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

#include <Functions/Meos/H3indexFromWkbPhysicalFunction.hpp>
#include <Functions/PhysicalFunction.hpp>
#include <MEOSWrapper.hpp>
#include <Nautilus/DataTypes/VarVal.hpp>
#include <Nautilus/DataTypes/VariableSizedData.hpp>
#include <Nautilus/Interface/Record.hpp>
#include <PhysicalFunctionRegistry.hpp>
#include <ErrorHandling.hpp>
#include <ExecutionContext.hpp>
#include <function.hpp>
#include <utility>
#include <val.hpp>

extern "C" {
#include <meos.h>
#include <meos_h3.h>
}
#include <string>

namespace NES {

H3indexFromWkbPhysicalFunction::H3indexFromWkbPhysicalFunction(PhysicalFunction wkb)
{
    paramFns.reserve(1);
    paramFns.push_back(std::move(wkb));
}

VarVal H3indexFromWkbPhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    auto wkb = paramFns[0].execute(record, arena).cast<VariableSizedData>();
    const auto result = nautilus::invoke(
        +[](const char* w, uint32_t wsz) -> uint64_t {
            try {
                MEOS::Meos::ensureMeosInitialized();
                H3Index cell = h3index_from_wkb((const uint8_t*)w, (size_t)wsz);
                return (uint64_t)cell;
            } catch (const std::exception&) { return 0ULL; }
        },
        wkb);
    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterH3indexFromWkbPhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 1,
                 "H3indexFromWkbPhysicalFunction requires 1 child but got {}",
                 arguments.childFunctions.size());
    return H3indexFromWkbPhysicalFunction(std::move(arguments.childFunctions[0]));
}

} // namespace NES
