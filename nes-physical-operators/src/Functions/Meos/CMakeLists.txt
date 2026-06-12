# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#    https://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(NES_ENABLE_MEOS)
add_plugin(TemporalIntersects PhysicalFunction nes-physical-operators TemporalIntersectsPhysicalFunction.cpp)
add_plugin(TemporalIntersectsGeometry PhysicalFunction nes-physical-operators TemporalIntersectsGeometryPhysicalFunction.cpp)
add_plugin(TemporalAIntersectsGeometry PhysicalFunction nes-physical-operators TemporalAIntersectsGeometryPhysicalFunction.cpp)
add_plugin(TemporalEContainsGeometry PhysicalFunction nes-physical-operators TemporalEContainsGeometryPhysicalFunction.cpp)
add_plugin(TemporalEDWithinGeometry PhysicalFunction nes-physical-operators TemporalEDWithinGeometryPhysicalFunction.cpp)
add_plugin(TemporalAtStBox PhysicalFunction nes-physical-operators TemporalAtStBoxPhysicalFunction.cpp)
add_plugin(TfloatCeil PhysicalFunction nes-physical-operators TfloatCeilPhysicalFunction.cpp)
add_plugin(TfloatCos PhysicalFunction nes-physical-operators TfloatCosPhysicalFunction.cpp)
add_plugin(TfloatDegrees PhysicalFunction nes-physical-operators TfloatDegreesPhysicalFunction.cpp)
add_plugin(TfloatFloor PhysicalFunction nes-physical-operators TfloatFloorPhysicalFunction.cpp)
add_plugin(TfloatRadians PhysicalFunction nes-physical-operators TfloatRadiansPhysicalFunction.cpp)
add_plugin(TfloatScaleValue PhysicalFunction nes-physical-operators TfloatScaleValuePhysicalFunction.cpp)
add_plugin(AddTfloatFloat PhysicalFunction nes-physical-operators AddTfloatFloatPhysicalFunction.cpp)
add_plugin(AddTintInt PhysicalFunction nes-physical-operators AddTintIntPhysicalFunction.cpp)
add_plugin(AddTnumberTnumber PhysicalFunction nes-physical-operators AddTnumberTnumberPhysicalFunction.cpp)
add_plugin(TdistanceTfloatFloat PhysicalFunction nes-physical-operators TdistanceTfloatFloatPhysicalFunction.cpp)
add_plugin(TdistanceTintInt PhysicalFunction nes-physical-operators TdistanceTintIntPhysicalFunction.cpp)
add_plugin(TdistanceTnumberTnumber PhysicalFunction nes-physical-operators TdistanceTnumberTnumberPhysicalFunction.cpp)
add_plugin(TemporalRound PhysicalFunction nes-physical-operators TemporalRoundPhysicalFunction.cpp)
add_plugin(TintScaleValue PhysicalFunction nes-physical-operators TintScaleValuePhysicalFunction.cpp)
add_plugin(TintShiftScaleValue PhysicalFunction nes-physical-operators TintShiftScaleValuePhysicalFunction.cpp)
add_plugin(TintShiftValue PhysicalFunction nes-physical-operators TintShiftValuePhysicalFunction.cpp)
add_plugin(DivTintInt PhysicalFunction nes-physical-operators DivTintIntPhysicalFunction.cpp)
add_plugin(DivTnumberTnumber PhysicalFunction nes-physical-operators DivTnumberTnumberPhysicalFunction.cpp)
add_plugin(MulTintInt PhysicalFunction nes-physical-operators MulTintIntPhysicalFunction.cpp)
add_plugin(MulTnumberTnumber PhysicalFunction nes-physical-operators MulTnumberTnumberPhysicalFunction.cpp)
add_plugin(SubTintInt PhysicalFunction nes-physical-operators SubTintIntPhysicalFunction.cpp)
add_plugin(SubTnumberTnumber PhysicalFunction nes-physical-operators SubTnumberTnumberPhysicalFunction.cpp)
add_plugin(DivTfloatFloat PhysicalFunction nes-physical-operators DivTfloatFloatPhysicalFunction.cpp)
add_plugin(MulTfloatFloat PhysicalFunction nes-physical-operators MulTfloatFloatPhysicalFunction.cpp)
add_plugin(SubTfloatFloat PhysicalFunction nes-physical-operators SubTfloatFloatPhysicalFunction.cpp)
add_plugin(TfloatShiftScaleValue PhysicalFunction nes-physical-operators TfloatShiftScaleValuePhysicalFunction.cpp)
add_plugin(TfloatShiftValue PhysicalFunction nes-physical-operators TfloatShiftValuePhysicalFunction.cpp)
add_plugin(TfloatSin PhysicalFunction nes-physical-operators TfloatSinPhysicalFunction.cpp)
add_plugin(TfloatTan PhysicalFunction nes-physical-operators TfloatTanPhysicalFunction.cpp)
add_plugin(TfloatToTint PhysicalFunction nes-physical-operators TfloatToTintPhysicalFunction.cpp)
add_plugin(TintToTfloat PhysicalFunction nes-physical-operators TintToTfloatPhysicalFunction.cpp)
add_plugin(TnumberAbs PhysicalFunction nes-physical-operators TnumberAbsPhysicalFunction.cpp)
endif()
