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

#include <Functions/Meos/H3indexFromWkbLogicalFunction.hpp>

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

H3indexFromWkbLogicalFunction::H3indexFromWkbLogicalFunction(LogicalFunction wkb)
    : dataType(DataTypeProvider::provideDataType(DataType::Type::UINT64))
{
    parameters.reserve(1);
    parameters.push_back(std::move(wkb));
}

DataType H3indexFromWkbLogicalFunction::getDataType() const { return dataType; }

LogicalFunction H3indexFromWkbLogicalFunction::withDataType(const DataType& newDataType) const
{
    auto copy = *this; copy.dataType = newDataType; return copy;
}

std::vector<LogicalFunction> H3indexFromWkbLogicalFunction::getChildren() const { return parameters; }

LogicalFunction H3indexFromWkbLogicalFunction::withChildren(const std::vector<LogicalFunction>& children) const
{
    PRECONDITION(children.size() == 1,
                 "H3indexFromWkbLogicalFunction requires 1 child, but got {}", children.size());
    auto copy = *this; copy.parameters = children; return copy;
}

std::string_view H3indexFromWkbLogicalFunction::getType() const { return NAME; }

bool H3indexFromWkbLogicalFunction::operator==(const LogicalFunctionConcept& rhs) const
{
    if (const auto* other = dynamic_cast<const H3indexFromWkbLogicalFunction*>(&rhs))
        return parameters == other->parameters;
    return false;
}

std::string H3indexFromWkbLogicalFunction::explain(ExplainVerbosity verbosity) const
{
    return fmt::format("{}({})", NAME, parameters[0].explain(verbosity));
}

LogicalFunction H3indexFromWkbLogicalFunction::withInferredDataType(const Schema& schema) const
{
    std::vector<LogicalFunction> c;
    c.reserve(1);
    c.emplace_back(parameters[0].withInferredDataType(schema));
    INVARIANT(c[0].getDataType().isType(DataType::Type::VARSIZED), "wkb must be VARSIZED");
    return withChildren(c);
}

SerializableFunction H3indexFromWkbLogicalFunction::serialize() const
{
    SerializableFunction proto;
    proto.set_function_type(std::string(NAME));
    DataTypeSerializationUtil::serializeDataType(dataType, proto.mutable_data_type());
    for (const auto& child : parameters)
        proto.add_children()->CopyFrom(child.serialize());
    return proto;
}

LogicalFunctionRegistryReturnType LogicalFunctionGeneratedRegistrar::RegisterH3indexFromWkbLogicalFunction(
    LogicalFunctionRegistryArguments arguments)
{
    PRECONDITION(arguments.children.size() == 1,
                 "H3indexFromWkbLogicalFunction requires 1 child but got {}",
                 arguments.children.size());
    return H3indexFromWkbLogicalFunction(std::move(arguments.children[0]));
}

} // namespace NES
