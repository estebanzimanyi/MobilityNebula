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

#include <Functions/Meos/EverNeQuadbinTquadbinLogicalFunction.hpp>

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

EverNeQuadbinTquadbinLogicalFunction::EverNeQuadbinTquadbinLogicalFunction(LogicalFunction arg0,
                                          LogicalFunction cell1,
                                          LogicalFunction ts1)
    : dataType(DataTypeProvider::provideDataType(DataType::Type::FLOAT64))
{
    parameters.reserve(3);
    parameters.push_back(std::move(arg0));
    parameters.push_back(std::move(cell1));
    parameters.push_back(std::move(ts1));
}

DataType EverNeQuadbinTquadbinLogicalFunction::getDataType() const
{
    return dataType;
}

LogicalFunction EverNeQuadbinTquadbinLogicalFunction::withDataType(const DataType& newDataType) const
{
    auto copy = *this;
    copy.dataType = newDataType;
    return copy;
}

std::vector<LogicalFunction> EverNeQuadbinTquadbinLogicalFunction::getChildren() const
{
    return parameters;
}

LogicalFunction EverNeQuadbinTquadbinLogicalFunction::withChildren(const std::vector<LogicalFunction>& children) const
{
    PRECONDITION(children.size() == 3, "EverNeQuadbinTquadbinLogicalFunction requires 3 children, but got {}", children.size());
    auto copy = *this;
    copy.parameters = children;
    return copy;
}

std::string_view EverNeQuadbinTquadbinLogicalFunction::getType() const
{
    return NAME;
}

bool EverNeQuadbinTquadbinLogicalFunction::operator==(const LogicalFunctionConcept& rhs) const
{
    if (const auto* other = dynamic_cast<const EverNeQuadbinTquadbinLogicalFunction*>(&rhs))
    {
        return parameters == other->parameters;
    }
    return false;
}

std::string EverNeQuadbinTquadbinLogicalFunction::explain(ExplainVerbosity verbosity) const
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

LogicalFunction EverNeQuadbinTquadbinLogicalFunction::withInferredDataType(const Schema& schema) const
{
    std::vector<LogicalFunction> newChildren;
    newChildren.reserve(parameters.size());
    for (const auto& child : parameters)
    {
        newChildren.emplace_back(child.withInferredDataType(schema));
    }
    return withChildren(newChildren);
}

SerializableFunction EverNeQuadbinTquadbinLogicalFunction::serialize() const
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

LogicalFunctionRegistryReturnType LogicalFunctionGeneratedRegistrar::RegisterEverNeQuadbinTquadbinLogicalFunction(
    LogicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.children.size() == 3,
                 "EverNeQuadbinTquadbinLogicalFunction requires 3 children but got {}",
                 arguments.children.size());
    auto arg0 = std::move(arguments.children[0]);
    auto arg1 = std::move(arguments.children[1]);
    auto arg2 = std::move(arguments.children[2]);
    return EverNeQuadbinTquadbinLogicalFunction(std::move(arg0), std::move(arg1), std::move(arg2));
}

} // namespace NES
