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

#include <Functions/Meos/EverEqTposeTposeLogicalFunction.hpp>

#include <DataTypes/DataType.hpp>
#include <DataTypes/DataTypeProvider.hpp>
#include <DataTypes/Schema.hpp>
#include <ErrorHandling.hpp>
#include <LogicalFunctionRegistry.hpp>
#include <Serialization/DataTypeSerializationUtil.hpp>
#include <fmt/format.h>
#include <SerializableVariantDescriptor.pb.h>

namespace NES
{

EverEqTposeTposeLogicalFunction::EverEqTposeTposeLogicalFunction(LogicalFunction x,
                                          LogicalFunction y,
                                          LogicalFunction theta,
                                          LogicalFunction ts,
                                          LogicalFunction px0,
                                          LogicalFunction py0,
                                          LogicalFunction ptheta0,
                                          LogicalFunction ts0)
    : dataType(DataTypeProvider::provideDataType(DataType::Type::FLOAT64))
{
    parameters.reserve(8);
    parameters.push_back(std::move(x));
    parameters.push_back(std::move(y));
    parameters.push_back(std::move(theta));
    parameters.push_back(std::move(ts));
    parameters.push_back(std::move(px0));
    parameters.push_back(std::move(py0));
    parameters.push_back(std::move(ptheta0));
    parameters.push_back(std::move(ts0));
}

DataType EverEqTposeTposeLogicalFunction::getDataType() const
{
    return dataType;
}

LogicalFunction EverEqTposeTposeLogicalFunction::withDataType(const DataType& newDataType) const
{
    auto copy = *this;
    copy.dataType = newDataType;
    return copy;
}

std::vector<LogicalFunction> EverEqTposeTposeLogicalFunction::getChildren() const
{
    return parameters;
}

LogicalFunction EverEqTposeTposeLogicalFunction::withChildren(const std::vector<LogicalFunction>& children) const
{
    PRECONDITION(children.size() == 8, "EverEqTposeTposeLogicalFunction requires 8 children, but got {}", children.size());
    auto copy = *this;
    copy.parameters = children;
    return copy;
}

std::string_view EverEqTposeTposeLogicalFunction::getType() const
{
    return NAME;
}

bool EverEqTposeTposeLogicalFunction::operator==(const LogicalFunctionConcept& rhs) const
{
    if (const auto* other = dynamic_cast<const EverEqTposeTposeLogicalFunction*>(&rhs))
    {
        return parameters == other->parameters;
    }
    return false;
}

std::string EverEqTposeTposeLogicalFunction::explain(ExplainVerbosity verbosity) const
{
    std::string args;
    for (size_t index = 0; index < parameters.size(); ++index)
    {
        if (index > 0)
        {
            args += ", ";
        }
        args += parameters[index].explain(verbosity);
    }
    return fmt::format("{}({})", NAME, args);
}

LogicalFunction EverEqTposeTposeLogicalFunction::withInferredDataType(const Schema& schema) const
{
    std::vector<LogicalFunction> newChildren;
    newChildren.reserve(parameters.size());
    for (const auto& child : parameters)
    {
        newChildren.emplace_back(child.withInferredDataType(schema));
    }
    return withChildren(newChildren);
}

SerializableFunction EverEqTposeTposeLogicalFunction::serialize() const
{
    SerializableFunction proto;
    proto.set_function_type(std::string(NAME));
    DataTypeSerializationUtil::serializeDataType(dataType, proto.mutable_data_type());
    for (const auto& child : parameters)
    {
        proto.add_children()->CopyFrom(child.serialize());
    }
    return proto;
}

LogicalFunctionRegistryReturnType LogicalFunctionGeneratedRegistrar::RegisterEverEqTposeTposeLogicalFunction(
    LogicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.children.size() == 8,
                 "EverEqTposeTposeLogicalFunction requires 8 children but got {}",
                 arguments.children.size());
    auto arg0 = std::move(arguments.children[0]);
    auto arg1 = std::move(arguments.children[1]);
    auto arg2 = std::move(arguments.children[2]);
    auto arg3 = std::move(arguments.children[3]);
    auto arg4 = std::move(arguments.children[4]);
    auto arg5 = std::move(arguments.children[5]);
    auto arg6 = std::move(arguments.children[6]);
    auto arg7 = std::move(arguments.children[7]);
    return EverEqTposeTposeLogicalFunction(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4), std::move(arg5), std::move(arg6), std::move(arg7));
}

} // namespace NES
