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

#include <Functions/Meos/AlwaysNePoseTposePhysicalFunction.hpp>

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
#include <meos_pose.h>
}

namespace NES {

AlwaysNePoseTposePhysicalFunction::AlwaysNePoseTposePhysicalFunction(PhysicalFunction px0Function,
                                                          PhysicalFunction py0Function,
                                                          PhysicalFunction ptheta0Function,
                                                          PhysicalFunction xFunction,
                                                          PhysicalFunction yFunction,
                                                          PhysicalFunction thetaFunction,
                                                          PhysicalFunction tsFunction)
{
    parameterFunctions.reserve(7);
    parameterFunctions.push_back(std::move(px0Function));
    parameterFunctions.push_back(std::move(py0Function));
    parameterFunctions.push_back(std::move(ptheta0Function));
    parameterFunctions.push_back(std::move(xFunction));
    parameterFunctions.push_back(std::move(yFunction));
    parameterFunctions.push_back(std::move(thetaFunction));
    parameterFunctions.push_back(std::move(tsFunction));
}

VarVal AlwaysNePoseTposePhysicalFunction::execute(const Record& record, ArenaRef& arena) const
{
    std::vector<VarVal> parameterValues;
    parameterValues.reserve(parameterFunctions.size());
    for (const auto& function : parameterFunctions)
    {
        parameterValues.emplace_back(function.execute(record, arena));
    }

    auto px0 = parameterValues[0].cast<nautilus::val<double>>();
    auto py0 = parameterValues[1].cast<nautilus::val<double>>();
    auto ptheta0 = parameterValues[2].cast<nautilus::val<double>>();
    auto x = parameterValues[3].cast<nautilus::val<double>>();
    auto y = parameterValues[4].cast<nautilus::val<double>>();
    auto theta = parameterValues[5].cast<nautilus::val<double>>();
    auto ts = parameterValues[6].cast<nautilus::val<uint64_t>>();

    const auto result = nautilus::invoke(
        +[](double px0,
            double py0,
            double ptheta0,
            double x,
            double y,
            double theta,
            uint64_t ts) -> double {
            try
            {
                MEOS::Meos::ensureMeosInitialized();
                std::string tempWkt = fmt::format("Pose(Point({} {}),{})@{}", x, y, theta, MEOS::Meos::convertEpochToTimestamp(ts));
                Temporal* temp = tpose_in(tempWkt.c_str());
                if (!temp) return 0.0;
                Pose* pose0 = pose_make_2d(px0, py0, ptheta0, false, 0);
                if (!pose0) { free(temp); return 0.0; }

                double r = always_ne_pose_tpose(pose0, temp);
                free(temp);
                free(pose0);
                return r;
            }
            catch (const std::exception&)
            {
                return 0.0;
            }
        },
        px0, py0, ptheta0, x, y, theta, ts);

    return VarVal(result);
}

PhysicalFunctionRegistryReturnType PhysicalFunctionGeneratedRegistrar::RegisterAlwaysNePoseTposePhysicalFunction(
    PhysicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.childFunctions.size() == 7,
                 "AlwaysNePoseTposePhysicalFunction requires 7 children but got {}",
                 arguments.childFunctions.size());
    auto arg0 = std::move(arguments.childFunctions[0]);
    auto arg1 = std::move(arguments.childFunctions[1]);
    auto arg2 = std::move(arguments.childFunctions[2]);
    auto arg3 = std::move(arguments.childFunctions[3]);
    auto arg4 = std::move(arguments.childFunctions[4]);
    auto arg5 = std::move(arguments.childFunctions[5]);
    auto arg6 = std::move(arguments.childFunctions[6]);
    return AlwaysNePoseTposePhysicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4), std::move(arg5), std::move(arg6));
}

} // namespace NES
