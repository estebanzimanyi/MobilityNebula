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

#include <RewriteRules/LowerToPhysical/LowerToPhysicalWindowedAggregation.hpp>

#include <cstdint>
#include <memory>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

#include <Aggregation/AggregationBuildPhysicalOperator.hpp>
#include <Aggregation/AggregationOperatorHandler.hpp>
#include <Aggregation/AggregationProbePhysicalOperator.hpp>
#include <Aggregation/Function/AggregationPhysicalFunction.hpp>
#include <DataTypes/DataTypeProvider.hpp>
#include <DataTypes/Schema.hpp>
#include <Functions/FieldAccessPhysicalFunction.hpp>
#include <Functions/FunctionProvider.hpp>
#include <Functions/PhysicalFunction.hpp>
#include <MemoryLayout/ColumnLayout.hpp>
#include <Nautilus/Interface/BufferRef/ColumnTupleBufferRef.hpp>
#include <Nautilus/Interface/Hash/MurMur3HashFunction.hpp>
#include <Nautilus/Interface/HashMap/ChainedHashMap/ChainedEntryMemoryProvider.hpp>
#include <Nautilus/Interface/HashMap/ChainedHashMap/ChainedHashMap.hpp>
#include <Nautilus/Interface/Record.hpp>
#include <Operators/LogicalOperator.hpp>
#include <Operators/Windows/WindowedAggregationLogicalOperator.hpp>
#include <RewriteRules/AbstractRewriteRule.hpp>
#include <Runtime/Execution/OperatorHandler.hpp>
#include <SliceStore/DefaultTimeBasedSliceStore.hpp>
#include <Traits/OutputOriginIdsTrait.hpp>
#include <Traits/TraitSet.hpp>
#include <Watermark/TimeFunction.hpp>
#include <WindowTypes/Measures/TimeCharacteristic.hpp>
#include <WindowTypes/Types/TimeBasedWindowType.hpp>
#include <magic_enum/magic_enum.hpp>
#include <AggregationPhysicalFunctionRegistry.hpp>
#include <ErrorHandling.hpp>
#include <HashMapOptions.hpp>
#include <PhysicalOperator.hpp>
#include <QueryExecutionConfiguration.hpp>
#include <RewriteRuleRegistry.hpp>
// Special-case lowering for TEMPORAL_SEQUENCE (multi-input) aggregation
#include <Operators/Windows/Aggregations/Meos/TemporalSequenceAggregationLogicalFunctionV2.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalLengthAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/PairMeetingAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/CrossDistanceAggregationLogicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalSequenceAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalLengthAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/PairMeetingAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/CrossDistanceAggregationPhysicalFunction.hpp>

#include <Aggregation/Function/Meos/TemporalNumInstantsAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalNumSequencesAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalNumTimestampsAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TfloatStartValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TfloatEndValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TfloatMinValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TfloatMaxValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberIntegralAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TintStartValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TintEndValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TintMinValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TintMaxValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberAvgValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberTwavgAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalStartTimestamptzAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalEndTimestamptzAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalLowerIncAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalUpperIncAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointIsSimpleAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TspatialExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/FloatExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/IntExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/BigintExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TimestamptzExtentTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/FloatUnionTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/IntUnionTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/BigintUnionTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TimestamptzUnionTransfnAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointTrajectoryAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TgeoCentroidAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointAzimuthAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointAngularDifferenceAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TgeompointToTgeometryAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalCopyAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberAbsAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberDeltaValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberAngularDifferenceAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalDerivativeAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalAtMaxAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalAtMinAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalMinusMaxAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TemporalMinusMinAggregationPhysicalFunction.hpp>
#if NPOINT
#include <Aggregation/Function/Meos/Npoint/TnpointCumulativeLengthAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/Npoint/TnpointSpeedAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/Npoint/TnpointToTgeompointAggregationPhysicalFunction.hpp>
#endif
#include <Aggregation/Function/Meos/TpointCumulativeLengthAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointSpeedAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointGetXAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointGetYAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TnumberTrendAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TgeoStartValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TgeoEndValueAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TgeoConvexHullAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/TpointTwcentroidAggregationPhysicalFunction.hpp>
#include <Aggregation/Function/Meos/DateExtentTransfnAggregationPhysicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/DateExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TgeoStartValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TgeoEndValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TgeoConvexHullAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointTwcentroidAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointCumulativeLengthAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointSpeedAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointGetXAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointGetYAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberTrendAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnpointCumulativeLengthAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnpointSpeedAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnpointToTgeompointAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberAbsAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberDeltaValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberAngularDifferenceAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalDerivativeAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalAtMaxAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalAtMinAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalMinusMaxAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalMinusMinAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TgeoCentroidAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointAzimuthAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointAngularDifferenceAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TgeompointToTgeometryAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalCopyAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TLengthExpAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointTrajectoryAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/FloatUnionTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/IntUnionTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/BigintUnionTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TimestamptzUnionTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/FloatExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/IntExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/BigintExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TimestamptzExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TspatialExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberExtentTransfnAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalStartTimestamptzAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalEndTimestamptzAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalLowerIncAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalUpperIncAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TpointIsSimpleAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberAvgValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberTwavgAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalTIntAvgValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalNumInstantsAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalNumSequencesAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TemporalNumTimestampsAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TfloatStartValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TfloatEndValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TfloatMinValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TfloatMaxValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TnumberIntegralAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TintStartValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TintEndValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TintMinValueAggregationLogicalFunction.hpp>
#include <Operators/Windows/Aggregations/Meos/TintMaxValueAggregationLogicalFunction.hpp>
namespace NES
{

static std::pair<std::vector<Record::RecordFieldIdentifier>, std::vector<Record::RecordFieldIdentifier>>
getKeyAndValueFields(const WindowedAggregationLogicalOperator& logicalOperator)
{
    std::vector<Record::RecordFieldIdentifier> fieldKeyNames;
    std::vector<Record::RecordFieldIdentifier> fieldValueNames;

    /// Getting the key and value field names
    for (const auto& nodeAccess : logicalOperator.getGroupingKeys())
    {
        fieldKeyNames.emplace_back(nodeAccess.getFieldName());
    }
    for (const auto& descriptor : logicalOperator.getWindowAggregation())
    {
        const auto aggregationResultFieldIdentifier = descriptor->onField.getFieldName();
        fieldValueNames.emplace_back(aggregationResultFieldIdentifier);
    }
    return {fieldKeyNames, fieldValueNames};
}

static std::unique_ptr<TimeFunction> getTimeFunction(const WindowedAggregationLogicalOperator& logicalOperator)
{
    auto* const timeWindow = dynamic_cast<Windowing::TimeBasedWindowType*>(logicalOperator.getWindowType().get());
    if (timeWindow == nullptr)
    {
        throw UnknownWindowType("Window type is not a time based window type");
    }

    switch (timeWindow->getTimeCharacteristic().getType())
    {
        case Windowing::TimeCharacteristic::Type::IngestionTime: {
            if (timeWindow->getTimeCharacteristic().field.name == Windowing::TimeCharacteristic::RECORD_CREATION_TS_FIELD_NAME)
            {
                return std::make_unique<IngestionTimeFunction>();
            }
            throw UnknownWindowType(
                "The ingestion time field of a window must be: {}", Windowing::TimeCharacteristic::RECORD_CREATION_TS_FIELD_NAME);
        }
        case Windowing::TimeCharacteristic::Type::EventTime: {
            /// For event time fields, we look up the reference field name and create an expression to read the field.
            auto timeCharacteristicField = timeWindow->getTimeCharacteristic().field.name;
            auto timeStampField = FieldAccessPhysicalFunction(timeCharacteristicField);
            return std::make_unique<EventTimeFunction>(timeStampField, timeWindow->getTimeCharacteristic().getTimeUnit());
        }
        default: {
            throw UnknownWindowType("Unknown window type: {}", magic_enum::enum_name(timeWindow->getTimeCharacteristic().getType()));
        }
    }
}

namespace
{
std::vector<std::shared_ptr<AggregationPhysicalFunction>>
getAggregationPhysicalFunctions(const WindowedAggregationLogicalOperator& logicalOperator, const QueryExecutionConfiguration& configuration)
{
    std::vector<std::shared_ptr<AggregationPhysicalFunction>> aggregationPhysicalFunctions;
    const auto& aggregationDescriptors = logicalOperator.getWindowAggregation();
    for (const auto& descriptor : aggregationDescriptors)
    {
        auto physicalInputType = DataTypeProvider::provideDataType(descriptor->getInputStamp().type);
        auto physicalFinalType = DataTypeProvider::provideDataType(descriptor->getFinalAggregateStamp().type);

        const auto resultFieldIdentifier = descriptor->asField.getFieldName();
        auto layout = std::make_shared<ColumnLayout>(configuration.pageSize.getValue(), logicalOperator.getInputSchemas()[0]);
        auto columnBufferRef = std::make_shared<Interface::BufferRef::ColumnTupleBufferRef>(layout);

        const auto name = descriptor->getName();

        // Custom lowering path for TEMPORAL_SEQUENCE: needs three field functions (lon, lat, ts)
        if (name == std::string_view("TemporalSequence"))
        {
            auto tsDescriptor = std::dynamic_pointer_cast<TemporalSequenceAggregationLogicalFunctionV2>(descriptor);
            INVARIANT(tsDescriptor != nullptr, "Expected TemporalSequenceAggregationLogicalFunctionV2 for TemporalSequence");

            // Lower the three input fields (lon, lat, timestamp)
            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(tsDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(tsDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(tsDescriptor->getTimestampField());

            // Create a dedicated in-memory layout for the aggregation state (PagedVector) that
            // matches the field identifiers used by the physical function ("lon", "lat", "timestamp").
            // Using the input schema here is incorrect because it would not match the internal
            // record written by the aggregation state, causing lookups by name to fail.
            Schema stateSchema;
            stateSchema.addField("lon", tsDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", tsDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", tsDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalSequenceAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }

        // Custom lowering path for TEMPORAL_LENGTH: same three-input shape as TEMPORAL_SEQUENCE,
        // returns a FLOAT64 (the spheroidal length of the per-(window, group) trajectory) instead of a VARSIZED WKB blob.
        if (name == std::string_view("TemporalLength"))
        {
            auto tlDescriptor = std::dynamic_pointer_cast<TemporalLengthAggregationLogicalFunction>(descriptor);
            INVARIANT(tlDescriptor != nullptr, "Expected TemporalLengthAggregationLogicalFunction for TemporalLength");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(tlDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(tlDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(tlDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", tlDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", tlDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", tlDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalLengthAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }

        // Custom lowering path for PAIR_MEETING (Q5): four input fields (lon, lat, ts, vehicle_id);
        // returns a VARSIZED string-encoded list of meeting pairs.
        if (name == std::string_view("PairMeeting"))
        {
            auto pmDescriptor = std::dynamic_pointer_cast<PairMeetingAggregationLogicalFunction>(descriptor);
            INVARIANT(pmDescriptor != nullptr, "Expected PairMeetingAggregationLogicalFunction for PairMeeting");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(pmDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(pmDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(pmDescriptor->getTimestampField());
            auto vidPF = QueryCompilation::FunctionProvider::lowerFunction(pmDescriptor->getVehicleIdField());

            Schema stateSchema;
            stateSchema.addField("lon", pmDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", pmDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", pmDescriptor->getTimestampField().getDataType());
            stateSchema.addField("vehicle_id", pmDescriptor->getVehicleIdField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<PairMeetingAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                vidPF,
                pmDescriptor->getDMeetMetres(),
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }

        // Custom lowering path for CROSS_DISTANCE (Q9): four input fields (lon, lat, ts, vehicle_id);
        // returns a FLOAT64 (distance between VID_A and VID_B latest positions in the window).
        if (name == std::string_view("CrossDistance"))
        {
            auto cdDescriptor = std::dynamic_pointer_cast<CrossDistanceAggregationLogicalFunction>(descriptor);
            INVARIANT(cdDescriptor != nullptr, "Expected CrossDistanceAggregationLogicalFunction for CrossDistance");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(cdDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(cdDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(cdDescriptor->getTimestampField());
            auto vidPF = QueryCompilation::FunctionProvider::lowerFunction(cdDescriptor->getVehicleIdField());

            Schema stateSchema;
            stateSchema.addField("lon", cdDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", cdDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", cdDescriptor->getTimestampField().getDataType());
            stateSchema.addField("vehicle_id", cdDescriptor->getVehicleIdField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<CrossDistanceAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                vidPF,
                cdDescriptor->getVidA(),
                cdDescriptor->getVidB(),
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalNumInstants (optimizer lowering) */
        if (name == std::string_view("TemporalNumInstants"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalNumInstantsAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalNumInstantsAggregationLogicalFunction for TemporalNumInstants");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalNumInstantsAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalNumInstants (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalNumSequences (optimizer lowering) */
        if (name == std::string_view("TemporalNumSequences"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalNumSequencesAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalNumSequencesAggregationLogicalFunction for TemporalNumSequences");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalNumSequencesAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalNumSequences (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalNumTimestamps (optimizer lowering) */
        if (name == std::string_view("TemporalNumTimestamps"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalNumTimestampsAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalNumTimestampsAggregationLogicalFunction for TemporalNumTimestamps");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalNumTimestampsAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalNumTimestamps (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TfloatStartValue (optimizer lowering) */
        if (name == std::string_view("TfloatStartValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TfloatStartValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TfloatStartValueAggregationLogicalFunction for TfloatStartValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TfloatStartValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TfloatStartValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TfloatEndValue (optimizer lowering) */
        if (name == std::string_view("TfloatEndValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TfloatEndValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TfloatEndValueAggregationLogicalFunction for TfloatEndValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TfloatEndValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TfloatEndValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TfloatMinValue (optimizer lowering) */
        if (name == std::string_view("TfloatMinValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TfloatMinValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TfloatMinValueAggregationLogicalFunction for TfloatMinValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TfloatMinValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TfloatMinValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TfloatMaxValue (optimizer lowering) */
        if (name == std::string_view("TfloatMaxValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TfloatMaxValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TfloatMaxValueAggregationLogicalFunction for TfloatMaxValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TfloatMaxValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TfloatMaxValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberIntegral (optimizer lowering) */
        if (name == std::string_view("TnumberIntegral"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberIntegralAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberIntegralAggregationLogicalFunction for TnumberIntegral");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberIntegralAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberIntegral (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TintStartValue (optimizer lowering) */
        if (name == std::string_view("TintStartValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TintStartValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TintStartValueAggregationLogicalFunction for TintStartValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TintStartValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TintStartValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TintEndValue (optimizer lowering) */
        if (name == std::string_view("TintEndValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TintEndValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TintEndValueAggregationLogicalFunction for TintEndValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TintEndValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TintEndValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TintMinValue (optimizer lowering) */
        if (name == std::string_view("TintMinValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TintMinValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TintMinValueAggregationLogicalFunction for TintMinValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TintMinValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TintMinValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TintMaxValue (optimizer lowering) */
        if (name == std::string_view("TintMaxValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TintMaxValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TintMaxValueAggregationLogicalFunction for TintMaxValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TintMaxValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TintMaxValue (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberAvgValue (optimizer lowering) */
        if (name == std::string_view("TnumberAvgValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberAvgValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberAvgValueAggregationLogicalFunction for TnumberAvgValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberAvgValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberAvgValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberTwavg (optimizer lowering) */
        if (name == std::string_view("TnumberTwavg"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberTwavgAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberTwavgAggregationLogicalFunction for TnumberTwavg");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberTwavgAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberTwavg (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalStartTimestamptz (optimizer lowering) */
        if (name == std::string_view("TemporalStartTimestamptz"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalStartTimestamptzAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalStartTimestamptzAggregationLogicalFunction for TemporalStartTimestamptz");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalStartTimestamptzAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalStartTimestamptz (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalEndTimestamptz (optimizer lowering) */
        if (name == std::string_view("TemporalEndTimestamptz"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalEndTimestamptzAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalEndTimestamptzAggregationLogicalFunction for TemporalEndTimestamptz");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalEndTimestamptzAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalEndTimestamptz (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalLowerInc (optimizer lowering) */
        if (name == std::string_view("TemporalLowerInc"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalLowerIncAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalLowerIncAggregationLogicalFunction for TemporalLowerInc");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalLowerIncAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalLowerInc (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalUpperInc (optimizer lowering) */
        if (name == std::string_view("TemporalUpperInc"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalUpperIncAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalUpperIncAggregationLogicalFunction for TemporalUpperInc");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalUpperIncAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalUpperInc (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointIsSimple (optimizer lowering) */
        if (name == std::string_view("TpointIsSimple"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointIsSimpleAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointIsSimpleAggregationLogicalFunction for TpointIsSimple");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointIsSimpleAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointIsSimple (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TspatialExtentTransfn (optimizer lowering) */
        if (name == std::string_view("TspatialExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TspatialExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TspatialExtentTransfnAggregationLogicalFunction for TspatialExtentTransfn");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TspatialExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TspatialExtentTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberExtentTransfn (optimizer lowering) */
        if (name == std::string_view("TnumberExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberExtentTransfnAggregationLogicalFunction for TnumberExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberExtentTransfn (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: FloatExtentTransfn (optimizer lowering) */
        if (name == std::string_view("FloatExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<FloatExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected FloatExtentTransfnAggregationLogicalFunction for FloatExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<FloatExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: FloatExtentTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: IntExtentTransfn (optimizer lowering) */
        if (name == std::string_view("IntExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<IntExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected IntExtentTransfnAggregationLogicalFunction for IntExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<IntExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: IntExtentTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: BigintExtentTransfn (optimizer lowering) */
        if (name == std::string_view("BigintExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<BigintExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected BigintExtentTransfnAggregationLogicalFunction for BigintExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<BigintExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: BigintExtentTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TimestamptzExtentTransfn (optimizer lowering) */
        if (name == std::string_view("TimestamptzExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TimestamptzExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TimestamptzExtentTransfnAggregationLogicalFunction for TimestamptzExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TimestamptzExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TimestamptzExtentTransfn (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: FloatUnionTransfn (optimizer lowering) */
        if (name == std::string_view("FloatUnionTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<FloatUnionTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected FloatUnionTransfnAggregationLogicalFunction for FloatUnionTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<FloatUnionTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: FloatUnionTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: IntUnionTransfn (optimizer lowering) */
        if (name == std::string_view("IntUnionTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<IntUnionTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected IntUnionTransfnAggregationLogicalFunction for IntUnionTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<IntUnionTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: IntUnionTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: BigintUnionTransfn (optimizer lowering) */
        if (name == std::string_view("BigintUnionTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<BigintUnionTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected BigintUnionTransfnAggregationLogicalFunction for BigintUnionTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<BigintUnionTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: BigintUnionTransfn (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TimestamptzUnionTransfn (optimizer lowering) */
        if (name == std::string_view("TimestamptzUnionTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TimestamptzUnionTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TimestamptzUnionTransfnAggregationLogicalFunction for TimestamptzUnionTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TimestamptzUnionTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TimestamptzUnionTransfn (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TpointTrajectory (optimizer lowering) */
        if (name == std::string_view("TpointTrajectory"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointTrajectoryAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointTrajectoryAggregationLogicalFunction for TpointTrajectory");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointTrajectoryAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointTrajectory (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TgeoCentroid (optimizer lowering) */
        if (name == std::string_view("TgeoCentroid"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TgeoCentroidAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TgeoCentroidAggregationLogicalFunction for TgeoCentroid");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TgeoCentroidAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TgeoCentroid (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointAzimuth (optimizer lowering) */
        if (name == std::string_view("TpointAzimuth"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointAzimuthAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointAzimuthAggregationLogicalFunction for TpointAzimuth");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointAzimuthAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointAzimuth (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointAngularDifference (optimizer lowering) */
        if (name == std::string_view("TpointAngularDifference"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointAngularDifferenceAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointAngularDifferenceAggregationLogicalFunction for TpointAngularDifference");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointAngularDifferenceAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointAngularDifference (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TgeompointToTgeometry (optimizer lowering) */
        if (name == std::string_view("TgeompointToTgeometry"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TgeompointToTgeometryAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TgeompointToTgeometryAggregationLogicalFunction for TgeompointToTgeometry");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TgeompointToTgeometryAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TgeompointToTgeometry (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalCopy (optimizer lowering) */
        if (name == std::string_view("TemporalCopy"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalCopyAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalCopyAggregationLogicalFunction for TemporalCopy");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalCopyAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalCopy (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberAbs (optimizer lowering) */
        if (name == std::string_view("TnumberAbs"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberAbsAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberAbsAggregationLogicalFunction for TnumberAbs");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberAbsAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberAbs (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberDeltaValue (optimizer lowering) */
        if (name == std::string_view("TnumberDeltaValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberDeltaValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberDeltaValueAggregationLogicalFunction for TnumberDeltaValue");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberDeltaValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberDeltaValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberAngularDifference (optimizer lowering) */
        if (name == std::string_view("TnumberAngularDifference"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberAngularDifferenceAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberAngularDifferenceAggregationLogicalFunction for TnumberAngularDifference");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberAngularDifferenceAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberAngularDifference (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalDerivative (optimizer lowering) */
        if (name == std::string_view("TemporalDerivative"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalDerivativeAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalDerivativeAggregationLogicalFunction for TemporalDerivative");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalDerivativeAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalDerivative (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalAtMax (optimizer lowering) */
        if (name == std::string_view("TemporalAtMax"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalAtMaxAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalAtMaxAggregationLogicalFunction for TemporalAtMax");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalAtMaxAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalAtMax (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalAtMin (optimizer lowering) */
        if (name == std::string_view("TemporalAtMin"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalAtMinAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalAtMinAggregationLogicalFunction for TemporalAtMin");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalAtMinAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalAtMin (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalMinusMax (optimizer lowering) */
        if (name == std::string_view("TemporalMinusMax"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalMinusMaxAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalMinusMaxAggregationLogicalFunction for TemporalMinusMax");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalMinusMaxAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalMinusMax (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TemporalMinusMin (optimizer lowering) */
        if (name == std::string_view("TemporalMinusMin"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TemporalMinusMinAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TemporalMinusMinAggregationLogicalFunction for TemporalMinusMin");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TemporalMinusMinAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TemporalMinusMin (optimizer lowering) */
#if NPOINT
        /* BEGIN CODEGEN AGGREGATION GLUE: TnpointCumulativeLength (optimizer lowering) */
        if (name == std::string_view("TnpointCumulativeLength"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnpointCumulativeLengthAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnpointCumulativeLengthAggregationLogicalFunction for TnpointCumulativeLength");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnpointCumulativeLengthAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnpointCumulativeLength (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnpointSpeed (optimizer lowering) */
        if (name == std::string_view("TnpointSpeed"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnpointSpeedAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnpointSpeedAggregationLogicalFunction for TnpointSpeed");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnpointSpeedAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnpointSpeed (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnpointToTgeompoint (optimizer lowering) */
        if (name == std::string_view("TnpointToTgeompoint"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnpointToTgeompointAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnpointToTgeompointAggregationLogicalFunction for TnpointToTgeompoint");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnpointToTgeompointAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnpointToTgeompoint (optimizer lowering) */
#endif
        /* BEGIN CODEGEN AGGREGATION GLUE: TpointCumulativeLength (optimizer lowering) */
        if (name == std::string_view("TpointCumulativeLength"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointCumulativeLengthAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointCumulativeLengthAggregationLogicalFunction for TpointCumulativeLength");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointCumulativeLengthAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointCumulativeLength (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointSpeed (optimizer lowering) */
        if (name == std::string_view("TpointSpeed"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointSpeedAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointSpeedAggregationLogicalFunction for TpointSpeed");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointSpeedAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointSpeed (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointGetX (optimizer lowering) */
        if (name == std::string_view("TpointGetX"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointGetXAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointGetXAggregationLogicalFunction for TpointGetX");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointGetXAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointGetX (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointGetY (optimizer lowering) */
        if (name == std::string_view("TpointGetY"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointGetYAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointGetYAggregationLogicalFunction for TpointGetY");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointGetYAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointGetY (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TnumberTrend (optimizer lowering) */
        if (name == std::string_view("TnumberTrend"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TnumberTrendAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TnumberTrendAggregationLogicalFunction for TnumberTrend");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TnumberTrendAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TnumberTrend (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: TgeoStartValue (optimizer lowering) */
        if (name == std::string_view("TgeoStartValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TgeoStartValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TgeoStartValueAggregationLogicalFunction for TgeoStartValue");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TgeoStartValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TgeoStartValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TgeoEndValue (optimizer lowering) */
        if (name == std::string_view("TgeoEndValue"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TgeoEndValueAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TgeoEndValueAggregationLogicalFunction for TgeoEndValue");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TgeoEndValueAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TgeoEndValue (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TgeoConvexHull (optimizer lowering) */
        if (name == std::string_view("TgeoConvexHull"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TgeoConvexHullAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TgeoConvexHullAggregationLogicalFunction for TgeoConvexHull");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TgeoConvexHullAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TgeoConvexHull (optimizer lowering) */

        /* BEGIN CODEGEN AGGREGATION GLUE: TpointTwcentroid (optimizer lowering) */
        if (name == std::string_view("TpointTwcentroid"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<TpointTwcentroidAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected TpointTwcentroidAggregationLogicalFunction for TpointTwcentroid");

            auto lonPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLonField());
            auto latPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getLatField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("lon", specificDescriptor->getLonField().getDataType());
            stateSchema.addField("lat", specificDescriptor->getLatField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<TpointTwcentroidAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                lonPF,
                latPF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: TpointTwcentroid (optimizer lowering) */
        /* BEGIN CODEGEN AGGREGATION GLUE: DateExtentTransfn (optimizer lowering) */
        if (name == std::string_view("DateExtentTransfn"))
        {
            auto specificDescriptor = std::dynamic_pointer_cast<DateExtentTransfnAggregationLogicalFunction>(descriptor);
            INVARIANT(specificDescriptor != nullptr, "Expected DateExtentTransfnAggregationLogicalFunction for DateExtentTransfn");

            auto valuePF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getValueField());
            auto tsPF = QueryCompilation::FunctionProvider::lowerFunction(specificDescriptor->getTimestampField());

            Schema stateSchema;
            stateSchema.addField("value", specificDescriptor->getValueField().getDataType());
            stateSchema.addField("timestamp", specificDescriptor->getTimestampField().getDataType());
            auto tupleBufferRef = Interface::BufferRef::TupleBufferRef::create(configuration.pageSize.getValue(), stateSchema);

            auto phys = std::make_shared<DateExtentTransfnAggregationPhysicalFunction>(
                std::move(physicalInputType),
                std::move(physicalFinalType),
                valuePF,
                tsPF,
                resultFieldIdentifier,
                tupleBufferRef);
            aggregationPhysicalFunctions.push_back(std::move(phys));
            continue;
        }
        /* END CODEGEN AGGREGATION GLUE: DateExtentTransfn (optimizer lowering) */















        // Default path: use registry for single-input aggregations
        auto aggregationInputFunction = QueryCompilation::FunctionProvider::lowerFunction(descriptor->onField);
        auto aggregationArguments = AggregationPhysicalFunctionRegistryArguments(
            std::move(physicalInputType),
            std::move(physicalFinalType),
            std::move(aggregationInputFunction),
            resultFieldIdentifier,
            columnBufferRef);
        if (auto aggregationPhysicalFunction
            = AggregationPhysicalFunctionRegistry::instance().create(std::string(name), std::move(aggregationArguments)))
        {
            aggregationPhysicalFunctions.push_back(aggregationPhysicalFunction.value());
        }
        else
        {
            throw UnknownAggregationType("unknown aggregation type: {}", name);
        }
    }
    return aggregationPhysicalFunctions;
}
}

RewriteRuleResultSubgraph LowerToPhysicalWindowedAggregation::apply(LogicalOperator logicalOperator)
{
    PRECONDITION(logicalOperator.tryGetAs<WindowedAggregationLogicalOperator>(), "Expected a WindowedAggregationLogicalOperator");
    PRECONDITION(std::ranges::size(logicalOperator.getChildren()) == 1, "Expected one child");
    auto outputOriginIdsOpt = getTrait<OutputOriginIdsTrait>(logicalOperator.getTraitSet());
    auto inputOriginIdsOpt = getTrait<OutputOriginIdsTrait>(logicalOperator.getChildren().at(0).getTraitSet());
    PRECONDITION(outputOriginIdsOpt.has_value(), "Expected the outputOriginIds trait to be set");
    PRECONDITION(inputOriginIdsOpt.has_value(), "Expected the inputOriginIds trait to be set");
    auto& outputOriginIds = outputOriginIdsOpt.value();
    PRECONDITION(std::ranges::size(outputOriginIds) == 1, "Expected one output origin id");
    PRECONDITION(logicalOperator.getInputSchemas().size() == 1, "Expected one input schema");

    auto aggregation = logicalOperator.getAs<WindowedAggregationLogicalOperator>();
    auto handlerId = getNextOperatorHandlerId();
    auto outputSchema = aggregation.getOutputSchema();
    auto outputOriginId = outputOriginIds[0];
    auto inputOriginIds = inputOriginIdsOpt.value();
    auto timeFunction = getTimeFunction(*aggregation);
    auto windowType = std::dynamic_pointer_cast<Windowing::TimeBasedWindowType>(aggregation->getWindowType());
    INVARIANT(windowType != nullptr, "Window type must be a time-based window type");
    auto aggregationPhysicalFunctions = getAggregationPhysicalFunctions(*aggregation, conf);

    const auto valueSize = std::accumulate(
        aggregationPhysicalFunctions.begin(),
        aggregationPhysicalFunctions.end(),
        0,
        [](const auto& sum, const auto& function) { return sum + function->getSizeOfStateInBytes(); });

    uint64_t keySize = 0;
    std::vector<PhysicalFunction> keyFunctions;
    auto newInputSchema = aggregation.getInputSchemas()[0];
    for (auto& nodeFunctionKey : aggregation->getGroupingKeys())
    {
        auto loweredFunctionType = nodeFunctionKey.getDataType();
        if (loweredFunctionType.isType(DataType::Type::VARSIZED))
        {
            loweredFunctionType.type = DataType::Type::VARSIZED_POINTER_REP;
            const bool fieldReplaceSuccess = newInputSchema.replaceTypeOfField(nodeFunctionKey.getFieldName(), loweredFunctionType);
            INVARIANT(fieldReplaceSuccess, "Expect to change the type of {} for {}", nodeFunctionKey.getFieldName(), newInputSchema);
        }
        keyFunctions.emplace_back(QueryCompilation::FunctionProvider::lowerFunction(nodeFunctionKey));
        keySize += DataTypeProvider::provideDataType(loweredFunctionType.type).getSizeInBytes();
    }
    const auto entrySize = sizeof(Interface::ChainedHashMapEntry) + keySize + valueSize;
    const auto numberOfBuckets = conf.numberOfPartitions.getValue();
    const auto pageSize = conf.pageSize.getValue();
    const auto entriesPerPage = pageSize / entrySize;

    const auto& [fieldKeyNames, fieldValueNames] = getKeyAndValueFields(*aggregation);
    const auto& [fieldKeys, fieldValues]
        = Interface::BufferRef::ChainedEntryMemoryProvider::createFieldOffsets(newInputSchema, fieldKeyNames, fieldValueNames);

    const auto windowMetaData = WindowMetaData{aggregation->getWindowStartFieldName(), aggregation->getWindowEndFieldName()};

    const HashMapOptions hashMapOptions(
        std::make_unique<Interface::MurMur3HashFunction>(),
        keyFunctions,
        fieldKeys,
        fieldValues,
        entriesPerPage,
        entrySize,
        keySize,
        valueSize,
        pageSize,
        numberOfBuckets);

    auto sliceAndWindowStore
        = std::make_unique<DefaultTimeBasedSliceStore>(windowType->getSize().getTime(), windowType->getSlide().getTime());
    auto handler = std::make_shared<AggregationOperatorHandler>(
        inputOriginIds | std::ranges::to<std::vector>(), outputOriginId, std::move(sliceAndWindowStore), conf.maxNumberOfBuckets);
    auto build = AggregationBuildPhysicalOperator(handlerId, std::move(timeFunction), aggregationPhysicalFunctions, hashMapOptions);
    auto probe = AggregationProbePhysicalOperator(hashMapOptions, aggregationPhysicalFunctions, handlerId, windowMetaData);

    auto buildWrapper = std::make_shared<PhysicalOperatorWrapper>(
        build, newInputSchema, outputSchema, handlerId, handler, PhysicalOperatorWrapper::PipelineLocation::EMIT);

    auto probeWrapper = std::make_shared<PhysicalOperatorWrapper>(
        probe,
        newInputSchema,
        outputSchema,
        handlerId,
        handler,
        PhysicalOperatorWrapper::PipelineLocation::SCAN,
        std::vector{buildWrapper});

    /// Creates a physical leaf for each logical leaf. Required, as this operator can have any number of sources.
    std::vector leafes(logicalOperator.getChildren().size(), buildWrapper);
    return {.root = probeWrapper, .leafs = leafes};
}

std::unique_ptr<AbstractRewriteRule>
RewriteRuleGeneratedRegistrar::RegisterWindowedAggregationRewriteRule(RewriteRuleRegistryArguments argument) /// NOLINT
{
    return std::make_unique<LowerToPhysicalWindowedAggregation>(argument.conf);
}
}
