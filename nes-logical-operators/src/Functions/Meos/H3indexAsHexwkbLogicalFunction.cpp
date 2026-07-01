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

#include <Functions/Meos/H3indexAsHexwkbLogicalFunction.hpp>

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

H3indexAsHexwkbLogicalFunction::H3indexAsHexwkbLogicalFunction(LogicalFunction cell, LogicalFunction variant)
    : dataType(DataTypeProvider::provideDataType(DataType::Type::VARSIZED))
{
    parameters.reserve(2);
    parameters.push_back(std::move(cell));
    parameters.push_back(std::move(variant));
}

DataType H3indexAsHexwkbLogicalFunction::getDataType() const { return dataType; }

LogicalFunction H3indexAsHexwkbLogicalFunction::withDataType(const DataType& newDataType) const
{
    auto copy = *this; copy.dataType = newDataType; return copy;
}

std::vector<LogicalFunction> H3indexAsHexwkbLogicalFunction::getChildren() const { return parameters; }

LogicalFunction H3indexAsHexwkbLogicalFunction::withChildren(const std::vector<LogicalFunction>& children) const
{
    PRECONDITION(children.size() == 2,
                 "H3indexAsHexwkbLogicalFunction requires 2 children, but got {}", children.size());
    auto copy = *this; copy.parameters = children; return copy;
}

std::string_view H3indexAsHexwkbLogicalFunction::getType() const { return NAME; }

bool H3indexAsHexwkbLogicalFunction::operator==(const LogicalFunctionConcept& rhs) const
{
    if (const auto* other = dynamic_cast<const H3indexAsHexwkbLogicalFunction*>(&rhs))
        return parameters == other->parameters;
    return false;
}

std::string H3indexAsHexwkbLogicalFunction::explain(ExplainVerbosity verbosity) const
{
    return fmt::format("{}({}, {})", NAME, parameters[0].explain(verbosity), parameters[1].explain(verbosity));
}

LogicalFunction H3indexAsHexwkbLogicalFunction::withInferredDataType(const Schema& schema) const
{
    std::vector<LogicalFunction> c;
    c.reserve(2);
    c.emplace_back(parameters[0].withInferredDataType(schema));
    c.emplace_back(parameters[1].withInferredDataType(schema));
    INVARIANT(c[0].getDataType().isType(DataType::Type::UINT64), "cell must be UINT64");
    INVARIANT(c[1].getDataType().isType(DataType::Type::UINT64), "variant must be UINT64");
    return withChildren(c);
}

SerializableFunction H3indexAsHexwkbLogicalFunction::serialize() const
{
    SerializableFunction proto;
    proto.set_function_type(std::string(NAME));
    DataTypeSerializationUtil::serializeDataType(dataType, proto.mutable_data_type());
    for (const auto& child : parameters)
        proto.add_children()->CopyFrom(child.serialize());
    return proto;
}

LogicalFunctionRegistryReturnType LogicalFunctionGeneratedRegistrar::RegisterH3indexAsHexwkbLogicalFunction(
    LogicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.children.size() == 2,
                 "H3indexAsHexwkbLogicalFunction requires 2 children but got {}",
                 arguments.children.size());
    return H3indexAsHexwkbLogicalFunction(std::move(arguments.children[0]), std::move(arguments.children[1]));
}

} // namespace NES
