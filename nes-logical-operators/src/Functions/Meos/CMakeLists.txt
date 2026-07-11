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
add_plugin(AlwaysEqBigintTbigint LogicalFunction nes-logical-operators AlwaysEqBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysEqTbigintBigint LogicalFunction nes-logical-operators AlwaysEqTbigintBigintLogicalFunction.cpp)
add_plugin(AlwaysGeBigintTbigint LogicalFunction nes-logical-operators AlwaysGeBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysGeTbigintBigint LogicalFunction nes-logical-operators AlwaysGeTbigintBigintLogicalFunction.cpp)
add_plugin(AlwaysGtBigintTbigint LogicalFunction nes-logical-operators AlwaysGtBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysGtTbigintBigint LogicalFunction nes-logical-operators AlwaysGtTbigintBigintLogicalFunction.cpp)
add_plugin(AlwaysLeBigintTbigint LogicalFunction nes-logical-operators AlwaysLeBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysLeTbigintBigint LogicalFunction nes-logical-operators AlwaysLeTbigintBigintLogicalFunction.cpp)
add_plugin(AlwaysLtBigintTbigint LogicalFunction nes-logical-operators AlwaysLtBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysLtTbigintBigint LogicalFunction nes-logical-operators AlwaysLtTbigintBigintLogicalFunction.cpp)
add_plugin(AlwaysNeBigintTbigint LogicalFunction nes-logical-operators AlwaysNeBigintTbigintLogicalFunction.cpp)
add_plugin(AlwaysNeTbigintBigint LogicalFunction nes-logical-operators AlwaysNeTbigintBigintLogicalFunction.cpp)
add_plugin(EverEqBigintTbigint LogicalFunction nes-logical-operators EverEqBigintTbigintLogicalFunction.cpp)
add_plugin(EverEqTbigintBigint LogicalFunction nes-logical-operators EverEqTbigintBigintLogicalFunction.cpp)
add_plugin(EverGeBigintTbigint LogicalFunction nes-logical-operators EverGeBigintTbigintLogicalFunction.cpp)
add_plugin(EverGeTbigintBigint LogicalFunction nes-logical-operators EverGeTbigintBigintLogicalFunction.cpp)
add_plugin(EverGtBigintTbigint LogicalFunction nes-logical-operators EverGtBigintTbigintLogicalFunction.cpp)
add_plugin(EverGtTbigintBigint LogicalFunction nes-logical-operators EverGtTbigintBigintLogicalFunction.cpp)
add_plugin(EverLeBigintTbigint LogicalFunction nes-logical-operators EverLeBigintTbigintLogicalFunction.cpp)
add_plugin(EverLeTbigintBigint LogicalFunction nes-logical-operators EverLeTbigintBigintLogicalFunction.cpp)
add_plugin(EverLtBigintTbigint LogicalFunction nes-logical-operators EverLtBigintTbigintLogicalFunction.cpp)
add_plugin(EverLtTbigintBigint LogicalFunction nes-logical-operators EverLtTbigintBigintLogicalFunction.cpp)
add_plugin(EverNeBigintTbigint LogicalFunction nes-logical-operators EverNeBigintTbigintLogicalFunction.cpp)
add_plugin(EverNeTbigintBigint LogicalFunction nes-logical-operators EverNeTbigintBigintLogicalFunction.cpp)
