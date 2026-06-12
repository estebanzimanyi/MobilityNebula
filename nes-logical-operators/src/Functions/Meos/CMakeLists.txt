# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#    https://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_plugin(Intersect LogicalFunction nes-logical-operators IntersectLogicalFunction.cpp)
#add_plugin(TemporalDWithin LogicalFunction nes-logical-operators TemporalDWithinFunction.cpp)
add_plugin(TemporalIntersects LogicalFunction nes-logical-operators TemporalIntersectsFunction.cpp)
add_plugin(TemporalIntersectsGeometry LogicalFunction nes-logical-operators TemporalIntersectsGeometryLogicalFunction.cpp)
add_plugin(TemporalAIntersectsGeometry LogicalFunction nes-logical-operators TemporalAIntersectsGeometryLogicalFunction.cpp)
add_plugin(TemporalEContainsGeometry LogicalFunction nes-logical-operators TemporalEContainsGeometryLogicalFunction.cpp)
add_plugin(TemporalEDWithinGeometry LogicalFunction nes-logical-operators TemporalEDWithinGeometryLogicalFunction.cpp)
add_plugin(TemporalAtStBox LogicalFunction nes-logical-operators TemporalAtStBoxLogicalFunction.cpp)
add_plugin(TfloatCeil LogicalFunction nes-logical-operators TfloatCeilLogicalFunction.cpp)
add_plugin(TfloatCos LogicalFunction nes-logical-operators TfloatCosLogicalFunction.cpp)
add_plugin(TfloatDegrees LogicalFunction nes-logical-operators TfloatDegreesLogicalFunction.cpp)
add_plugin(TfloatFloor LogicalFunction nes-logical-operators TfloatFloorLogicalFunction.cpp)
add_plugin(TfloatRadians LogicalFunction nes-logical-operators TfloatRadiansLogicalFunction.cpp)
add_plugin(TfloatScaleValue LogicalFunction nes-logical-operators TfloatScaleValueLogicalFunction.cpp)
add_plugin(AddTfloatFloat LogicalFunction nes-logical-operators AddTfloatFloatLogicalFunction.cpp)
add_plugin(AddTintInt LogicalFunction nes-logical-operators AddTintIntLogicalFunction.cpp)
add_plugin(AddTnumberTnumber LogicalFunction nes-logical-operators AddTnumberTnumberLogicalFunction.cpp)
add_plugin(TdistanceTfloatFloat LogicalFunction nes-logical-operators TdistanceTfloatFloatLogicalFunction.cpp)
add_plugin(TdistanceTintInt LogicalFunction nes-logical-operators TdistanceTintIntLogicalFunction.cpp)
add_plugin(TdistanceTnumberTnumber LogicalFunction nes-logical-operators TdistanceTnumberTnumberLogicalFunction.cpp)
add_plugin(TemporalRound LogicalFunction nes-logical-operators TemporalRoundLogicalFunction.cpp)
add_plugin(TintScaleValue LogicalFunction nes-logical-operators TintScaleValueLogicalFunction.cpp)
add_plugin(TintShiftScaleValue LogicalFunction nes-logical-operators TintShiftScaleValueLogicalFunction.cpp)
add_plugin(TintShiftValue LogicalFunction nes-logical-operators TintShiftValueLogicalFunction.cpp)
add_plugin(DivTintInt LogicalFunction nes-logical-operators DivTintIntLogicalFunction.cpp)
add_plugin(DivTnumberTnumber LogicalFunction nes-logical-operators DivTnumberTnumberLogicalFunction.cpp)
add_plugin(MulTintInt LogicalFunction nes-logical-operators MulTintIntLogicalFunction.cpp)
add_plugin(MulTnumberTnumber LogicalFunction nes-logical-operators MulTnumberTnumberLogicalFunction.cpp)
add_plugin(SubTintInt LogicalFunction nes-logical-operators SubTintIntLogicalFunction.cpp)
add_plugin(SubTnumberTnumber LogicalFunction nes-logical-operators SubTnumberTnumberLogicalFunction.cpp)
add_plugin(DivTfloatFloat LogicalFunction nes-logical-operators DivTfloatFloatLogicalFunction.cpp)
add_plugin(MulTfloatFloat LogicalFunction nes-logical-operators MulTfloatFloatLogicalFunction.cpp)
add_plugin(SubTfloatFloat LogicalFunction nes-logical-operators SubTfloatFloatLogicalFunction.cpp)
add_plugin(TfloatShiftScaleValue LogicalFunction nes-logical-operators TfloatShiftScaleValueLogicalFunction.cpp)
add_plugin(TfloatShiftValue LogicalFunction nes-logical-operators TfloatShiftValueLogicalFunction.cpp)
add_plugin(TfloatSin LogicalFunction nes-logical-operators TfloatSinLogicalFunction.cpp)
add_plugin(TfloatTan LogicalFunction nes-logical-operators TfloatTanLogicalFunction.cpp)
add_plugin(TbigintToTfloat LogicalFunction nes-logical-operators TbigintToTfloatLogicalFunction.cpp)
add_plugin(TbigintToTint LogicalFunction nes-logical-operators TbigintToTintLogicalFunction.cpp)
add_plugin(TfloatToTbigint LogicalFunction nes-logical-operators TfloatToTbigintLogicalFunction.cpp)
add_plugin(TfloatToTint LogicalFunction nes-logical-operators TfloatToTintLogicalFunction.cpp)
add_plugin(TintToTbigint LogicalFunction nes-logical-operators TintToTbigintLogicalFunction.cpp)
add_plugin(TintToTfloat LogicalFunction nes-logical-operators TintToTfloatLogicalFunction.cpp)
add_plugin(TnumberAbs LogicalFunction nes-logical-operators TnumberAbsLogicalFunction.cpp)
