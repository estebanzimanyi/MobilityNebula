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
add_plugin(AcontainsGeoTrgeometry LogicalFunction nes-logical-operators AcontainsGeoTrgeometryLogicalFunction.cpp)
add_plugin(AcoversGeoTrgeometry LogicalFunction nes-logical-operators AcoversGeoTrgeometryLogicalFunction.cpp)
add_plugin(AcoversTrgeometryGeo LogicalFunction nes-logical-operators AcoversTrgeometryGeoLogicalFunction.cpp)
add_plugin(AdisjointTrgeometryGeo LogicalFunction nes-logical-operators AdisjointTrgeometryGeoLogicalFunction.cpp)
add_plugin(AdisjointTrgeometryTrgeometry LogicalFunction nes-logical-operators AdisjointTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(AdwithinTrgeometryGeo LogicalFunction nes-logical-operators AdwithinTrgeometryGeoLogicalFunction.cpp)
add_plugin(AdwithinTrgeometryTrgeometry LogicalFunction nes-logical-operators AdwithinTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(AintersectsTrgeometryGeo LogicalFunction nes-logical-operators AintersectsTrgeometryGeoLogicalFunction.cpp)
add_plugin(AintersectsTrgeometryTrgeometry LogicalFunction nes-logical-operators AintersectsTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(AtouchesTrgeometryGeo LogicalFunction nes-logical-operators AtouchesTrgeometryGeoLogicalFunction.cpp)
add_plugin(EcontainsGeoTrgeometry LogicalFunction nes-logical-operators EcontainsGeoTrgeometryLogicalFunction.cpp)
add_plugin(EcoversGeoTrgeometry LogicalFunction nes-logical-operators EcoversGeoTrgeometryLogicalFunction.cpp)
add_plugin(EcoversTrgeometryGeo LogicalFunction nes-logical-operators EcoversTrgeometryGeoLogicalFunction.cpp)
add_plugin(EdisjointTrgeometryGeo LogicalFunction nes-logical-operators EdisjointTrgeometryGeoLogicalFunction.cpp)
add_plugin(EdisjointTrgeometryTrgeometry LogicalFunction nes-logical-operators EdisjointTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(EdwithinTrgeometryGeo LogicalFunction nes-logical-operators EdwithinTrgeometryGeoLogicalFunction.cpp)
add_plugin(EdwithinTrgeometryTrgeometry LogicalFunction nes-logical-operators EdwithinTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(EintersectsTrgeometryGeo LogicalFunction nes-logical-operators EintersectsTrgeometryGeoLogicalFunction.cpp)
add_plugin(EintersectsTrgeometryTrgeometry LogicalFunction nes-logical-operators EintersectsTrgeometryTrgeometryLogicalFunction.cpp)
add_plugin(EtouchesTrgeometryGeo LogicalFunction nes-logical-operators EtouchesTrgeometryGeoLogicalFunction.cpp)
