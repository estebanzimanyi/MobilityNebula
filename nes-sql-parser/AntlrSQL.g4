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

grammar AntlrSQL;

@lexer::postinclude {
#include <Util/DisableWarningsPragma.hpp>
DISABLE_WARNING_PUSH
DISABLE_WARNING(-Wlogical-op-parentheses)
DISABLE_WARNING(-Wunused-parameter)
}

@parser::postinclude {
#include <Util/DisableWarningsPragma.hpp>
DISABLE_WARNING_PUSH
DISABLE_WARNING(-Wlogical-op-parentheses)
DISABLE_WARNING(-Wunused-parameter)
}

@parser::members {
      bool SQL_standard_keyword_behavior = false;
      bool legacy_exponent_literal_as_decimal_enabled = false;
}

@lexer::members {
  bool isValidDecimal() {
    int nextChar = _input->LA(1);
    if (nextChar >= 'A' && nextChar <= 'Z' || nextChar >= '0' && nextChar <= '9' ||
      nextChar == '_') {
      return false;
    } else {
      return true;
    }
  }

  bool isHint() {
    int nextChar = _input->LA(1);
    if (nextChar == '+') {
      return true;
    } else {
      return false;
    }
  }
}

singleStatement: statement ';'? EOF;

terminatedStatement: statement ';';
multipleStatements: (statement (';' statement)* ';'?)? EOF;
statement: query | createStatement | dropStatement | showStatement;

createStatement: CREATE createDefinition;
createDefinition: createLogicalSourceDefinition | createPhysicalSourceDefinition | createSinkDefinition;
createLogicalSourceDefinition: LOGICAL SOURCE sourceName=identifier schemaDefinition fromQuery?;

createPhysicalSourceDefinition: PHYSICAL SOURCE FOR logicalSource=identifier
                                TYPE type=identifier
                                (SET '(' options=namedConfigExpressionSeq ')')?;

createSinkDefinition: SINK sinkName=identifier schemaDefinition TYPE type=identifier (SET '(' options=namedConfigExpressionSeq ')')?;


schemaDefinition: '(' columnDefinition (',' columnDefinition)* ')';
columnDefinition: identifierChain typeDefinition;

typeDefinition: DATA_TYPE;

fromQuery: AS query;

dropStatement: DROP dropSubject;
dropSubject: dropQuery | dropSource | dropSink;
dropQuery: QUERY id=unsignedIntegerLiteral;
dropSource: dropLogicalSourceSubject | dropPhysicalSourceSubject;
dropLogicalSourceSubject: LOGICAL SOURCE name=strictIdentifier;
dropPhysicalSourceSubject: PHYSICAL SOURCE id=unsignedIntegerLiteral;
dropSink: SINK name=strictIdentifier;

showStatement: SHOW showSubject (WHERE showFilter)? (FORMAT showFormat)?;
showFormat: TEXT | JSON;
showSubject: QUERIES #showQueriesSubject
    | LOGICAL SOURCES #showLogicalSourcesSubject
    | PHYSICAL SOURCES (FOR logicalSourceName=strictIdentifier)? #showPhysicalSourcesSubject
    | SINKS #showSinksSubject;

showFilter: attr=strictIdentifier EQ value=constant;

query : queryTerm queryOrganization;

queryOrganization:
         (ORDER BY order+=sortItem (',' order+=sortItem)*)?
         (LIMIT (ALL | limit=INTEGER_VALUE))?
         (OFFSET offset=INTEGER_VALUE)?
         ;

queryTerm: queryPrimary #primaryQuery
         |  left=queryTerm setoperator=UNION right=queryTerm  #setOperation
         ;

queryPrimary
    : querySpecification                                                    #queryPrimaryDefault
    | fromStatement                                                         #fromStmt
    | TABLE multipartIdentifier                                             #table
    | inlineTable                                                           #inlineTableDefault1
    | '(' query ')'                                                         #subquery
    ;
/// new layout to be closer to traditional SQL
querySpecification: selectClause fromClause whereClause? windowedAggregationClause? havingClause? sinkClause?;


fromClause: FROM relation (',' relation)*;

relation
    : relationPrimary joinRelation*
    ;

joinRelation
    : (joinType) JOIN right=relationPrimary joinCriteria? windowClause
    | NATURAL joinType JOIN right=relationPrimary windowClause
    ;

joinType
    : INNER?
    ;

joinCriteria
    : ON booleanExpression
    ;

relationPrimary
    : multipartIdentifier tableAlias          #tableName
    | '(' query ')'  tableAlias               #aliasedQuery
    | '(' relation ')' tableAlias             #aliasedRelation
    | inlineTable                             #inlineTableDefault2
    | inlineSource                            #inlineDefinedSource
    ;

inlineSource
    : type=identifier '(' parameters=namedConfigExpressionSeq ')'
    ;

schema: SCHEMA schemaDefinition
 ;

fromStatement: fromClause fromStatementBody+;

fromStatementBody: selectClause whereClause? groupByClause?;

selectClause : SELECT (hints+=hint)* namedExpressionSeq;

whereClause: WHERE booleanExpression;

havingClause: HAVING booleanExpression;

inlineTable
    : VALUES expression (',' expression)* tableAlias
    ;

tableAlias
    : (AS? identifier identifierList?)?
    ;

multipartIdentifier
    : parts+=errorCapturingIdentifier ('.' parts+=errorCapturingIdentifier)*
    ;

namedConfigExpression: (constant | schema) AS name=identifierChain;

namedExpression
    : expression AS name=identifier
    | expression
    ;

identifier: strictIdentifier;

strictIdentifier
    : IDENTIFIER #unquotedIdentifier
    | quotedIdentifier #quotedIdentifierAlternative;

quotedIdentifier
    : BACKQUOTED_IDENTIFIER
    ;

BACKQUOTED_IDENTIFIER
    : '`' ( ~'`' | '``' )* '`'
    ;

identifierChain: strictIdentifier ('.' strictIdentifier)*;

identifierList
    : '(' identifierSeq ')'
    ;

identifierSeq
    : ident+=errorCapturingIdentifier (',' ident+=errorCapturingIdentifier)*
    ;

errorCapturingIdentifier
    : identifier errorCapturingIdentifierExtra
    ;

errorCapturingIdentifierExtra
    : (MINUS identifier)+    #errorIdent
    |                        #realIdent
    ;

namedConfigExpressionSeq: (namedConfigExpression (',' namedConfigExpression)*)?;
namedExpressionSeq
    : namedExpression (',' namedExpression)*
    ;

expression
    : valueExpression
    | booleanExpression
    | identifier
    | schema
    ;

booleanExpression
    : NOT booleanExpression                                        #logicalNot
    | EXISTS '(' query ')'                                         #exists
    | valueExpression predicate?                                   #predicated
    | left=booleanExpression op=AND right=booleanExpression  #logicalBinary
    | left=booleanExpression op=OR right=booleanExpression   #logicalBinary
    ;

/// Problem fixed that the querySpecification rule could match an empty string
windowedAggregationClause:
    groupByClause? windowClause watermarkClause?
    | windowClause groupByClause? watermarkClause?;

groupByClause
    : GROUP BY groupingExpressions+=expression (',' groupingExpressions+=expression)* (
      WITH kind=ROLLUP
    | WITH kind=CUBE
    | kind=GROUPING SETS '(' groupingSet (',' groupingSet)* ')')?
    | GROUP BY kind=GROUPING SETS '(' groupingSet (',' groupingSet)* ')'
    ;

groupingSet
    : '(' (expression (',' expression)*)? ')'
    | expression
    ;

windowClause
    : WINDOW windowSpec
    ;

watermarkClause: WATERMARK '(' watermarkParameters ')';

watermarkParameters: watermarkIdentifier=identifier ',' watermark=INTEGER_VALUE watermarkTimeUnit=timeUnit;
/// Adding Threshold Windows
windowSpec:
    timeWindow #timeBasedWindow
    | countWindow #countBasedWindow
    | conditionWindow #thresholdBasedWindow
    ;

timeWindow
    : TUMBLING '(' (timestampParameter ',')?  sizeParameter ')'                       #tumblingWindow
    | SLIDING '(' (timestampParameter ',')? sizeParameter ',' advancebyParameter ')' #slidingWindow
    ;

countWindow:
    TUMBLING '(' INTEGER_VALUE ')'    #countBasedTumbling
    ;

conditionWindow
    : THRESHOLD '(' conditionParameter (',' thresholdMinSizeParameter)? ')' #thresholdWindow
    ;

conditionParameter: expression;
thresholdMinSizeParameter: INTEGER_VALUE;

sizeParameter: SIZE INTEGER_VALUE timeUnit;

advancebyParameter: ADVANCE BY INTEGER_VALUE timeUnit;

timeUnit: MS
        | SEC
        | MINUTE
        | HOUR
        | DAY
        ;

timestampParameter: name=identifier;

functionName:  EDWITHIN_TGEO_GEO
    | TFLOAT_CEIL
    | TFLOAT_EXP
    | TFLOAT_FLOOR
    | TFLOAT_LN
    | TFLOAT_LOG10
    | TFLOAT_RADIANS
    | TFLOAT_TO_TINT
    | TINT_TO_TFLOAT
    | ADD_TFLOAT_FLOAT
    | SUB_TFLOAT_FLOAT
    | MUL_TFLOAT_FLOAT
    | DIV_TFLOAT_FLOAT
    | ADD_TINT_INT
    | SUB_TINT_INT
    | MUL_TINT_INT
    | DIV_TINT_INT
    | ADD_FLOAT_TFLOAT
    | SUB_FLOAT_TFLOAT
    | MUL_FLOAT_TFLOAT
    | DIV_FLOAT_TFLOAT
    | ADD_INT_TINT
    | SUB_INT_TINT
    | MUL_INT_TINT
    | DIV_INT_TINT
    | ADD_TBIGINT_BIGINT
    | SUB_TBIGINT_BIGINT
    | MUL_TBIGINT_BIGINT
    | DIV_TBIGINT_BIGINT
    | ADD_BIGINT_TBIGINT
    | SUB_BIGINT_TBIGINT
    | MUL_BIGINT_TBIGINT
    | DIV_BIGINT_TBIGINT
    | ADD_TNUMBER_TNUMBER
    | SUB_TNUMBER_TNUMBER
    | MUL_TNUMBER_TNUMBER
    | DIV_TNUMBER_TNUMBER
    | TFLOAT_SHIFT_VALUE
    | TFLOAT_SCALE_VALUE
    | TFLOAT_SHIFT_SCALE_VALUE
    | TINT_SHIFT_VALUE
    | TINT_SCALE_VALUE
    | TINT_SHIFT_SCALE_VALUE
    | TBIGINT_SHIFT_VALUE
    | TBIGINT_SCALE_VALUE
    | TBIGINT_SHIFT_SCALE_VALUE
    | TDISTANCE_TFLOAT_FLOAT
    | TDISTANCE_TINT_INT
    | TDISTANCE_TNUMBER_TNUMBER
    | TEMPORAL_ROUND
    | EVER_EQ_TBIGINT_BIGINT
    | EVER_NE_TBIGINT_BIGINT
    | EVER_LT_TBIGINT_BIGINT
    | EVER_LE_TBIGINT_BIGINT
    | EVER_GT_TBIGINT_BIGINT
    | EVER_GE_TBIGINT_BIGINT
    | ALWAYS_EQ_TBIGINT_BIGINT
    | ALWAYS_NE_TBIGINT_BIGINT
    | ALWAYS_LT_TBIGINT_BIGINT
    | ALWAYS_LE_TBIGINT_BIGINT
    | ALWAYS_GT_TBIGINT_BIGINT
    | ALWAYS_GE_TBIGINT_BIGINT
    | EVER_EQ_BIGINT_TBIGINT
    | EVER_NE_BIGINT_TBIGINT
    | EVER_LT_BIGINT_TBIGINT
    | EVER_LE_BIGINT_TBIGINT
    | EVER_GT_BIGINT_TBIGINT
    | EVER_GE_BIGINT_TBIGINT
    | ALWAYS_EQ_BIGINT_TBIGINT
    | ALWAYS_NE_BIGINT_TBIGINT
    | ALWAYS_LT_BIGINT_TBIGINT
    | ALWAYS_LE_BIGINT_TBIGINT
    | ALWAYS_GT_BIGINT_TBIGINT
    | ALWAYS_GE_BIGINT_TBIGINT
    | EVER_EQ_TBOOL_BOOL
    | EVER_NE_TBOOL_BOOL
    | ALWAYS_EQ_TBOOL_BOOL
    | ALWAYS_NE_TBOOL_BOOL
    | EVER_EQ_BOOL_TBOOL
    | EVER_NE_BOOL_TBOOL
    | ALWAYS_EQ_BOOL_TBOOL
    | ALWAYS_NE_BOOL_TBOOL
    | TBIGINT_TO_TINT
    | TBIGINT_TO_TFLOAT
    | TINT_TO_TBIGINT
    | TFLOAT_TO_TBIGINT
    | TBOOL_TO_TINT
    | TNOT_TBOOL
    | TAND_TBOOL_BOOL
    | TOR_TBOOL_BOOL
    | TEQ_TBOOL_BOOL
    | TNE_TBOOL_BOOL
    | TAND_BOOL_TBOOL
    | TOR_BOOL_TBOOL
    | TEQ_BOOL_TBOOL
    | TNE_BOOL_TBOOL
    | TAND_TBOOL_TBOOL
    | TOR_TBOOL_TBOOL
    | ECOVERS_TGEO_GEO
    | EDISJOINT_TGEO_GEO
    | NAD_TGEO_TGEO
    | ECONTAINS_TCBUFFER_TCBUFFER
    | ACONTAINS_TCBUFFER_TCBUFFER
    | EDISJOINT_TCBUFFER_TCBUFFER
    | EDWITHIN_TCBUFFER_CBUFFER
    | ADWITHIN_TCBUFFER_CBUFFER
    | ACONTAINS_TGEO_GEO
    | ACOVERS_TGEO_GEO
    | ADISJOINT_TGEO_GEO
    | AINTERSECTS_TGEO_GEO
    | ATOUCHES_TGEO_GEO
    | ECONTAINS_TGEO_GEO
    | ECOVERS_TGEO_GEO
    | EDISJOINT_TGEO_GEO
    | EINTERSECTS_TGEO_GEO
    | ETOUCHES_TGEO_GEO
    | EVER_EQ_TGEO_GEO
    | EVER_NE_TGEO_GEO
    | ALWAYS_EQ_TGEO_GEO
    | ALWAYS_NE_TGEO_GEO
    | NAD_TGEO_GEO
    | ADWITHIN_TGEO_GEO
    | ACONTAINS_TGEO_TGEO
    | ACOVERS_TGEO_TGEO
    | ADISJOINT_TGEO_TGEO
    | AINTERSECTS_TGEO_TGEO
    | ATOUCHES_TGEO_TGEO
    | ECONTAINS_TGEO_TGEO
    | ECOVERS_TGEO_TGEO
    | EDISJOINT_TGEO_TGEO
    | EINTERSECTS_TGEO_TGEO
    | ETOUCHES_TGEO_TGEO
    | EVER_EQ_TGEO_TGEO
    | EVER_NE_TGEO_TGEO
    | ALWAYS_EQ_TGEO_TGEO
    | ALWAYS_NE_TGEO_TGEO
    | NAD_TGEO_TGEO
    | EDWITHIN_TGEO_TGEO
    | ADWITHIN_TGEO_TGEO
    | ACONTAINS_TCBUFFER_GEO
    | ACOVERS_TCBUFFER_GEO
    | ADISJOINT_TCBUFFER_GEO
    | AINTERSECTS_TCBUFFER_GEO
    | ATOUCHES_TCBUFFER_GEO
    | ECONTAINS_TCBUFFER_GEO
    | ECOVERS_TCBUFFER_GEO
    | EDISJOINT_TCBUFFER_GEO
    | EINTERSECTS_TCBUFFER_GEO
    | ETOUCHES_TCBUFFER_GEO
    | NAD_TCBUFFER_GEO
    | EDWITHIN_TCBUFFER_GEO
    | ADWITHIN_TCBUFFER_GEO
    | ACONTAINS_TCBUFFER_TCBUFFER
    | ACOVERS_TCBUFFER_TCBUFFER
    | ADISJOINT_TCBUFFER_TCBUFFER
    | AINTERSECTS_TCBUFFER_TCBUFFER
    | ATOUCHES_TCBUFFER_TCBUFFER
    | ECONTAINS_TCBUFFER_TCBUFFER
    | ECOVERS_TCBUFFER_TCBUFFER
    | EDISJOINT_TCBUFFER_TCBUFFER
    | EINTERSECTS_TCBUFFER_TCBUFFER
    | ETOUCHES_TCBUFFER_TCBUFFER
    | EVER_EQ_TCBUFFER_TCBUFFER
    | EVER_NE_TCBUFFER_TCBUFFER
    | ALWAYS_EQ_TCBUFFER_TCBUFFER
    | ALWAYS_NE_TCBUFFER_TCBUFFER
    | NAD_TCBUFFER_TCBUFFER
    | EDWITHIN_TCBUFFER_TCBUFFER
    | ADWITHIN_TCBUFFER_TCBUFFER
    | MINDISTANCE_TCBUFFER_TCBUFFER
    | H3INDEX_EQ
    | H3INDEX_NE
    | H3INDEX_LT
    | H3INDEX_LE
    | H3INDEX_GT
    | H3INDEX_GE
    | H3INDEX_CMP
    | H3INDEX_OUT
    | TH3INDEX_GET_RESOLUTION
    | TH3INDEX_GET_BASE_CELL_NUMBER
    | TH3INDEX_IS_VALID_CELL
    | TH3INDEX_IS_PENTAGON
    | TH3INDEX_CELL_TO_PARENT
    | TH3INDEX_CELL_TO_PARENT_NEXT
    | TH3INDEX_CELL_TO_CENTER_CHILD
    | TH3INDEX_CELL_TO_CENTER_CHILD_NEXT
    | TH3INDEX_CELL_TO_CHILD_POS
    | TH3INDEX_ARE_NEIGHBOR_CELLS
    | TH3INDEX_GRID_DISTANCE
    | EVEREQTH3INDEXH3INDEX
    | EVERNETH3INDEXH3INDEX
    | ALWAYSEQTH3INDEXH3INDEX
    | ALWAYSNETH3INDEXH3INDEX
    | QUADBIN_EQ
    | QUADBIN_NE
    | QUADBIN_LT
    | QUADBIN_LE
    | QUADBIN_GT
    | QUADBIN_GE
    | QUADBIN_CMP
    | QUADBIN_GET_RESOLUTION
    | QUADBIN_IS_VALID_CELL
    | QUADBIN_CELL_AREA
    | QUADBIN_CELL_TO_PARENT
    | QUADBIN_CELL_TO_QUADKEY
    | QUADBIN_POINT_TO_CELL
    | QUADBIN_TILE_TO_CELL
    | EVEREQTQUADBINQUADBIN
    | EVERNETQUADBINQUADBIN
    | ALWAYSEQTQUADBINQUADBIN
    | ALWAYSNETQUADBINQUADBIN
    | EVEREQTTEXTTEXT
    | EVEREQTEXTTTEXT
    | EVERNETTEXTTEXT
    | EVERNETEXTTTEXT
    | EVERGETTEXTTEXT
    | EVERGETEXTTTEXT
    | EVERGTTTEXTTEXT
    | EVERGTTEXTTTEXT
    | EVERLETTEXTTEXT
    | EVERLETEXTTTEXT
    | EVERLTTTEXTTEXT
    | EVERLTTEXTTTEXT
    | ALWAYSEQTTEXTTEXT
    | ALWAYSEQTEXTTTEXT
    | ALWAYSNETTEXTTEXT
    | ALWAYSNETEXTTTEXT
    | ALWAYSGETTEXTTEXT
    | ALWAYSGETEXTTTEXT
    | ALWAYSGTTTEXTTEXT
    | ALWAYSGTTEXTTTEXT
    | ALWAYSLETTEXTTEXT
    | ALWAYSLETEXTTTEXT
    | ALWAYSLTTTEXTTEXT
    | ALWAYSLTTEXTTTEXT
    | TEQ_TTEXT_TEXT
    | TEQ_TEXT_TTEXT
    | TNE_TTEXT_TEXT
    | TNE_TEXT_TTEXT
    | TTEXT_UPPER
    | TTEXT_LOWER
    | TTEXT_INITCAP
    | TEXT_UPPER
    | TEXT_LOWER
    | TEXT_INITCAP
    | TEXTCAT_TTEXT_TEXT
    | TEXTCAT_TEXT_TTEXT
    | TEXTCAT_TTEXT_TTEXT
    | TGE_TTEXT_TEXT
    | TGE_TEXT_TTEXT
    | TGT_TTEXT_TEXT
    | TGT_TEXT_TTEXT
    | TLE_TTEXT_TEXT
    | TLE_TEXT_TTEXT
    | TLT_TTEXT_TEXT
    | TLT_TEXT_TTEXT
    | JSONB_EQ
    | JSONB_NE
    | JSONB_LT
    | JSONB_LE
    | JSONB_GT
    | JSONB_GE
    | JSONB_CMP
    | JSONB_CONTAINED
    | JSONB_CONTAINS
    | JSONB_EXISTS
    | JSONB_ARRAY_LENGTH
    | JSONB_TO_CSTRING
    | JSONB_PRETTY
    | JSONB_OBJECT_FIELD_TEXT
    | JSONB_ARRAY_ELEMENT_TEXT
    | EVEREQTJSONBJSONB
    | EVERNETJSONBJSONB
    | ALWAYSEQTJSONBJSONB
    | ALWAYSNETJSONBJSONB
    | EVEREQTJSONBTJSONB
    | EVERNETJSONBTJSONB
    | ALWAYSEQTJSONBTJSONB
    | ALWAYSNETJSONBTJSONB
    | NAD_TNPOINT_GEO
    | NAD_TNPOINT_NPOINT
    | NAD_TNPOINT_TNPOINT
    | EVEREQNPOINTTNPOINT
    | EVEREQTNPOINTNPOINT
    | EVEREQTNPOINTTNPOINT
    | EVERNENPOINTTNPOINT
    | EVERNETNPOINTNPOINT
    | EVERNETNPOINTTNPOINT
    | ALWAYSEQNPOINTTNPOINT
    | ALWAYSEQTNPOINTNPOINT
    | ALWAYSEQTNPOINTTNPOINT
    | ALWAYSNENPOINTTNPOINT
    | ALWAYSNETNPOINTNPOINT
    | ALWAYSNETNPOINTTNPOINT
    | NAD_TPOSE_POSE
    | NAD_TPOSE_TPOSE
    | NAD_TPOSE_GEO
    | EVEREQPOSETPOSE
    | EVEREQTPOSEPOSE
    | EVEREQTPOSETPOSE
    | EVERNEPOSETPOSE
    | EVERNETPOSEPOSE
    | EVERNETPOSETPOSE
    | ALWAYSEQPOSETPOSE
    | ALWAYSEQTPOSEPOSE
    | ALWAYSEQTPOSETPOSE
    | ALWAYSNEPOSETPOSE
    | ALWAYSNETPOSEPOSE
    | ALWAYSNETPOSETPOSE
    | GEOMBOUNDARY
    | GEOMCENTROID
    | GEOMCONVEXHULL
    | GEOREVERSE
    | GEOPOINTS
    | GEOMUNARYUNION
    | GEO_SET_SRID
    | GEO_ROUND
    | GEO_TRANSFORM
    | GEOSRID
    | GEONUMGEOS
    | GEONUMPOINTS
    | GEOMLENGTH
    | GEOMPERIMETER
    | GEOMISEMPTY
    | GEOMAZIMUTH
    | GEOISUNITARY
    | GEOEQUALS
    | GEOSAME
    | GEOMINTERSECTS
    | GEOMINTERSECTS2D
    | GEOMINTERSECTS3D
    | GEOMCONTAINS
    | GEOMCOVERS
    | GEOMDISJOINT2D
    | GEOMTOUCHES
    | GEOGINTERSECTS
    | GEOGDISTANCE
    | LINELOCATEPOINT
    | GEOMDISTANCE2D
    | GEOMDISTANCE3D
    | GEOMDWITHIN2D
    | GEOMDWITHIN3D
    | GEOGDWITHIN
    | GEOMDWITHIN
    | GEOMINTERSECTION2D
    | GEOMINTERSECTION2DCOLL
    | GEOMDIFFERENCE2D
    | GEOMSHORTESTLINE2D
    | GEOMSHORTESTLINE3D
    | LINE_INTERPOLATE_POINT
    | LINE_SUBSTRING
    | LINE_NUMPOINTS
    | LINE_POINT_N
    | GEO_GEO_N
    | GEO_AS_EWKT
    | GEO_AS_GEOJSON
    | GEOM_POINT_MAKE2D
    | GEOM_POINT_MAKE3DZ
    | GEOG_POINT_MAKE2D
    | GEOG_POINT_MAKE3DZ
    | GEOGAREA
    | GEOGLENGTH
    | GEOGPERIMETER
    | GEOGTOGEOM
    | GEOMTOGEOG
    | GEOG_CENTROID
    | INTSPANLOWER
    | INTSPANUPPER
    | INTSPANWIDTH
    | INTSPANLOWERINC
    | INTSPANUPPERINC
    | FLOATSPANLOWER
    | FLOATSPANUPPER
    | FLOATSPANWIDTH
    | FLOATSPANLOWERINC
    | FLOATSPANUPPERINC
    | CONTAINED_INT_SPAN
    | CONTAINED_FLOAT_SPAN
    | CONTAINED_SPAN_SPAN
    | CONTAINED_FLOATSPAN_SPAN
    | CONTAINS_SPAN_INT
    | CONTAINS_SPAN_FLOAT
    | CONTAINS_SPAN_SPAN
    | CONTAINS_FLOATSPAN_SPAN
    | ACONTAINS_GEO_TRGEOMETRY
    | ACOVERS_GEO_TRGEOMETRY
    | ACOVERS_TRGEOMETRY_GEO
    | ADISJOINT_TRGEOMETRY_GEO
    | ADISJOINT_TRGEOMETRY_TRGEOMETRY
    | ADWITHIN_TRGEOMETRY_GEO
    | ADWITHIN_TRGEOMETRY_TRGEOMETRY
    | AINTERSECTS_TRGEOMETRY_GEO
    | AINTERSECTS_TRGEOMETRY_TRGEOMETRY
    | ALWAYS_EQ_GEO_TRGEOMETRY
    | ALWAYS_EQ_TRGEOMETRY_GEO
    | ALWAYS_EQ_TRGEOMETRY_TRGEOMETRY
    | ALWAYS_NE_GEO_TRGEOMETRY
    | ALWAYS_NE_TRGEOMETRY_GEO
    | ALWAYS_NE_TRGEOMETRY_TRGEOMETRY
    | ATOUCHES_TRGEOMETRY_GEO
    | ECONTAINS_GEO_TRGEOMETRY
    | ECOVERS_GEO_TRGEOMETRY
    | ECOVERS_TRGEOMETRY_GEO
    | EDISJOINT_TRGEOMETRY_GEO
    | EDISJOINT_TRGEOMETRY_TRGEOMETRY
    | EDWITHIN_TRGEOMETRY_GEO
    | EDWITHIN_TRGEOMETRY_TRGEOMETRY
    | EINTERSECTS_TRGEOMETRY_GEO
    | EINTERSECTS_TRGEOMETRY_TRGEOMETRY
    | ETOUCHES_TRGEOMETRY_GEO
    | EVER_EQ_GEO_TRGEOMETRY
    | EVER_EQ_TRGEOMETRY_GEO
    | EVER_EQ_TRGEOMETRY_TRGEOMETRY
    | EVER_NE_GEO_TRGEOMETRY
    | EVER_NE_TRGEOMETRY_GEO
    | EVER_NE_TRGEOMETRY_TRGEOMETRY
    | NAD_TRGEOMETRY_GEO
    | NAD_TRGEOMETRY_TRGEOMETRY
    | TEMPORAL_EINTERSECTS_TPOSE_GEOMETRY
    | TEMPORAL_AINTERSECTS_TPOSE_GEOMETRY
    | TEMPORAL_ECOVERS_TPOSE_GEOMETRY
    | TEMPORAL_EDISJOINT_TPOSE_GEOMETRY
    | TEMPORAL_ADISJOINT_TPOSE_GEOMETRY
    | TEMPORAL_ETOUCHES_TPOSE_GEOMETRY
    | TEMPORAL_ATOUCHES_TPOSE_GEOMETRY
    | TEMPORAL_ECONTAINS_TPOSE_GEOMETRY
    | TEMPORAL_ACONTAINS_TPOSE_GEOMETRY
    | TEMPORAL_EDWITHIN_TPOSE_GEOMETRY
    | TEMPORAL_ADWITHIN_TPOSE_GEOMETRY
    | TEMPORAL_EINTERSECTS_TPOSE_TPOSE
    | TEMPORAL_AINTERSECTS_TPOSE_TPOSE
    | TEMPORAL_ECOVERS_TPOSE_TPOSE
    | TEMPORAL_EDISJOINT_TPOSE_TPOSE
    | TEMPORAL_ADISJOINT_TPOSE_TPOSE
    | TEMPORAL_ETOUCHES_TPOSE_TPOSE
    | TEMPORAL_ATOUCHES_TPOSE_TPOSE
    | TEMPORAL_ECONTAINS_TPOSE_TPOSE
    | TEMPORAL_ACONTAINS_TPOSE_TPOSE
    | TEMPORAL_EDWITHIN_TPOSE_TPOSE
    | TEMPORAL_ADWITHIN_TPOSE_TPOSE
    | TEMPORAL_NAD_TPOSE_GEOMETRY
    | TEMPORAL_NAD_TPOSE_TPOSE
    | TEMPORAL_EINTERSECTS_TNPOINT_GEOMETRY
    | TEMPORAL_AINTERSECTS_TNPOINT_GEOMETRY
    | TEMPORAL_ECOVERS_TNPOINT_GEOMETRY
    | TEMPORAL_EDISJOINT_TNPOINT_GEOMETRY
    | TEMPORAL_ADISJOINT_TNPOINT_GEOMETRY
    | TEMPORAL_ETOUCHES_TNPOINT_GEOMETRY
    | TEMPORAL_ATOUCHES_TNPOINT_GEOMETRY
    | TEMPORAL_ECONTAINS_TNPOINT_GEOMETRY
    | TEMPORAL_ACONTAINS_TNPOINT_GEOMETRY
    | TEMPORAL_EDWITHIN_TNPOINT_GEOMETRY
    | TEMPORAL_ADWITHIN_TNPOINT_GEOMETRY
    | TEMPORAL_EINTERSECTS_TNPOINT_TNPOINT
    | TEMPORAL_AINTERSECTS_TNPOINT_TNPOINT
    | TEMPORAL_ECOVERS_TNPOINT_TNPOINT
    | TEMPORAL_EDISJOINT_TNPOINT_TNPOINT
    | TEMPORAL_ADISJOINT_TNPOINT_TNPOINT
    | TEMPORAL_ETOUCHES_TNPOINT_TNPOINT
    | TEMPORAL_ATOUCHES_TNPOINT_TNPOINT
    | TEMPORAL_ECONTAINS_TNPOINT_TNPOINT
    | TEMPORAL_ACONTAINS_TNPOINT_TNPOINT
    | TEMPORAL_EDWITHIN_TNPOINT_TNPOINT
    | TEMPORAL_ADWITHIN_TNPOINT_TNPOINT
    | TEMPORAL_NAD_TNPOINT_GEOMETRY
    | TEMPORAL_NAD_TNPOINT_TNPOINT
    | ALWAYSEQTEMPORALTEMPORAL
    | ALWAYSGETEMPORALTEMPORAL
    | ALWAYSGTTEMPORALTEMPORAL
    | ALWAYSLETEMPORALTEMPORAL
    | ALWAYSLTTEMPORALTEMPORAL
    | ALWAYSNETEMPORALTEMPORAL
    | EVEREQTEMPORALTEMPORAL
    | EVERGETEMPORALTEMPORAL
    | EVERGTTEMPORALTEMPORAL
    | EVERLETEMPORALTEMPORAL
    | EVERLTTEMPORALTEMPORAL
    | EVERNETEMPORALTEMPORAL
    | ALWAYSEQTFLOATFLOAT
    | ALWAYSGETFLOATFLOAT
    | ALWAYSGTTFLOATFLOAT
    | ALWAYSLETFLOATFLOAT
    | ALWAYSLTTFLOATFLOAT
    | ALWAYSNETFLOATFLOAT
    | EVEREQTFLOATFLOAT
    | EVERGETFLOATFLOAT
    | EVERGTTFLOATFLOAT
    | EVERLETFLOATFLOAT
    | EVERLTTFLOATFLOAT
    | EVERNETFLOATFLOAT
    | ALWAYSEQTINTINT
    | ALWAYSGETINTINT
    | ALWAYSGTTINTINT
    | ALWAYSLETINTINT
    | ALWAYSLTTINTINT
    | ALWAYSNETINTINT
    | EVEREQTINTINT
    | EVERGETINTINT
    | EVERGTTINTINT
    | EVERLETINTINT
    | EVERLTTINTINT
    | EVERNETINTINT
    | ALWAYSEQFLOATTFLOAT
    | ALWAYSGEFLOATTFLOAT
    | ALWAYSGTFLOATTFLOAT
    | ALWAYSLEFLOATTFLOAT
    | ALWAYSLTFLOATTFLOAT
    | ALWAYSNEFLOATTFLOAT
    | EVEREQFLOATTFLOAT
    | EVERGEFLOATTFLOAT
    | EVERGTFLOATTFLOAT
    | EVERLEFLOATTFLOAT
    | EVERLTFLOATTFLOAT
    | EVERNEFLOATTFLOAT
    | ALWAYSEQINTTINT
    | ALWAYSGEINTTINT
    | ALWAYSGTINTTINT
    | ALWAYSLEINTTINT
    | ALWAYSLTINTTINT
    | ALWAYSNEINTTINT
    | EVEREQINTTINT
    | EVERGEINTTINT
    | EVERGTINTTINT
    | EVERLEINTTINT
    | EVERLTINTTINT
    | EVERNEINTTINT
    | TEMPORALAINTERSECTSGEOMETRY
    | TEMPORALECONTAINSGEOMETRY
    | TEMPORALINTERSECTSGEOMETRY
    | TEMPORALEDWITHINGEOMETRY
    | ALWAYSEQTCBUFFERCBUFFER
    | ALWAYSNETCBUFFERCBUFFER
    | EVEREQTCBUFFERCBUFFER
    | EVERNETCBUFFERCBUFFER
    | TEMPORAL_AT_STBOX
    | ACONTAINS_TCBUFFER_CBUFFER
    | ACOVERS_TCBUFFER_CBUFFER
    | ADISJOINT_TCBUFFER_CBUFFER
    | AINTERSECTS_TCBUFFER_CBUFFER
    | ATOUCHES_TCBUFFER_CBUFFER
    | ECONTAINS_TCBUFFER_CBUFFER
    | ECOVERS_TCBUFFER_CBUFFER
    | EDISJOINT_TCBUFFER_CBUFFER
    | EINTERSECTS_TCBUFFER_CBUFFER
    | ETOUCHES_TCBUFFER_CBUFFER
    | NAD_TCBUFFER_CBUFFER
    | ALWAYSEQ_TQUADBIN_TQUADBIN
    | ALWAYSNE_TQUADBIN_TQUADBIN
    | EVEREQ_TQUADBIN_TQUADBIN
    | EVERNE_TQUADBIN_TQUADBIN
    | ALWAYSEQ_QUADBIN_TQUADBIN
    | ALWAYSNE_QUADBIN_TQUADBIN
    | EVEREQ_QUADBIN_TQUADBIN
    | EVERNE_QUADBIN_TQUADBIN
    | ACOVERS_GEO_TGEO
    | TFLOAT_COS
    | TFLOAT_SIN
    | TFLOAT_TAN
    | TFLOAT_DEGREES
    | TNUMBER_ABS
    | TEQ_TFLOAT_FLOAT
    | TEQ_FLOAT_TFLOAT
    | TEQ_TINT_INT
    | TEQ_INT_TINT
    | TEQ_TEMPORAL_TEMPORAL
    | TNE_TFLOAT_FLOAT
    | TNE_FLOAT_TFLOAT
    | TNE_TINT_INT
    | TNE_INT_TINT
    | TNE_TEMPORAL_TEMPORAL
    | TLT_TFLOAT_FLOAT
    | TLT_FLOAT_TFLOAT
    | TLT_TINT_INT
    | TLT_INT_TINT
    | TLT_TEMPORAL_TEMPORAL
    | TLE_TFLOAT_FLOAT
    | TLE_FLOAT_TFLOAT
    | TLE_TINT_INT
    | TLE_INT_TINT
    | TLE_TEMPORAL_TEMPORAL
    | TGT_TFLOAT_FLOAT
    | TGT_FLOAT_TFLOAT
    | TGT_TINT_INT
    | TGT_INT_TINT
    | TGT_TEMPORAL_TEMPORAL
    | TGE_TFLOAT_FLOAT
    | TGE_FLOAT_TFLOAT
    | TGE_TINT_INT
    | TGE_INT_TINT
    | TGE_TEMPORAL_TEMPORAL
    | GEO_FROM_GEOJSON
    | GEOM_FROM_HEXEWKB
    | GEO_AS_HEXEWKB
    | GEOM_MIN_BOUNDING_CENTER
    | GEOM_MIN_BOUNDING_RADIUS
    | GEO_TRANSFORM_PIPELINE
    | GEOM_BUFFER
    | GEOM_RELATE_PATTERN
    | H3_GS_POINT_TO_CELL
    | H3INDEX_IN
    | EINTERSECTS_TPCPOINT_GEO
    | NAD_TPCPOINT_GEO
    | JSON_ARRAY_LENGTH
    | JSON_TYPEOF
    | JSON_ARRAY_ELEMENT_TEXT
    | JSON_OBJECT_FIELD_TEXT
    | FLOATSPAN_MAKE
    | INTSPAN_MAKE;

sinkClause: INTO sink (',' sink)*;

sink: identifier | inlineSink;

inlineSink
    : type=identifier '(' parameters=namedConfigExpressionSeq ')'
    ;

nullNotnull
    : NOT? NULLTOKEN
    ;

streamName: IDENTIFIER;

fileFormat: CSV_FORMAT;

sortItem
    : expression ordering=(ASC | DESC)? (NULLS nullOrder=(LAST | FIRST))?
    ;

predicate
    : NOT? kind=BETWEEN lower=valueExpression AND upper=valueExpression
    | NOT? kind=IN '(' expression (',' expression)* ')'
    | NOT? kind=IN '(' query ')'
    | NOT? kind=RLIKE pattern=valueExpression
    | NOT? kind=LIKE quantifier=(ANY | SOME | ALL) ('('')' | '(' expression (',' expression)* ')')
    | NOT? kind=LIKE pattern=valueExpression (ESCAPE escapeChar=STRING)?
    | IS nullNotnull
    | IS NOT? kind=(TRUE | FALSE | UNKNOWN)
    | IS NOT? kind=DISTINCT FROM right=valueExpression
    ;


valueExpression
    : (functionName | typeDefinition) '(' (argument+=expression (',' argument+=expression)*)? ')'                 #functionCall
    | op=(MINUS | PLUS | TILDE) valueExpression                                        #arithmeticUnary
    | left=valueExpression op=(ASTERISK | SLASH | PERCENT | DIV) right=valueExpression #arithmeticBinary
    | left=valueExpression op=(PLUS | MINUS | CONCAT_PIPE) right=valueExpression       #arithmeticBinary
    | left=valueExpression op=AMPERSAND right=valueExpression                          #arithmeticBinary
    | left=valueExpression op=HAT right=valueExpression                                #arithmeticBinary
    | left=valueExpression op=PIPE right=valueExpression                               #arithmeticBinary
    | left=valueExpression comparisonOperator right=valueExpression                          #comparison
    | primaryExpression                                                                      #valueExpressionDefault
    ;

comparisonOperator
    : EQ | NEQ | NEQJ | LT | LTE | GT | GTE | NSEQ
    ;

hint
    : '/*+' hintStatements+=hintStatement (','? hintStatements+=hintStatement)* '*/'
    ;

hintStatement
    : hintName=identifier
    | hintName=identifier '(' parameters+=primaryExpression (',' parameters+=primaryExpression)* ')'
    ;

primaryExpression
    : ASTERISK                                                                                 #star
    | qualifiedName '.' ASTERISK                                                               #star
    | base=primaryExpression '.' fieldName=identifier                                          #dereference
    | '(' query ')'                                                                            #subqueryExpression
    | '(' namedExpression (',' namedExpression)+ ')'                                           #rowConstructor
    | '(' expression ')'                                                                       #parenthesizedExpression
    | constant                                                                                 #constantDefault
    | identifier                                                                               #columnReference
    ;

qualifiedName
    : identifier ('.' identifier)*
    ;

number
    : MINUS? INTEGER_VALUE              #integerLiteral
    | MINUS? FLOAT_LITERAL              #floatLiteral
    ;

unsignedIntegerLiteral: INTEGER_VALUE;

signedIntegerLiteral: MINUS INTEGER_VALUE;

constant
    : NULLTOKEN                                                                                #nullLiteral
    | identifier STRING                                                                        #typeConstructor
    | number                                                                                   #numericLiteral
    | booleanValue                                                                             #booleanLiteral
    | STRING                                                                                  #stringLiteral
    ;

booleanValue
    : TRUE | FALSE
    ;


ALL: 'ALL' | 'all';
AND: 'AND' | 'and';
ANY: 'ANY';
AS: 'AS' | 'as';
ASC: 'ASC' | 'asc';
AT: 'AT';
BETWEEN: 'BETWEEN' | 'between';
BY: 'BY' | 'by';
COMMENT: 'COMMENT';
CUBE: 'CUBE';
DELETE: 'DELETE';
DESC: 'DESC' | 'desc';
DISTINCT: 'DISTINCT';
DIV: 'DIV';
DROP: 'DROP';
ELSE: 'ELSE';
END: 'END';
ESCAPE: 'ESCAPE';
EXISTS: 'EXISTS';
FALSE: 'FALSE';
FIRST: 'FIRST';
FOR: 'FOR';
FROM: 'FROM' | 'from';
FULL: 'FULL';
GROUP: 'GROUP' | 'group';
GROUPING: 'GROUPING';
HAVING: 'HAVING' | 'having';
IF: 'IF';
IN: 'IN' | 'in';
INNER: 'INNER' | 'inner';
INSERT: 'INSERT' | 'insert';
INTO: 'INTO' | 'into';
IS: 'IS'  'is';
JOIN: 'JOIN' | 'join';
LAST: 'LAST';
LEFT: 'LEFT';
LIKE: 'LIKE';
LIMIT: 'LIMIT' | 'limit';
LIST: 'LIST';
MERGE: 'MERGE' | 'merge';
NATURAL: 'NATURAL';
NOT: 'NOT' | 'not' | '!';
NULLTOKEN:'NULL';
NULLS: 'NULLS';
OF: 'OF';
ON: 'ON' | 'on';
OR: 'OR' | 'or';
ORDER: 'ORDER' | 'order';
QUERY: 'QUERY';
RECOVER: 'RECOVER';
RIGHT: 'RIGHT';
RLIKE: 'RLIKE' | 'REGEXP';
ROLLUP: 'ROLLUP';
SCHEMA: 'SCHEMA';
SELECT: 'SELECT' | 'select';
SETS: 'SETS';
SOME: 'SOME';
START: 'START';
TABLE: 'TABLE';
TO: 'TO';
TRUE: 'TRUE';
TYPE: 'TYPE';
UNION: 'UNION' | 'union';
UNKNOWN: 'UNKNOWN';
USE: 'USE';
USING: 'USING';
VALUES: 'VALUES';
WHEN: 'WHEN';
WHERE: 'WHERE' | 'where';
WINDOW: 'WINDOW' | 'window';
WITH: 'WITH';
SET: 'SET';
TUMBLING: 'TUMBLING' | 'tumbling';
SLIDING: 'SLIDING' | 'sliding';
THRESHOLD : 'THRESHOLD'|'threshold';
SIZE: 'SIZE' | 'size';
ADVANCE: 'ADVANCE' | 'advance';
MS: 'MS' | 'ms';
SEC: 'SEC' | 'sec';
MINUTE: 'MINUTE' | 'minute' | 'MINUTES' | 'minutes';
HOUR: 'HOUR' | 'hour' | 'HOURS' | 'hours';
DAY: 'DAY' | 'day' | 'DAYS' | 'days';
MIN: 'MIN' | 'min';
MAX: 'MAX' | 'max';
AVG: 'AVG' | 'avg';
SUM: 'SUM' | 'sum';
COUNT: 'COUNT' | 'count';
MEDIAN: 'MEDIAN' | 'median';
VAR: 'VAR' | 'var';
ARRAY_AGG: 'ARRAY_AGG' | 'array_agg';
TEMPORAL_SEQUENCE: 'TEMPORAL_SEQUENCE' | 'temporal_sequence';
TEMPORAL_EINTERSECTS_GEOMETRY: 'TEMPORAL_EINTERSECTS_GEOMETRY' | 'temporal_eintersects_geometry';
TEMPORAL_AINTERSECTS_GEOMETRY: 'TEMPORAL_AINTERSECTS_GEOMETRY' | 'temporal_aintersects_geometry';
TEMPORAL_ECONTAINS_GEOMETRY: 'TEMPORAL_ECONTAINS_GEOMETRY' | 'temporal_econtains_geometry';
EDWITHIN_TGEO_GEO: 'EDWITHIN_TGEO_GEO' | 'edwithin_tgeo_geo';
TGEO_AT_STBOX: 'TGEO_AT_STBOX' | 'tgeo_at_stbox';
/* BEGIN CODEGEN LEXER TOKENS */
TFLOAT_CEIL: 'TFLOAT_CEIL' | 'tfloat_ceil';
TFLOAT_EXP: 'TFLOAT_EXP' | 'tfloat_exp';
TFLOAT_FLOOR: 'TFLOAT_FLOOR' | 'tfloat_floor';
TFLOAT_LN: 'TFLOAT_LN' | 'tfloat_ln';
TFLOAT_LOG10: 'TFLOAT_LOG10' | 'tfloat_log10';
TFLOAT_RADIANS: 'TFLOAT_RADIANS' | 'tfloat_radians';
TFLOAT_TO_TINT: 'TFLOAT_TO_TINT' | 'tfloat_to_tint';
TINT_TO_TFLOAT: 'TINT_TO_TFLOAT' | 'tint_to_tfloat';
ADD_TFLOAT_FLOAT: 'ADD_TFLOAT_FLOAT' | 'add_tfloat_float';
SUB_TFLOAT_FLOAT: 'SUB_TFLOAT_FLOAT' | 'sub_tfloat_float';
MUL_TFLOAT_FLOAT: 'MUL_TFLOAT_FLOAT' | 'mul_tfloat_float';
DIV_TFLOAT_FLOAT: 'DIV_TFLOAT_FLOAT' | 'div_tfloat_float';
ADD_TINT_INT: 'ADD_TINT_INT' | 'add_tint_int';
SUB_TINT_INT: 'SUB_TINT_INT' | 'sub_tint_int';
MUL_TINT_INT: 'MUL_TINT_INT' | 'mul_tint_int';
DIV_TINT_INT: 'DIV_TINT_INT' | 'div_tint_int';
ADD_FLOAT_TFLOAT: 'ADD_FLOAT_TFLOAT' | 'add_float_tfloat';
SUB_FLOAT_TFLOAT: 'SUB_FLOAT_TFLOAT' | 'sub_float_tfloat';
MUL_FLOAT_TFLOAT: 'MUL_FLOAT_TFLOAT' | 'mul_float_tfloat';
DIV_FLOAT_TFLOAT: 'DIV_FLOAT_TFLOAT' | 'div_float_tfloat';
ADD_INT_TINT: 'ADD_INT_TINT' | 'add_int_tint';
SUB_INT_TINT: 'SUB_INT_TINT' | 'sub_int_tint';
MUL_INT_TINT: 'MUL_INT_TINT' | 'mul_int_tint';
DIV_INT_TINT: 'DIV_INT_TINT' | 'div_int_tint';
ADD_TBIGINT_BIGINT: 'ADD_TBIGINT_BIGINT' | 'add_tbigint_bigint';
SUB_TBIGINT_BIGINT: 'SUB_TBIGINT_BIGINT' | 'sub_tbigint_bigint';
MUL_TBIGINT_BIGINT: 'MUL_TBIGINT_BIGINT' | 'mul_tbigint_bigint';
DIV_TBIGINT_BIGINT: 'DIV_TBIGINT_BIGINT' | 'div_tbigint_bigint';
ADD_BIGINT_TBIGINT: 'ADD_BIGINT_TBIGINT' | 'add_bigint_tbigint';
SUB_BIGINT_TBIGINT: 'SUB_BIGINT_TBIGINT' | 'sub_bigint_tbigint';
MUL_BIGINT_TBIGINT: 'MUL_BIGINT_TBIGINT' | 'mul_bigint_tbigint';
DIV_BIGINT_TBIGINT: 'DIV_BIGINT_TBIGINT' | 'div_bigint_tbigint';
ADD_TNUMBER_TNUMBER: 'ADD_TNUMBER_TNUMBER' | 'add_tnumber_tnumber';
SUB_TNUMBER_TNUMBER: 'SUB_TNUMBER_TNUMBER' | 'sub_tnumber_tnumber';
MUL_TNUMBER_TNUMBER: 'MUL_TNUMBER_TNUMBER' | 'mul_tnumber_tnumber';
DIV_TNUMBER_TNUMBER: 'DIV_TNUMBER_TNUMBER' | 'div_tnumber_tnumber';
TFLOAT_SHIFT_VALUE: 'TFLOAT_SHIFT_VALUE' | 'tfloat_shift_value';
TFLOAT_SCALE_VALUE: 'TFLOAT_SCALE_VALUE' | 'tfloat_scale_value';
TFLOAT_SHIFT_SCALE_VALUE: 'TFLOAT_SHIFT_SCALE_VALUE' | 'tfloat_shift_scale_value';
TINT_SHIFT_VALUE: 'TINT_SHIFT_VALUE' | 'tint_shift_value';
TINT_SCALE_VALUE: 'TINT_SCALE_VALUE' | 'tint_scale_value';
TINT_SHIFT_SCALE_VALUE: 'TINT_SHIFT_SCALE_VALUE' | 'tint_shift_scale_value';
TBIGINT_SHIFT_VALUE: 'TBIGINT_SHIFT_VALUE' | 'tbigint_shift_value';
TBIGINT_SCALE_VALUE: 'TBIGINT_SCALE_VALUE' | 'tbigint_scale_value';
TBIGINT_SHIFT_SCALE_VALUE: 'TBIGINT_SHIFT_SCALE_VALUE' | 'tbigint_shift_scale_value';
TDISTANCE_TFLOAT_FLOAT: 'TDISTANCE_TFLOAT_FLOAT' | 'tdistance_tfloat_float';
TDISTANCE_TINT_INT: 'TDISTANCE_TINT_INT' | 'tdistance_tint_int';
TDISTANCE_TNUMBER_TNUMBER: 'TDISTANCE_TNUMBER_TNUMBER' | 'tdistance_tnumber_tnumber';
TEMPORAL_ROUND: 'TEMPORAL_ROUND' | 'temporal_round';
EVER_EQ_TBIGINT_BIGINT: 'EVER_EQ_TBIGINT_BIGINT' | 'ever_eq_tbigint_bigint';
EVER_NE_TBIGINT_BIGINT: 'EVER_NE_TBIGINT_BIGINT' | 'ever_ne_tbigint_bigint';
EVER_LT_TBIGINT_BIGINT: 'EVER_LT_TBIGINT_BIGINT' | 'ever_lt_tbigint_bigint';
EVER_LE_TBIGINT_BIGINT: 'EVER_LE_TBIGINT_BIGINT' | 'ever_le_tbigint_bigint';
EVER_GT_TBIGINT_BIGINT: 'EVER_GT_TBIGINT_BIGINT' | 'ever_gt_tbigint_bigint';
EVER_GE_TBIGINT_BIGINT: 'EVER_GE_TBIGINT_BIGINT' | 'ever_ge_tbigint_bigint';
ALWAYS_EQ_TBIGINT_BIGINT: 'ALWAYS_EQ_TBIGINT_BIGINT' | 'always_eq_tbigint_bigint';
ALWAYS_NE_TBIGINT_BIGINT: 'ALWAYS_NE_TBIGINT_BIGINT' | 'always_ne_tbigint_bigint';
ALWAYS_LT_TBIGINT_BIGINT: 'ALWAYS_LT_TBIGINT_BIGINT' | 'always_lt_tbigint_bigint';
ALWAYS_LE_TBIGINT_BIGINT: 'ALWAYS_LE_TBIGINT_BIGINT' | 'always_le_tbigint_bigint';
ALWAYS_GT_TBIGINT_BIGINT: 'ALWAYS_GT_TBIGINT_BIGINT' | 'always_gt_tbigint_bigint';
ALWAYS_GE_TBIGINT_BIGINT: 'ALWAYS_GE_TBIGINT_BIGINT' | 'always_ge_tbigint_bigint';
EVER_EQ_BIGINT_TBIGINT: 'EVER_EQ_BIGINT_TBIGINT' | 'ever_eq_bigint_tbigint';
EVER_NE_BIGINT_TBIGINT: 'EVER_NE_BIGINT_TBIGINT' | 'ever_ne_bigint_tbigint';
EVER_LT_BIGINT_TBIGINT: 'EVER_LT_BIGINT_TBIGINT' | 'ever_lt_bigint_tbigint';
EVER_LE_BIGINT_TBIGINT: 'EVER_LE_BIGINT_TBIGINT' | 'ever_le_bigint_tbigint';
EVER_GT_BIGINT_TBIGINT: 'EVER_GT_BIGINT_TBIGINT' | 'ever_gt_bigint_tbigint';
EVER_GE_BIGINT_TBIGINT: 'EVER_GE_BIGINT_TBIGINT' | 'ever_ge_bigint_tbigint';
ALWAYS_EQ_BIGINT_TBIGINT: 'ALWAYS_EQ_BIGINT_TBIGINT' | 'always_eq_bigint_tbigint';
ALWAYS_NE_BIGINT_TBIGINT: 'ALWAYS_NE_BIGINT_TBIGINT' | 'always_ne_bigint_tbigint';
ALWAYS_LT_BIGINT_TBIGINT: 'ALWAYS_LT_BIGINT_TBIGINT' | 'always_lt_bigint_tbigint';
ALWAYS_LE_BIGINT_TBIGINT: 'ALWAYS_LE_BIGINT_TBIGINT' | 'always_le_bigint_tbigint';
ALWAYS_GT_BIGINT_TBIGINT: 'ALWAYS_GT_BIGINT_TBIGINT' | 'always_gt_bigint_tbigint';
ALWAYS_GE_BIGINT_TBIGINT: 'ALWAYS_GE_BIGINT_TBIGINT' | 'always_ge_bigint_tbigint';
EVER_EQ_TBOOL_BOOL: 'EVER_EQ_TBOOL_BOOL' | 'ever_eq_tbool_bool';
EVER_NE_TBOOL_BOOL: 'EVER_NE_TBOOL_BOOL' | 'ever_ne_tbool_bool';
ALWAYS_EQ_TBOOL_BOOL: 'ALWAYS_EQ_TBOOL_BOOL' | 'always_eq_tbool_bool';
ALWAYS_NE_TBOOL_BOOL: 'ALWAYS_NE_TBOOL_BOOL' | 'always_ne_tbool_bool';
EVER_EQ_BOOL_TBOOL: 'EVER_EQ_BOOL_TBOOL' | 'ever_eq_bool_tbool';
EVER_NE_BOOL_TBOOL: 'EVER_NE_BOOL_TBOOL' | 'ever_ne_bool_tbool';
ALWAYS_EQ_BOOL_TBOOL: 'ALWAYS_EQ_BOOL_TBOOL' | 'always_eq_bool_tbool';
ALWAYS_NE_BOOL_TBOOL: 'ALWAYS_NE_BOOL_TBOOL' | 'always_ne_bool_tbool';
TBIGINT_TO_TINT: 'TBIGINT_TO_TINT' | 'tbigint_to_tint';
TBIGINT_TO_TFLOAT: 'TBIGINT_TO_TFLOAT' | 'tbigint_to_tfloat';
TINT_TO_TBIGINT: 'TINT_TO_TBIGINT' | 'tint_to_tbigint';
TFLOAT_TO_TBIGINT: 'TFLOAT_TO_TBIGINT' | 'tfloat_to_tbigint';
TBOOL_TO_TINT: 'TBOOL_TO_TINT' | 'tbool_to_tint';
TNOT_TBOOL: 'TNOT_TBOOL' | 'tnot_tbool';
TAND_TBOOL_BOOL: 'TAND_TBOOL_BOOL' | 'tand_tbool_bool';
TOR_TBOOL_BOOL: 'TOR_TBOOL_BOOL' | 'tor_tbool_bool';
TEQ_TBOOL_BOOL: 'TEQ_TBOOL_BOOL' | 'teq_tbool_bool';
TNE_TBOOL_BOOL: 'TNE_TBOOL_BOOL' | 'tne_tbool_bool';
TAND_BOOL_TBOOL: 'TAND_BOOL_TBOOL' | 'tand_bool_tbool';
TOR_BOOL_TBOOL: 'TOR_BOOL_TBOOL' | 'tor_bool_tbool';
TEQ_BOOL_TBOOL: 'TEQ_BOOL_TBOOL' | 'teq_bool_tbool';
TNE_BOOL_TBOOL: 'TNE_BOOL_TBOOL' | 'tne_bool_tbool';
TAND_TBOOL_TBOOL: 'TAND_TBOOL_TBOOL' | 'tand_tbool_tbool';
TOR_TBOOL_TBOOL: 'TOR_TBOOL_TBOOL' | 'tor_tbool_tbool';
ECOVERS_TGEO_GEO: 'ECOVERS_TGEO_GEO' | 'ecovers_tgeo_geo';
EDISJOINT_TGEO_GEO: 'EDISJOINT_TGEO_GEO' | 'edisjoint_tgeo_geo';
NAD_TGEO_TGEO: 'NAD_TGEO_TGEO' | 'nad_tgeo_tgeo';
ECONTAINS_TCBUFFER_TCBUFFER: 'ECONTAINS_TCBUFFER_TCBUFFER' | 'econtains_tcbuffer_tcbuffer';
ACONTAINS_TCBUFFER_TCBUFFER: 'ACONTAINS_TCBUFFER_TCBUFFER' | 'acontains_tcbuffer_tcbuffer';
EDISJOINT_TCBUFFER_TCBUFFER: 'EDISJOINT_TCBUFFER_TCBUFFER' | 'edisjoint_tcbuffer_tcbuffer';
EDWITHIN_TCBUFFER_CBUFFER: 'EDWITHIN_TCBUFFER_CBUFFER' | 'edwithin_tcbuffer_cbuffer';
ADWITHIN_TCBUFFER_CBUFFER: 'ADWITHIN_TCBUFFER_CBUFFER' | 'adwithin_tcbuffer_cbuffer';
ACONTAINS_TGEO_GEO: 'ACONTAINS_TGEO_GEO' | 'acontains_tgeo_geo';
ACOVERS_TGEO_GEO: 'ACOVERS_TGEO_GEO' | 'acovers_tgeo_geo';
ADISJOINT_TGEO_GEO: 'ADISJOINT_TGEO_GEO' | 'adisjoint_tgeo_geo';
AINTERSECTS_TGEO_GEO: 'AINTERSECTS_TGEO_GEO' | 'aintersects_tgeo_geo';
ATOUCHES_TGEO_GEO: 'ATOUCHES_TGEO_GEO' | 'atouches_tgeo_geo';
ECONTAINS_TGEO_GEO: 'ECONTAINS_TGEO_GEO' | 'econtains_tgeo_geo';
ECOVERS_TGEO_GEO: 'ECOVERS_TGEO_GEO' | 'ecovers_tgeo_geo';
EDISJOINT_TGEO_GEO: 'EDISJOINT_TGEO_GEO' | 'edisjoint_tgeo_geo';
EINTERSECTS_TGEO_GEO: 'EINTERSECTS_TGEO_GEO' | 'eintersects_tgeo_geo';
ETOUCHES_TGEO_GEO: 'ETOUCHES_TGEO_GEO' | 'etouches_tgeo_geo';
EVER_EQ_TGEO_GEO: 'EVER_EQ_TGEO_GEO' | 'ever_eq_tgeo_geo';
EVER_NE_TGEO_GEO: 'EVER_NE_TGEO_GEO' | 'ever_ne_tgeo_geo';
ALWAYS_EQ_TGEO_GEO: 'ALWAYS_EQ_TGEO_GEO' | 'always_eq_tgeo_geo';
ALWAYS_NE_TGEO_GEO: 'ALWAYS_NE_TGEO_GEO' | 'always_ne_tgeo_geo';
NAD_TGEO_GEO: 'NAD_TGEO_GEO' | 'nad_tgeo_geo';
ADWITHIN_TGEO_GEO: 'ADWITHIN_TGEO_GEO' | 'adwithin_tgeo_geo';
ACONTAINS_TGEO_TGEO: 'ACONTAINS_TGEO_TGEO' | 'acontains_tgeo_tgeo';
ACOVERS_TGEO_TGEO: 'ACOVERS_TGEO_TGEO' | 'acovers_tgeo_tgeo';
ADISJOINT_TGEO_TGEO: 'ADISJOINT_TGEO_TGEO' | 'adisjoint_tgeo_tgeo';
AINTERSECTS_TGEO_TGEO: 'AINTERSECTS_TGEO_TGEO' | 'aintersects_tgeo_tgeo';
ATOUCHES_TGEO_TGEO: 'ATOUCHES_TGEO_TGEO' | 'atouches_tgeo_tgeo';
ECONTAINS_TGEO_TGEO: 'ECONTAINS_TGEO_TGEO' | 'econtains_tgeo_tgeo';
ECOVERS_TGEO_TGEO: 'ECOVERS_TGEO_TGEO' | 'ecovers_tgeo_tgeo';
EDISJOINT_TGEO_TGEO: 'EDISJOINT_TGEO_TGEO' | 'edisjoint_tgeo_tgeo';
EINTERSECTS_TGEO_TGEO: 'EINTERSECTS_TGEO_TGEO' | 'eintersects_tgeo_tgeo';
ETOUCHES_TGEO_TGEO: 'ETOUCHES_TGEO_TGEO' | 'etouches_tgeo_tgeo';
EVER_EQ_TGEO_TGEO: 'EVER_EQ_TGEO_TGEO' | 'ever_eq_tgeo_tgeo';
EVER_NE_TGEO_TGEO: 'EVER_NE_TGEO_TGEO' | 'ever_ne_tgeo_tgeo';
ALWAYS_EQ_TGEO_TGEO: 'ALWAYS_EQ_TGEO_TGEO' | 'always_eq_tgeo_tgeo';
ALWAYS_NE_TGEO_TGEO: 'ALWAYS_NE_TGEO_TGEO' | 'always_ne_tgeo_tgeo';
NAD_TGEO_TGEO: 'NAD_TGEO_TGEO' | 'nad_tgeo_tgeo';
EDWITHIN_TGEO_TGEO: 'EDWITHIN_TGEO_TGEO' | 'edwithin_tgeo_tgeo';
ADWITHIN_TGEO_TGEO: 'ADWITHIN_TGEO_TGEO' | 'adwithin_tgeo_tgeo';
ACONTAINS_TCBUFFER_GEO: 'ACONTAINS_TCBUFFER_GEO' | 'acontains_tcbuffer_geo';
ACOVERS_TCBUFFER_GEO: 'ACOVERS_TCBUFFER_GEO' | 'acovers_tcbuffer_geo';
ADISJOINT_TCBUFFER_GEO: 'ADISJOINT_TCBUFFER_GEO' | 'adisjoint_tcbuffer_geo';
AINTERSECTS_TCBUFFER_GEO: 'AINTERSECTS_TCBUFFER_GEO' | 'aintersects_tcbuffer_geo';
ATOUCHES_TCBUFFER_GEO: 'ATOUCHES_TCBUFFER_GEO' | 'atouches_tcbuffer_geo';
ECONTAINS_TCBUFFER_GEO: 'ECONTAINS_TCBUFFER_GEO' | 'econtains_tcbuffer_geo';
ECOVERS_TCBUFFER_GEO: 'ECOVERS_TCBUFFER_GEO' | 'ecovers_tcbuffer_geo';
EDISJOINT_TCBUFFER_GEO: 'EDISJOINT_TCBUFFER_GEO' | 'edisjoint_tcbuffer_geo';
EINTERSECTS_TCBUFFER_GEO: 'EINTERSECTS_TCBUFFER_GEO' | 'eintersects_tcbuffer_geo';
ETOUCHES_TCBUFFER_GEO: 'ETOUCHES_TCBUFFER_GEO' | 'etouches_tcbuffer_geo';
NAD_TCBUFFER_GEO: 'NAD_TCBUFFER_GEO' | 'nad_tcbuffer_geo';
EDWITHIN_TCBUFFER_GEO: 'EDWITHIN_TCBUFFER_GEO' | 'edwithin_tcbuffer_geo';
ADWITHIN_TCBUFFER_GEO: 'ADWITHIN_TCBUFFER_GEO' | 'adwithin_tcbuffer_geo';
ACONTAINS_TCBUFFER_TCBUFFER: 'ACONTAINS_TCBUFFER_TCBUFFER' | 'acontains_tcbuffer_tcbuffer';
ACOVERS_TCBUFFER_TCBUFFER: 'ACOVERS_TCBUFFER_TCBUFFER' | 'acovers_tcbuffer_tcbuffer';
ADISJOINT_TCBUFFER_TCBUFFER: 'ADISJOINT_TCBUFFER_TCBUFFER' | 'adisjoint_tcbuffer_tcbuffer';
AINTERSECTS_TCBUFFER_TCBUFFER: 'AINTERSECTS_TCBUFFER_TCBUFFER' | 'aintersects_tcbuffer_tcbuffer';
ATOUCHES_TCBUFFER_TCBUFFER: 'ATOUCHES_TCBUFFER_TCBUFFER' | 'atouches_tcbuffer_tcbuffer';
ECONTAINS_TCBUFFER_TCBUFFER: 'ECONTAINS_TCBUFFER_TCBUFFER' | 'econtains_tcbuffer_tcbuffer';
ECOVERS_TCBUFFER_TCBUFFER: 'ECOVERS_TCBUFFER_TCBUFFER' | 'ecovers_tcbuffer_tcbuffer';
EDISJOINT_TCBUFFER_TCBUFFER: 'EDISJOINT_TCBUFFER_TCBUFFER' | 'edisjoint_tcbuffer_tcbuffer';
EINTERSECTS_TCBUFFER_TCBUFFER: 'EINTERSECTS_TCBUFFER_TCBUFFER' | 'eintersects_tcbuffer_tcbuffer';
ETOUCHES_TCBUFFER_TCBUFFER: 'ETOUCHES_TCBUFFER_TCBUFFER' | 'etouches_tcbuffer_tcbuffer';
EVER_EQ_TCBUFFER_TCBUFFER: 'EVER_EQ_TCBUFFER_TCBUFFER' | 'ever_eq_tcbuffer_tcbuffer';
EVER_NE_TCBUFFER_TCBUFFER: 'EVER_NE_TCBUFFER_TCBUFFER' | 'ever_ne_tcbuffer_tcbuffer';
ALWAYS_EQ_TCBUFFER_TCBUFFER: 'ALWAYS_EQ_TCBUFFER_TCBUFFER' | 'always_eq_tcbuffer_tcbuffer';
ALWAYS_NE_TCBUFFER_TCBUFFER: 'ALWAYS_NE_TCBUFFER_TCBUFFER' | 'always_ne_tcbuffer_tcbuffer';
NAD_TCBUFFER_TCBUFFER: 'NAD_TCBUFFER_TCBUFFER' | 'nad_tcbuffer_tcbuffer';
EDWITHIN_TCBUFFER_TCBUFFER: 'EDWITHIN_TCBUFFER_TCBUFFER' | 'edwithin_tcbuffer_tcbuffer';
ADWITHIN_TCBUFFER_TCBUFFER: 'ADWITHIN_TCBUFFER_TCBUFFER' | 'adwithin_tcbuffer_tcbuffer';
MINDISTANCE_TCBUFFER_TCBUFFER: 'MINDISTANCE_TCBUFFER_TCBUFFER' | 'mindistance_tcbuffer_tcbuffer';
H3INDEX_EQ: 'H3INDEX_EQ' | 'h3index_eq';
H3INDEX_NE: 'H3INDEX_NE' | 'h3index_ne';
H3INDEX_LT: 'H3INDEX_LT' | 'h3index_lt';
H3INDEX_LE: 'H3INDEX_LE' | 'h3index_le';
H3INDEX_GT: 'H3INDEX_GT' | 'h3index_gt';
H3INDEX_GE: 'H3INDEX_GE' | 'h3index_ge';
H3INDEX_CMP: 'H3INDEX_CMP' | 'h3index_cmp';
H3INDEX_OUT: 'H3INDEX_OUT' | 'h3index_out';
TH3INDEX_GET_RESOLUTION: 'TH3INDEX_GET_RESOLUTION' | 'th3index_get_resolution';
TH3INDEX_GET_BASE_CELL_NUMBER: 'TH3INDEX_GET_BASE_CELL_NUMBER' | 'th3index_get_base_cell_number';
TH3INDEX_IS_VALID_CELL: 'TH3INDEX_IS_VALID_CELL' | 'th3index_is_valid_cell';
TH3INDEX_IS_PENTAGON: 'TH3INDEX_IS_PENTAGON' | 'th3index_is_pentagon';
TH3INDEX_CELL_TO_PARENT: 'TH3INDEX_CELL_TO_PARENT' | 'th3index_cell_to_parent';
TH3INDEX_CELL_TO_PARENT_NEXT: 'TH3INDEX_CELL_TO_PARENT_NEXT' | 'th3index_cell_to_parent_next';
TH3INDEX_CELL_TO_CENTER_CHILD: 'TH3INDEX_CELL_TO_CENTER_CHILD' | 'th3index_cell_to_center_child';
TH3INDEX_CELL_TO_CENTER_CHILD_NEXT: 'TH3INDEX_CELL_TO_CENTER_CHILD_NEXT' | 'th3index_cell_to_center_child_next';
TH3INDEX_CELL_TO_CHILD_POS: 'TH3INDEX_CELL_TO_CHILD_POS' | 'th3index_cell_to_child_pos';
TH3INDEX_ARE_NEIGHBOR_CELLS: 'TH3INDEX_ARE_NEIGHBOR_CELLS' | 'th3index_are_neighbor_cells';
TH3INDEX_GRID_DISTANCE: 'TH3INDEX_GRID_DISTANCE' | 'th3index_grid_distance';
EVEREQTH3INDEXH3INDEX: 'EVEREQTH3INDEXH3INDEX' | 'evereqth3indexh3index';
EVERNETH3INDEXH3INDEX: 'EVERNETH3INDEXH3INDEX' | 'everneth3indexh3index';
ALWAYSEQTH3INDEXH3INDEX: 'ALWAYSEQTH3INDEXH3INDEX' | 'alwayseqth3indexh3index';
ALWAYSNETH3INDEXH3INDEX: 'ALWAYSNETH3INDEXH3INDEX' | 'alwaysneth3indexh3index';
QUADBIN_EQ: 'QUADBIN_EQ' | 'quadbin_eq';
QUADBIN_NE: 'QUADBIN_NE' | 'quadbin_ne';
QUADBIN_LT: 'QUADBIN_LT' | 'quadbin_lt';
QUADBIN_LE: 'QUADBIN_LE' | 'quadbin_le';
QUADBIN_GT: 'QUADBIN_GT' | 'quadbin_gt';
QUADBIN_GE: 'QUADBIN_GE' | 'quadbin_ge';
QUADBIN_CMP: 'QUADBIN_CMP' | 'quadbin_cmp';
QUADBIN_GET_RESOLUTION: 'QUADBIN_GET_RESOLUTION' | 'quadbin_get_resolution';
QUADBIN_IS_VALID_CELL: 'QUADBIN_IS_VALID_CELL' | 'quadbin_is_valid_cell';
QUADBIN_CELL_AREA: 'QUADBIN_CELL_AREA' | 'quadbin_cell_area';
QUADBIN_CELL_TO_PARENT: 'QUADBIN_CELL_TO_PARENT' | 'quadbin_cell_to_parent';
QUADBIN_CELL_TO_QUADKEY: 'QUADBIN_CELL_TO_QUADKEY' | 'quadbin_cell_to_quadkey';
QUADBIN_POINT_TO_CELL: 'QUADBIN_POINT_TO_CELL' | 'quadbin_point_to_cell';
QUADBIN_TILE_TO_CELL: 'QUADBIN_TILE_TO_CELL' | 'quadbin_tile_to_cell';
EVEREQTQUADBINQUADBIN: 'EVEREQTQUADBINQUADBIN' | 'evereqtquadbinquadbin';
EVERNETQUADBINQUADBIN: 'EVERNETQUADBINQUADBIN' | 'evernetquadbinquadbin';
ALWAYSEQTQUADBINQUADBIN: 'ALWAYSEQTQUADBINQUADBIN' | 'alwayseqtquadbinquadbin';
ALWAYSNETQUADBINQUADBIN: 'ALWAYSNETQUADBINQUADBIN' | 'alwaysnetquadbinquadbin';
EVEREQTTEXTTEXT: 'EVEREQTTEXTTEXT' | 'evereqttexttext';
EVEREQTEXTTTEXT: 'EVEREQTEXTTTEXT' | 'evereqtextttext';
EVERNETTEXTTEXT: 'EVERNETTEXTTEXT' | 'evernettexttext';
EVERNETEXTTTEXT: 'EVERNETEXTTTEXT' | 'evernetextttext';
EVERGETTEXTTEXT: 'EVERGETTEXTTEXT' | 'evergettexttext';
EVERGETEXTTTEXT: 'EVERGETEXTTTEXT' | 'evergetextttext';
EVERGTTTEXTTEXT: 'EVERGTTTEXTTEXT' | 'evergtttexttext';
EVERGTTEXTTTEXT: 'EVERGTTEXTTTEXT' | 'evergttextttext';
EVERLETTEXTTEXT: 'EVERLETTEXTTEXT' | 'everlettexttext';
EVERLETEXTTTEXT: 'EVERLETEXTTTEXT' | 'everletextttext';
EVERLTTTEXTTEXT: 'EVERLTTTEXTTEXT' | 'everltttexttext';
EVERLTTEXTTTEXT: 'EVERLTTEXTTTEXT' | 'everlttextttext';
ALWAYSEQTTEXTTEXT: 'ALWAYSEQTTEXTTEXT' | 'alwayseqttexttext';
ALWAYSEQTEXTTTEXT: 'ALWAYSEQTEXTTTEXT' | 'alwayseqtextttext';
ALWAYSNETTEXTTEXT: 'ALWAYSNETTEXTTEXT' | 'alwaysnettexttext';
ALWAYSNETEXTTTEXT: 'ALWAYSNETEXTTTEXT' | 'alwaysnetextttext';
ALWAYSGETTEXTTEXT: 'ALWAYSGETTEXTTEXT' | 'alwaysgettexttext';
ALWAYSGETEXTTTEXT: 'ALWAYSGETEXTTTEXT' | 'alwaysgetextttext';
ALWAYSGTTTEXTTEXT: 'ALWAYSGTTTEXTTEXT' | 'alwaysgtttexttext';
ALWAYSGTTEXTTTEXT: 'ALWAYSGTTEXTTTEXT' | 'alwaysgttextttext';
ALWAYSLETTEXTTEXT: 'ALWAYSLETTEXTTEXT' | 'alwayslettexttext';
ALWAYSLETEXTTTEXT: 'ALWAYSLETEXTTTEXT' | 'alwaysletextttext';
ALWAYSLTTTEXTTEXT: 'ALWAYSLTTTEXTTEXT' | 'alwaysltttexttext';
ALWAYSLTTEXTTTEXT: 'ALWAYSLTTEXTTTEXT' | 'alwayslttextttext';
TEQ_TTEXT_TEXT: 'TEQ_TTEXT_TEXT' | 'teq_ttext_text';
TEQ_TEXT_TTEXT: 'TEQ_TEXT_TTEXT' | 'teq_text_ttext';
TNE_TTEXT_TEXT: 'TNE_TTEXT_TEXT' | 'tne_ttext_text';
TNE_TEXT_TTEXT: 'TNE_TEXT_TTEXT' | 'tne_text_ttext';
TTEXT_UPPER: 'TTEXT_UPPER' | 'ttext_upper';
TTEXT_LOWER: 'TTEXT_LOWER' | 'ttext_lower';
TTEXT_INITCAP: 'TTEXT_INITCAP' | 'ttext_initcap';
TEXT_UPPER: 'TEXT_UPPER' | 'text_upper';
TEXT_LOWER: 'TEXT_LOWER' | 'text_lower';
TEXT_INITCAP: 'TEXT_INITCAP' | 'text_initcap';
TEXTCAT_TTEXT_TEXT: 'TEXTCAT_TTEXT_TEXT' | 'textcat_ttext_text';
TEXTCAT_TEXT_TTEXT: 'TEXTCAT_TEXT_TTEXT' | 'textcat_text_ttext';
TEXTCAT_TTEXT_TTEXT: 'TEXTCAT_TTEXT_TTEXT' | 'textcat_ttext_ttext';
TGE_TTEXT_TEXT: 'TGE_TTEXT_TEXT' | 'tge_ttext_text';
TGE_TEXT_TTEXT: 'TGE_TEXT_TTEXT' | 'tge_text_ttext';
TGT_TTEXT_TEXT: 'TGT_TTEXT_TEXT' | 'tgt_ttext_text';
TGT_TEXT_TTEXT: 'TGT_TEXT_TTEXT' | 'tgt_text_ttext';
TLE_TTEXT_TEXT: 'TLE_TTEXT_TEXT' | 'tle_ttext_text';
TLE_TEXT_TTEXT: 'TLE_TEXT_TTEXT' | 'tle_text_ttext';
TLT_TTEXT_TEXT: 'TLT_TTEXT_TEXT' | 'tlt_ttext_text';
TLT_TEXT_TTEXT: 'TLT_TEXT_TTEXT' | 'tlt_text_ttext';
JSONB_EQ: 'JSONB_EQ' | 'jsonb_eq';
JSONB_NE: 'JSONB_NE' | 'jsonb_ne';
JSONB_LT: 'JSONB_LT' | 'jsonb_lt';
JSONB_LE: 'JSONB_LE' | 'jsonb_le';
JSONB_GT: 'JSONB_GT' | 'jsonb_gt';
JSONB_GE: 'JSONB_GE' | 'jsonb_ge';
JSONB_CMP: 'JSONB_CMP' | 'jsonb_cmp';
JSONB_CONTAINED: 'JSONB_CONTAINED' | 'jsonb_contained';
JSONB_CONTAINS: 'JSONB_CONTAINS' | 'jsonb_contains';
JSONB_EXISTS: 'JSONB_EXISTS' | 'jsonb_exists';
JSONB_ARRAY_LENGTH: 'JSONB_ARRAY_LENGTH' | 'jsonb_array_length';
JSONB_TO_CSTRING: 'JSONB_TO_CSTRING' | 'jsonb_to_cstring';
JSONB_PRETTY: 'JSONB_PRETTY' | 'jsonb_pretty';
JSONB_OBJECT_FIELD_TEXT: 'JSONB_OBJECT_FIELD_TEXT' | 'jsonb_object_field_text';
JSONB_ARRAY_ELEMENT_TEXT: 'JSONB_ARRAY_ELEMENT_TEXT' | 'jsonb_array_element_text';
EVEREQTJSONBJSONB: 'EVEREQTJSONBJSONB' | 'evereqtjsonbjsonb';
EVERNETJSONBJSONB: 'EVERNETJSONBJSONB' | 'evernetjsonbjsonb';
ALWAYSEQTJSONBJSONB: 'ALWAYSEQTJSONBJSONB' | 'alwayseqtjsonbjsonb';
ALWAYSNETJSONBJSONB: 'ALWAYSNETJSONBJSONB' | 'alwaysnetjsonbjsonb';
EVEREQTJSONBTJSONB: 'EVEREQTJSONBTJSONB' | 'evereqtjsonbtjsonb';
EVERNETJSONBTJSONB: 'EVERNETJSONBTJSONB' | 'evernetjsonbtjsonb';
ALWAYSEQTJSONBTJSONB: 'ALWAYSEQTJSONBTJSONB' | 'alwayseqtjsonbtjsonb';
ALWAYSNETJSONBTJSONB: 'ALWAYSNETJSONBTJSONB' | 'alwaysnetjsonbtjsonb';
NAD_TNPOINT_GEO: 'NAD_TNPOINT_GEO' | 'nad_tnpoint_geo';
NAD_TNPOINT_NPOINT: 'NAD_TNPOINT_NPOINT' | 'nad_tnpoint_npoint';
NAD_TNPOINT_TNPOINT: 'NAD_TNPOINT_TNPOINT' | 'nad_tnpoint_tnpoint';
EVEREQNPOINTTNPOINT: 'EVEREQNPOINTTNPOINT' | 'evereqnpointtnpoint';
EVEREQTNPOINTNPOINT: 'EVEREQTNPOINTNPOINT' | 'evereqtnpointnpoint';
EVEREQTNPOINTTNPOINT: 'EVEREQTNPOINTTNPOINT' | 'evereqtnpointtnpoint';
EVERNENPOINTTNPOINT: 'EVERNENPOINTTNPOINT' | 'evernenpointtnpoint';
EVERNETNPOINTNPOINT: 'EVERNETNPOINTNPOINT' | 'evernetnpointnpoint';
EVERNETNPOINTTNPOINT: 'EVERNETNPOINTTNPOINT' | 'evernetnpointtnpoint';
ALWAYSEQNPOINTTNPOINT: 'ALWAYSEQNPOINTTNPOINT' | 'alwayseqnpointtnpoint';
ALWAYSEQTNPOINTNPOINT: 'ALWAYSEQTNPOINTNPOINT' | 'alwayseqtnpointnpoint';
ALWAYSEQTNPOINTTNPOINT: 'ALWAYSEQTNPOINTTNPOINT' | 'alwayseqtnpointtnpoint';
ALWAYSNENPOINTTNPOINT: 'ALWAYSNENPOINTTNPOINT' | 'alwaysnenpointtnpoint';
ALWAYSNETNPOINTNPOINT: 'ALWAYSNETNPOINTNPOINT' | 'alwaysnetnpointnpoint';
ALWAYSNETNPOINTTNPOINT: 'ALWAYSNETNPOINTTNPOINT' | 'alwaysnetnpointtnpoint';
NAD_TPOSE_POSE: 'NAD_TPOSE_POSE' | 'nad_tpose_pose';
NAD_TPOSE_TPOSE: 'NAD_TPOSE_TPOSE' | 'nad_tpose_tpose';
NAD_TPOSE_GEO: 'NAD_TPOSE_GEO' | 'nad_tpose_geo';
EVEREQPOSETPOSE: 'EVEREQPOSETPOSE' | 'evereqposetpose';
EVEREQTPOSEPOSE: 'EVEREQTPOSEPOSE' | 'evereqtposepose';
EVEREQTPOSETPOSE: 'EVEREQTPOSETPOSE' | 'evereqtposetpose';
EVERNEPOSETPOSE: 'EVERNEPOSETPOSE' | 'everneposetpose';
EVERNETPOSEPOSE: 'EVERNETPOSEPOSE' | 'evernetposepose';
EVERNETPOSETPOSE: 'EVERNETPOSETPOSE' | 'evernetposetpose';
ALWAYSEQPOSETPOSE: 'ALWAYSEQPOSETPOSE' | 'alwayseqposetpose';
ALWAYSEQTPOSEPOSE: 'ALWAYSEQTPOSEPOSE' | 'alwayseqtposepose';
ALWAYSEQTPOSETPOSE: 'ALWAYSEQTPOSETPOSE' | 'alwayseqtposetpose';
ALWAYSNEPOSETPOSE: 'ALWAYSNEPOSETPOSE' | 'alwaysneposetpose';
ALWAYSNETPOSEPOSE: 'ALWAYSNETPOSEPOSE' | 'alwaysnetposepose';
ALWAYSNETPOSETPOSE: 'ALWAYSNETPOSETPOSE' | 'alwaysnetposetpose';
GEOMBOUNDARY: 'GEOMBOUNDARY' | 'geomboundary';
GEOMCENTROID: 'GEOMCENTROID' | 'geomcentroid';
GEOMCONVEXHULL: 'GEOMCONVEXHULL' | 'geomconvexhull';
GEOREVERSE: 'GEOREVERSE' | 'georeverse';
GEOPOINTS: 'GEOPOINTS' | 'geopoints';
GEOMUNARYUNION: 'GEOMUNARYUNION' | 'geomunaryunion';
GEO_SET_SRID: 'GEO_SET_SRID' | 'geo_set_srid';
GEO_ROUND: 'GEO_ROUND' | 'geo_round';
GEO_TRANSFORM: 'GEO_TRANSFORM' | 'geo_transform';
GEOSRID: 'GEOSRID' | 'geosrid';
GEONUMGEOS: 'GEONUMGEOS' | 'geonumgeos';
GEONUMPOINTS: 'GEONUMPOINTS' | 'geonumpoints';
GEOMLENGTH: 'GEOMLENGTH' | 'geomlength';
GEOMPERIMETER: 'GEOMPERIMETER' | 'geomperimeter';
GEOMISEMPTY: 'GEOMISEMPTY' | 'geomisempty';
GEOMAZIMUTH: 'GEOMAZIMUTH' | 'geomazimuth';
GEOISUNITARY: 'GEOISUNITARY' | 'geoisunitary';
GEOEQUALS: 'GEOEQUALS' | 'geoequals';
GEOSAME: 'GEOSAME' | 'geosame';
GEOMINTERSECTS: 'GEOMINTERSECTS' | 'geomintersects';
GEOMINTERSECTS2D: 'GEOMINTERSECTS2D' | 'geomintersects2d';
GEOMINTERSECTS3D: 'GEOMINTERSECTS3D' | 'geomintersects3d';
GEOMCONTAINS: 'GEOMCONTAINS' | 'geomcontains';
GEOMCOVERS: 'GEOMCOVERS' | 'geomcovers';
GEOMDISJOINT2D: 'GEOMDISJOINT2D' | 'geomdisjoint2d';
GEOMTOUCHES: 'GEOMTOUCHES' | 'geomtouches';
GEOGINTERSECTS: 'GEOGINTERSECTS' | 'geogintersects';
GEOGDISTANCE: 'GEOGDISTANCE' | 'geogdistance';
LINELOCATEPOINT: 'LINELOCATEPOINT' | 'linelocatepoint';
GEOMDISTANCE2D: 'GEOMDISTANCE2D' | 'geomdistance2d';
GEOMDISTANCE3D: 'GEOMDISTANCE3D' | 'geomdistance3d';
GEOMDWITHIN2D: 'GEOMDWITHIN2D' | 'geomdwithin2d';
GEOMDWITHIN3D: 'GEOMDWITHIN3D' | 'geomdwithin3d';
GEOGDWITHIN: 'GEOGDWITHIN' | 'geogdwithin';
GEOMDWITHIN: 'GEOMDWITHIN' | 'geomdwithin';
GEOMINTERSECTION2D: 'GEOMINTERSECTION2D' | 'geomintersection2d';
GEOMINTERSECTION2DCOLL: 'GEOMINTERSECTION2DCOLL' | 'geomintersection2dcoll';
GEOMDIFFERENCE2D: 'GEOMDIFFERENCE2D' | 'geomdifference2d';
GEOMSHORTESTLINE2D: 'GEOMSHORTESTLINE2D' | 'geomshortestline2d';
GEOMSHORTESTLINE3D: 'GEOMSHORTESTLINE3D' | 'geomshortestline3d';
LINE_INTERPOLATE_POINT: 'LINE_INTERPOLATE_POINT' | 'line_interpolate_point';
LINE_SUBSTRING: 'LINE_SUBSTRING' | 'line_substring';
LINE_NUMPOINTS: 'LINE_NUMPOINTS' | 'line_numpoints';
LINE_POINT_N: 'LINE_POINT_N' | 'line_point_n';
GEO_GEO_N: 'GEO_GEO_N' | 'geo_geo_n';
GEO_AS_EWKT: 'GEO_AS_EWKT' | 'geo_as_ewkt';
GEO_AS_GEOJSON: 'GEO_AS_GEOJSON' | 'geo_as_geojson';
GEOM_POINT_MAKE2D: 'GEOM_POINT_MAKE2D' | 'geom_point_make2d';
GEOM_POINT_MAKE3DZ: 'GEOM_POINT_MAKE3DZ' | 'geom_point_make3dz';
GEOG_POINT_MAKE2D: 'GEOG_POINT_MAKE2D' | 'geog_point_make2d';
GEOG_POINT_MAKE3DZ: 'GEOG_POINT_MAKE3DZ' | 'geog_point_make3dz';
GEOGAREA: 'GEOGAREA' | 'geogarea';
GEOGLENGTH: 'GEOGLENGTH' | 'geoglength';
GEOGPERIMETER: 'GEOGPERIMETER' | 'geogperimeter';
GEOGTOGEOM: 'GEOGTOGEOM' | 'geogtogeom';
GEOMTOGEOG: 'GEOMTOGEOG' | 'geomtogeog';
GEOG_CENTROID: 'GEOG_CENTROID' | 'geog_centroid';
INTSPANLOWER: 'INTSPANLOWER' | 'intspanlower';
INTSPANUPPER: 'INTSPANUPPER' | 'intspanupper';
INTSPANWIDTH: 'INTSPANWIDTH' | 'intspanwidth';
INTSPANLOWERINC: 'INTSPANLOWERINC' | 'intspanlowerinc';
INTSPANUPPERINC: 'INTSPANUPPERINC' | 'intspanupperinc';
FLOATSPANLOWER: 'FLOATSPANLOWER' | 'floatspanlower';
FLOATSPANUPPER: 'FLOATSPANUPPER' | 'floatspanupper';
FLOATSPANWIDTH: 'FLOATSPANWIDTH' | 'floatspanwidth';
FLOATSPANLOWERINC: 'FLOATSPANLOWERINC' | 'floatspanlowerinc';
FLOATSPANUPPERINC: 'FLOATSPANUPPERINC' | 'floatspanupperinc';
CONTAINED_INT_SPAN: 'CONTAINED_INT_SPAN' | 'contained_int_span';
CONTAINED_FLOAT_SPAN: 'CONTAINED_FLOAT_SPAN' | 'contained_float_span';
CONTAINED_SPAN_SPAN: 'CONTAINED_SPAN_SPAN' | 'contained_span_span';
CONTAINED_FLOATSPAN_SPAN: 'CONTAINED_FLOATSPAN_SPAN' | 'contained_floatspan_span';
CONTAINS_SPAN_INT: 'CONTAINS_SPAN_INT' | 'contains_span_int';
CONTAINS_SPAN_FLOAT: 'CONTAINS_SPAN_FLOAT' | 'contains_span_float';
CONTAINS_SPAN_SPAN: 'CONTAINS_SPAN_SPAN' | 'contains_span_span';
CONTAINS_FLOATSPAN_SPAN: 'CONTAINS_FLOATSPAN_SPAN' | 'contains_floatspan_span';
ACONTAINS_GEO_TRGEOMETRY: 'ACONTAINS_GEO_TRGEOMETRY' | 'acontains_geo_trgeometry';
ACOVERS_GEO_TRGEOMETRY: 'ACOVERS_GEO_TRGEOMETRY' | 'acovers_geo_trgeometry';
ACOVERS_TRGEOMETRY_GEO: 'ACOVERS_TRGEOMETRY_GEO' | 'acovers_trgeometry_geo';
ADISJOINT_TRGEOMETRY_GEO: 'ADISJOINT_TRGEOMETRY_GEO' | 'adisjoint_trgeometry_geo';
ADISJOINT_TRGEOMETRY_TRGEOMETRY: 'ADISJOINT_TRGEOMETRY_TRGEOMETRY' | 'adisjoint_trgeometry_trgeometry';
ADWITHIN_TRGEOMETRY_GEO: 'ADWITHIN_TRGEOMETRY_GEO' | 'adwithin_trgeometry_geo';
ADWITHIN_TRGEOMETRY_TRGEOMETRY: 'ADWITHIN_TRGEOMETRY_TRGEOMETRY' | 'adwithin_trgeometry_trgeometry';
AINTERSECTS_TRGEOMETRY_GEO: 'AINTERSECTS_TRGEOMETRY_GEO' | 'aintersects_trgeometry_geo';
AINTERSECTS_TRGEOMETRY_TRGEOMETRY: 'AINTERSECTS_TRGEOMETRY_TRGEOMETRY' | 'aintersects_trgeometry_trgeometry';
ALWAYS_EQ_GEO_TRGEOMETRY: 'ALWAYS_EQ_GEO_TRGEOMETRY' | 'always_eq_geo_trgeometry';
ALWAYS_EQ_TRGEOMETRY_GEO: 'ALWAYS_EQ_TRGEOMETRY_GEO' | 'always_eq_trgeometry_geo';
ALWAYS_EQ_TRGEOMETRY_TRGEOMETRY: 'ALWAYS_EQ_TRGEOMETRY_TRGEOMETRY' | 'always_eq_trgeometry_trgeometry';
ALWAYS_NE_GEO_TRGEOMETRY: 'ALWAYS_NE_GEO_TRGEOMETRY' | 'always_ne_geo_trgeometry';
ALWAYS_NE_TRGEOMETRY_GEO: 'ALWAYS_NE_TRGEOMETRY_GEO' | 'always_ne_trgeometry_geo';
ALWAYS_NE_TRGEOMETRY_TRGEOMETRY: 'ALWAYS_NE_TRGEOMETRY_TRGEOMETRY' | 'always_ne_trgeometry_trgeometry';
ATOUCHES_TRGEOMETRY_GEO: 'ATOUCHES_TRGEOMETRY_GEO' | 'atouches_trgeometry_geo';
ECONTAINS_GEO_TRGEOMETRY: 'ECONTAINS_GEO_TRGEOMETRY' | 'econtains_geo_trgeometry';
ECOVERS_GEO_TRGEOMETRY: 'ECOVERS_GEO_TRGEOMETRY' | 'ecovers_geo_trgeometry';
ECOVERS_TRGEOMETRY_GEO: 'ECOVERS_TRGEOMETRY_GEO' | 'ecovers_trgeometry_geo';
EDISJOINT_TRGEOMETRY_GEO: 'EDISJOINT_TRGEOMETRY_GEO' | 'edisjoint_trgeometry_geo';
EDISJOINT_TRGEOMETRY_TRGEOMETRY: 'EDISJOINT_TRGEOMETRY_TRGEOMETRY' | 'edisjoint_trgeometry_trgeometry';
EDWITHIN_TRGEOMETRY_GEO: 'EDWITHIN_TRGEOMETRY_GEO' | 'edwithin_trgeometry_geo';
EDWITHIN_TRGEOMETRY_TRGEOMETRY: 'EDWITHIN_TRGEOMETRY_TRGEOMETRY' | 'edwithin_trgeometry_trgeometry';
EINTERSECTS_TRGEOMETRY_GEO: 'EINTERSECTS_TRGEOMETRY_GEO' | 'eintersects_trgeometry_geo';
EINTERSECTS_TRGEOMETRY_TRGEOMETRY: 'EINTERSECTS_TRGEOMETRY_TRGEOMETRY' | 'eintersects_trgeometry_trgeometry';
ETOUCHES_TRGEOMETRY_GEO: 'ETOUCHES_TRGEOMETRY_GEO' | 'etouches_trgeometry_geo';
EVER_EQ_GEO_TRGEOMETRY: 'EVER_EQ_GEO_TRGEOMETRY' | 'ever_eq_geo_trgeometry';
EVER_EQ_TRGEOMETRY_GEO: 'EVER_EQ_TRGEOMETRY_GEO' | 'ever_eq_trgeometry_geo';
EVER_EQ_TRGEOMETRY_TRGEOMETRY: 'EVER_EQ_TRGEOMETRY_TRGEOMETRY' | 'ever_eq_trgeometry_trgeometry';
EVER_NE_GEO_TRGEOMETRY: 'EVER_NE_GEO_TRGEOMETRY' | 'ever_ne_geo_trgeometry';
EVER_NE_TRGEOMETRY_GEO: 'EVER_NE_TRGEOMETRY_GEO' | 'ever_ne_trgeometry_geo';
EVER_NE_TRGEOMETRY_TRGEOMETRY: 'EVER_NE_TRGEOMETRY_TRGEOMETRY' | 'ever_ne_trgeometry_trgeometry';
NAD_TRGEOMETRY_GEO: 'NAD_TRGEOMETRY_GEO' | 'nad_trgeometry_geo';
NAD_TRGEOMETRY_TRGEOMETRY: 'NAD_TRGEOMETRY_TRGEOMETRY' | 'nad_trgeometry_trgeometry';
TEMPORAL_EINTERSECTS_TPOSE_GEOMETRY: 'TEMPORAL_EINTERSECTS_TPOSE_GEOMETRY' | 'temporal_eintersects_tpose_geometry';
TEMPORAL_AINTERSECTS_TPOSE_GEOMETRY: 'TEMPORAL_AINTERSECTS_TPOSE_GEOMETRY' | 'temporal_aintersects_tpose_geometry';
TEMPORAL_ECOVERS_TPOSE_GEOMETRY: 'TEMPORAL_ECOVERS_TPOSE_GEOMETRY' | 'temporal_ecovers_tpose_geometry';
TEMPORAL_EDISJOINT_TPOSE_GEOMETRY: 'TEMPORAL_EDISJOINT_TPOSE_GEOMETRY' | 'temporal_edisjoint_tpose_geometry';
TEMPORAL_ADISJOINT_TPOSE_GEOMETRY: 'TEMPORAL_ADISJOINT_TPOSE_GEOMETRY' | 'temporal_adisjoint_tpose_geometry';
TEMPORAL_ETOUCHES_TPOSE_GEOMETRY: 'TEMPORAL_ETOUCHES_TPOSE_GEOMETRY' | 'temporal_etouches_tpose_geometry';
TEMPORAL_ATOUCHES_TPOSE_GEOMETRY: 'TEMPORAL_ATOUCHES_TPOSE_GEOMETRY' | 'temporal_atouches_tpose_geometry';
TEMPORAL_ECONTAINS_TPOSE_GEOMETRY: 'TEMPORAL_ECONTAINS_TPOSE_GEOMETRY' | 'temporal_econtains_tpose_geometry';
TEMPORAL_ACONTAINS_TPOSE_GEOMETRY: 'TEMPORAL_ACONTAINS_TPOSE_GEOMETRY' | 'temporal_acontains_tpose_geometry';
TEMPORAL_EDWITHIN_TPOSE_GEOMETRY: 'TEMPORAL_EDWITHIN_TPOSE_GEOMETRY' | 'temporal_edwithin_tpose_geometry';
TEMPORAL_ADWITHIN_TPOSE_GEOMETRY: 'TEMPORAL_ADWITHIN_TPOSE_GEOMETRY' | 'temporal_adwithin_tpose_geometry';
TEMPORAL_EINTERSECTS_TPOSE_TPOSE: 'TEMPORAL_EINTERSECTS_TPOSE_TPOSE' | 'temporal_eintersects_tpose_tpose';
TEMPORAL_AINTERSECTS_TPOSE_TPOSE: 'TEMPORAL_AINTERSECTS_TPOSE_TPOSE' | 'temporal_aintersects_tpose_tpose';
TEMPORAL_ECOVERS_TPOSE_TPOSE: 'TEMPORAL_ECOVERS_TPOSE_TPOSE' | 'temporal_ecovers_tpose_tpose';
TEMPORAL_EDISJOINT_TPOSE_TPOSE: 'TEMPORAL_EDISJOINT_TPOSE_TPOSE' | 'temporal_edisjoint_tpose_tpose';
TEMPORAL_ADISJOINT_TPOSE_TPOSE: 'TEMPORAL_ADISJOINT_TPOSE_TPOSE' | 'temporal_adisjoint_tpose_tpose';
TEMPORAL_ETOUCHES_TPOSE_TPOSE: 'TEMPORAL_ETOUCHES_TPOSE_TPOSE' | 'temporal_etouches_tpose_tpose';
TEMPORAL_ATOUCHES_TPOSE_TPOSE: 'TEMPORAL_ATOUCHES_TPOSE_TPOSE' | 'temporal_atouches_tpose_tpose';
TEMPORAL_ECONTAINS_TPOSE_TPOSE: 'TEMPORAL_ECONTAINS_TPOSE_TPOSE' | 'temporal_econtains_tpose_tpose';
TEMPORAL_ACONTAINS_TPOSE_TPOSE: 'TEMPORAL_ACONTAINS_TPOSE_TPOSE' | 'temporal_acontains_tpose_tpose';
TEMPORAL_EDWITHIN_TPOSE_TPOSE: 'TEMPORAL_EDWITHIN_TPOSE_TPOSE' | 'temporal_edwithin_tpose_tpose';
TEMPORAL_ADWITHIN_TPOSE_TPOSE: 'TEMPORAL_ADWITHIN_TPOSE_TPOSE' | 'temporal_adwithin_tpose_tpose';
TEMPORAL_NAD_TPOSE_GEOMETRY: 'TEMPORAL_NAD_TPOSE_GEOMETRY' | 'temporal_nad_tpose_geometry';
TEMPORAL_NAD_TPOSE_TPOSE: 'TEMPORAL_NAD_TPOSE_TPOSE' | 'temporal_nad_tpose_tpose';
TEMPORAL_EINTERSECTS_TNPOINT_GEOMETRY: 'TEMPORAL_EINTERSECTS_TNPOINT_GEOMETRY' | 'temporal_eintersects_tnpoint_geometry';
TEMPORAL_AINTERSECTS_TNPOINT_GEOMETRY: 'TEMPORAL_AINTERSECTS_TNPOINT_GEOMETRY' | 'temporal_aintersects_tnpoint_geometry';
TEMPORAL_ECOVERS_TNPOINT_GEOMETRY: 'TEMPORAL_ECOVERS_TNPOINT_GEOMETRY' | 'temporal_ecovers_tnpoint_geometry';
TEMPORAL_EDISJOINT_TNPOINT_GEOMETRY: 'TEMPORAL_EDISJOINT_TNPOINT_GEOMETRY' | 'temporal_edisjoint_tnpoint_geometry';
TEMPORAL_ADISJOINT_TNPOINT_GEOMETRY: 'TEMPORAL_ADISJOINT_TNPOINT_GEOMETRY' | 'temporal_adisjoint_tnpoint_geometry';
TEMPORAL_ETOUCHES_TNPOINT_GEOMETRY: 'TEMPORAL_ETOUCHES_TNPOINT_GEOMETRY' | 'temporal_etouches_tnpoint_geometry';
TEMPORAL_ATOUCHES_TNPOINT_GEOMETRY: 'TEMPORAL_ATOUCHES_TNPOINT_GEOMETRY' | 'temporal_atouches_tnpoint_geometry';
TEMPORAL_ECONTAINS_TNPOINT_GEOMETRY: 'TEMPORAL_ECONTAINS_TNPOINT_GEOMETRY' | 'temporal_econtains_tnpoint_geometry';
TEMPORAL_ACONTAINS_TNPOINT_GEOMETRY: 'TEMPORAL_ACONTAINS_TNPOINT_GEOMETRY' | 'temporal_acontains_tnpoint_geometry';
TEMPORAL_EDWITHIN_TNPOINT_GEOMETRY: 'TEMPORAL_EDWITHIN_TNPOINT_GEOMETRY' | 'temporal_edwithin_tnpoint_geometry';
TEMPORAL_ADWITHIN_TNPOINT_GEOMETRY: 'TEMPORAL_ADWITHIN_TNPOINT_GEOMETRY' | 'temporal_adwithin_tnpoint_geometry';
TEMPORAL_EINTERSECTS_TNPOINT_TNPOINT: 'TEMPORAL_EINTERSECTS_TNPOINT_TNPOINT' | 'temporal_eintersects_tnpoint_tnpoint';
TEMPORAL_AINTERSECTS_TNPOINT_TNPOINT: 'TEMPORAL_AINTERSECTS_TNPOINT_TNPOINT' | 'temporal_aintersects_tnpoint_tnpoint';
TEMPORAL_ECOVERS_TNPOINT_TNPOINT: 'TEMPORAL_ECOVERS_TNPOINT_TNPOINT' | 'temporal_ecovers_tnpoint_tnpoint';
TEMPORAL_EDISJOINT_TNPOINT_TNPOINT: 'TEMPORAL_EDISJOINT_TNPOINT_TNPOINT' | 'temporal_edisjoint_tnpoint_tnpoint';
TEMPORAL_ADISJOINT_TNPOINT_TNPOINT: 'TEMPORAL_ADISJOINT_TNPOINT_TNPOINT' | 'temporal_adisjoint_tnpoint_tnpoint';
TEMPORAL_ETOUCHES_TNPOINT_TNPOINT: 'TEMPORAL_ETOUCHES_TNPOINT_TNPOINT' | 'temporal_etouches_tnpoint_tnpoint';
TEMPORAL_ATOUCHES_TNPOINT_TNPOINT: 'TEMPORAL_ATOUCHES_TNPOINT_TNPOINT' | 'temporal_atouches_tnpoint_tnpoint';
TEMPORAL_ECONTAINS_TNPOINT_TNPOINT: 'TEMPORAL_ECONTAINS_TNPOINT_TNPOINT' | 'temporal_econtains_tnpoint_tnpoint';
TEMPORAL_ACONTAINS_TNPOINT_TNPOINT: 'TEMPORAL_ACONTAINS_TNPOINT_TNPOINT' | 'temporal_acontains_tnpoint_tnpoint';
TEMPORAL_EDWITHIN_TNPOINT_TNPOINT: 'TEMPORAL_EDWITHIN_TNPOINT_TNPOINT' | 'temporal_edwithin_tnpoint_tnpoint';
TEMPORAL_ADWITHIN_TNPOINT_TNPOINT: 'TEMPORAL_ADWITHIN_TNPOINT_TNPOINT' | 'temporal_adwithin_tnpoint_tnpoint';
TEMPORAL_NAD_TNPOINT_GEOMETRY: 'TEMPORAL_NAD_TNPOINT_GEOMETRY' | 'temporal_nad_tnpoint_geometry';
TEMPORAL_NAD_TNPOINT_TNPOINT: 'TEMPORAL_NAD_TNPOINT_TNPOINT' | 'temporal_nad_tnpoint_tnpoint';
ALWAYSEQTEMPORALTEMPORAL: 'ALWAYSEQTEMPORALTEMPORAL' | 'alwayseqtemporaltemporal';
ALWAYSGETEMPORALTEMPORAL: 'ALWAYSGETEMPORALTEMPORAL' | 'alwaysgetemporaltemporal';
ALWAYSGTTEMPORALTEMPORAL: 'ALWAYSGTTEMPORALTEMPORAL' | 'alwaysgttemporaltemporal';
ALWAYSLETEMPORALTEMPORAL: 'ALWAYSLETEMPORALTEMPORAL' | 'alwaysletemporaltemporal';
ALWAYSLTTEMPORALTEMPORAL: 'ALWAYSLTTEMPORALTEMPORAL' | 'alwayslttemporaltemporal';
ALWAYSNETEMPORALTEMPORAL: 'ALWAYSNETEMPORALTEMPORAL' | 'alwaysnetemporaltemporal';
EVEREQTEMPORALTEMPORAL: 'EVEREQTEMPORALTEMPORAL' | 'evereqtemporaltemporal';
EVERGETEMPORALTEMPORAL: 'EVERGETEMPORALTEMPORAL' | 'evergetemporaltemporal';
EVERGTTEMPORALTEMPORAL: 'EVERGTTEMPORALTEMPORAL' | 'evergttemporaltemporal';
EVERLETEMPORALTEMPORAL: 'EVERLETEMPORALTEMPORAL' | 'everletemporaltemporal';
EVERLTTEMPORALTEMPORAL: 'EVERLTTEMPORALTEMPORAL' | 'everlttemporaltemporal';
EVERNETEMPORALTEMPORAL: 'EVERNETEMPORALTEMPORAL' | 'evernetemporaltemporal';
ALWAYSEQTFLOATFLOAT: 'ALWAYSEQTFLOATFLOAT' | 'alwayseqtfloatfloat';
ALWAYSGETFLOATFLOAT: 'ALWAYSGETFLOATFLOAT' | 'alwaysgetfloatfloat';
ALWAYSGTTFLOATFLOAT: 'ALWAYSGTTFLOATFLOAT' | 'alwaysgttfloatfloat';
ALWAYSLETFLOATFLOAT: 'ALWAYSLETFLOATFLOAT' | 'alwaysletfloatfloat';
ALWAYSLTTFLOATFLOAT: 'ALWAYSLTTFLOATFLOAT' | 'alwayslttfloatfloat';
ALWAYSNETFLOATFLOAT: 'ALWAYSNETFLOATFLOAT' | 'alwaysnetfloatfloat';
EVEREQTFLOATFLOAT: 'EVEREQTFLOATFLOAT' | 'evereqtfloatfloat';
EVERGETFLOATFLOAT: 'EVERGETFLOATFLOAT' | 'evergetfloatfloat';
EVERGTTFLOATFLOAT: 'EVERGTTFLOATFLOAT' | 'evergttfloatfloat';
EVERLETFLOATFLOAT: 'EVERLETFLOATFLOAT' | 'everletfloatfloat';
EVERLTTFLOATFLOAT: 'EVERLTTFLOATFLOAT' | 'everlttfloatfloat';
EVERNETFLOATFLOAT: 'EVERNETFLOATFLOAT' | 'evernetfloatfloat';
ALWAYSEQTINTINT: 'ALWAYSEQTINTINT' | 'alwayseqtintint';
ALWAYSGETINTINT: 'ALWAYSGETINTINT' | 'alwaysgetintint';
ALWAYSGTTINTINT: 'ALWAYSGTTINTINT' | 'alwaysgttintint';
ALWAYSLETINTINT: 'ALWAYSLETINTINT' | 'alwaysletintint';
ALWAYSLTTINTINT: 'ALWAYSLTTINTINT' | 'alwayslttintint';
ALWAYSNETINTINT: 'ALWAYSNETINTINT' | 'alwaysnetintint';
EVEREQTINTINT: 'EVEREQTINTINT' | 'evereqtintint';
EVERGETINTINT: 'EVERGETINTINT' | 'evergetintint';
EVERGTTINTINT: 'EVERGTTINTINT' | 'evergttintint';
EVERLETINTINT: 'EVERLETINTINT' | 'everletintint';
EVERLTTINTINT: 'EVERLTTINTINT' | 'everlttintint';
EVERNETINTINT: 'EVERNETINTINT' | 'evernetintint';
ALWAYSEQFLOATTFLOAT: 'ALWAYSEQFLOATTFLOAT' | 'alwayseqfloattfloat';
ALWAYSGEFLOATTFLOAT: 'ALWAYSGEFLOATTFLOAT' | 'alwaysgefloattfloat';
ALWAYSGTFLOATTFLOAT: 'ALWAYSGTFLOATTFLOAT' | 'alwaysgtfloattfloat';
ALWAYSLEFLOATTFLOAT: 'ALWAYSLEFLOATTFLOAT' | 'alwayslefloattfloat';
ALWAYSLTFLOATTFLOAT: 'ALWAYSLTFLOATTFLOAT' | 'alwaysltfloattfloat';
ALWAYSNEFLOATTFLOAT: 'ALWAYSNEFLOATTFLOAT' | 'alwaysnefloattfloat';
EVEREQFLOATTFLOAT: 'EVEREQFLOATTFLOAT' | 'evereqfloattfloat';
EVERGEFLOATTFLOAT: 'EVERGEFLOATTFLOAT' | 'evergefloattfloat';
EVERGTFLOATTFLOAT: 'EVERGTFLOATTFLOAT' | 'evergtfloattfloat';
EVERLEFLOATTFLOAT: 'EVERLEFLOATTFLOAT' | 'everlefloattfloat';
EVERLTFLOATTFLOAT: 'EVERLTFLOATTFLOAT' | 'everltfloattfloat';
EVERNEFLOATTFLOAT: 'EVERNEFLOATTFLOAT' | 'evernefloattfloat';
ALWAYSEQINTTINT: 'ALWAYSEQINTTINT' | 'alwayseqinttint';
ALWAYSGEINTTINT: 'ALWAYSGEINTTINT' | 'alwaysgeinttint';
ALWAYSGTINTTINT: 'ALWAYSGTINTTINT' | 'alwaysgtinttint';
ALWAYSLEINTTINT: 'ALWAYSLEINTTINT' | 'alwaysleinttint';
ALWAYSLTINTTINT: 'ALWAYSLTINTTINT' | 'alwaysltinttint';
ALWAYSNEINTTINT: 'ALWAYSNEINTTINT' | 'alwaysneinttint';
EVEREQINTTINT: 'EVEREQINTTINT' | 'evereqinttint';
EVERGEINTTINT: 'EVERGEINTTINT' | 'evergeinttint';
EVERGTINTTINT: 'EVERGTINTTINT' | 'evergtinttint';
EVERLEINTTINT: 'EVERLEINTTINT' | 'everleinttint';
EVERLTINTTINT: 'EVERLTINTTINT' | 'everltinttint';
EVERNEINTTINT: 'EVERNEINTTINT' | 'everneinttint';
TEMPORALAINTERSECTSGEOMETRY: 'TEMPORALAINTERSECTSGEOMETRY' | 'temporalaintersectsgeometry';
TEMPORALECONTAINSGEOMETRY: 'TEMPORALECONTAINSGEOMETRY' | 'temporalecontainsgeometry';
TEMPORALINTERSECTSGEOMETRY: 'TEMPORALINTERSECTSGEOMETRY' | 'temporalintersectsgeometry';
TEMPORALEDWITHINGEOMETRY: 'TEMPORALEDWITHINGEOMETRY' | 'temporaledwithingeometry';
ALWAYSEQTCBUFFERCBUFFER: 'ALWAYSEQTCBUFFERCBUFFER' | 'alwayseqtcbuffercbuffer';
ALWAYSNETCBUFFERCBUFFER: 'ALWAYSNETCBUFFERCBUFFER' | 'alwaysnetcbuffercbuffer';
EVEREQTCBUFFERCBUFFER: 'EVEREQTCBUFFERCBUFFER' | 'evereqtcbuffercbuffer';
EVERNETCBUFFERCBUFFER: 'EVERNETCBUFFERCBUFFER' | 'evernetcbuffercbuffer';
TEMPORAL_AT_STBOX: 'TEMPORAL_AT_STBOX' | 'temporal_at_stbox';
ACONTAINS_TCBUFFER_CBUFFER: 'ACONTAINS_TCBUFFER_CBUFFER' | 'acontains_tcbuffer_cbuffer';
ACOVERS_TCBUFFER_CBUFFER: 'ACOVERS_TCBUFFER_CBUFFER' | 'acovers_tcbuffer_cbuffer';
ADISJOINT_TCBUFFER_CBUFFER: 'ADISJOINT_TCBUFFER_CBUFFER' | 'adisjoint_tcbuffer_cbuffer';
AINTERSECTS_TCBUFFER_CBUFFER: 'AINTERSECTS_TCBUFFER_CBUFFER' | 'aintersects_tcbuffer_cbuffer';
ATOUCHES_TCBUFFER_CBUFFER: 'ATOUCHES_TCBUFFER_CBUFFER' | 'atouches_tcbuffer_cbuffer';
ECONTAINS_TCBUFFER_CBUFFER: 'ECONTAINS_TCBUFFER_CBUFFER' | 'econtains_tcbuffer_cbuffer';
ECOVERS_TCBUFFER_CBUFFER: 'ECOVERS_TCBUFFER_CBUFFER' | 'ecovers_tcbuffer_cbuffer';
EDISJOINT_TCBUFFER_CBUFFER: 'EDISJOINT_TCBUFFER_CBUFFER' | 'edisjoint_tcbuffer_cbuffer';
EINTERSECTS_TCBUFFER_CBUFFER: 'EINTERSECTS_TCBUFFER_CBUFFER' | 'eintersects_tcbuffer_cbuffer';
ETOUCHES_TCBUFFER_CBUFFER: 'ETOUCHES_TCBUFFER_CBUFFER' | 'etouches_tcbuffer_cbuffer';
NAD_TCBUFFER_CBUFFER: 'NAD_TCBUFFER_CBUFFER' | 'nad_tcbuffer_cbuffer';
ALWAYSEQ_TQUADBIN_TQUADBIN: 'ALWAYSEQ_TQUADBIN_TQUADBIN' | 'alwayseq_tquadbin_tquadbin';
ALWAYSNE_TQUADBIN_TQUADBIN: 'ALWAYSNE_TQUADBIN_TQUADBIN' | 'alwaysne_tquadbin_tquadbin';
EVEREQ_TQUADBIN_TQUADBIN: 'EVEREQ_TQUADBIN_TQUADBIN' | 'evereq_tquadbin_tquadbin';
EVERNE_TQUADBIN_TQUADBIN: 'EVERNE_TQUADBIN_TQUADBIN' | 'everne_tquadbin_tquadbin';
ALWAYSEQ_QUADBIN_TQUADBIN: 'ALWAYSEQ_QUADBIN_TQUADBIN' | 'alwayseq_quadbin_tquadbin';
ALWAYSNE_QUADBIN_TQUADBIN: 'ALWAYSNE_QUADBIN_TQUADBIN' | 'alwaysne_quadbin_tquadbin';
EVEREQ_QUADBIN_TQUADBIN: 'EVEREQ_QUADBIN_TQUADBIN' | 'evereq_quadbin_tquadbin';
EVERNE_QUADBIN_TQUADBIN: 'EVERNE_QUADBIN_TQUADBIN' | 'everne_quadbin_tquadbin';
ACOVERS_GEO_TGEO: 'ACOVERS_GEO_TGEO' | 'acovers_geo_tgeo';
TFLOAT_COS: 'TFLOAT_COS' | 'tfloat_cos';
TFLOAT_SIN: 'TFLOAT_SIN' | 'tfloat_sin';
TFLOAT_TAN: 'TFLOAT_TAN' | 'tfloat_tan';
TFLOAT_DEGREES: 'TFLOAT_DEGREES' | 'tfloat_degrees';
TNUMBER_ABS: 'TNUMBER_ABS' | 'tnumber_abs';
TEQ_TFLOAT_FLOAT: 'TEQ_TFLOAT_FLOAT' | 'teq_tfloat_float';
TEQ_FLOAT_TFLOAT: 'TEQ_FLOAT_TFLOAT' | 'teq_float_tfloat';
TEQ_TINT_INT: 'TEQ_TINT_INT' | 'teq_tint_int';
TEQ_INT_TINT: 'TEQ_INT_TINT' | 'teq_int_tint';
TEQ_TEMPORAL_TEMPORAL: 'TEQ_TEMPORAL_TEMPORAL' | 'teq_temporal_temporal';
TNE_TFLOAT_FLOAT: 'TNE_TFLOAT_FLOAT' | 'tne_tfloat_float';
TNE_FLOAT_TFLOAT: 'TNE_FLOAT_TFLOAT' | 'tne_float_tfloat';
TNE_TINT_INT: 'TNE_TINT_INT' | 'tne_tint_int';
TNE_INT_TINT: 'TNE_INT_TINT' | 'tne_int_tint';
TNE_TEMPORAL_TEMPORAL: 'TNE_TEMPORAL_TEMPORAL' | 'tne_temporal_temporal';
TLT_TFLOAT_FLOAT: 'TLT_TFLOAT_FLOAT' | 'tlt_tfloat_float';
TLT_FLOAT_TFLOAT: 'TLT_FLOAT_TFLOAT' | 'tlt_float_tfloat';
TLT_TINT_INT: 'TLT_TINT_INT' | 'tlt_tint_int';
TLT_INT_TINT: 'TLT_INT_TINT' | 'tlt_int_tint';
TLT_TEMPORAL_TEMPORAL: 'TLT_TEMPORAL_TEMPORAL' | 'tlt_temporal_temporal';
TLE_TFLOAT_FLOAT: 'TLE_TFLOAT_FLOAT' | 'tle_tfloat_float';
TLE_FLOAT_TFLOAT: 'TLE_FLOAT_TFLOAT' | 'tle_float_tfloat';
TLE_TINT_INT: 'TLE_TINT_INT' | 'tle_tint_int';
TLE_INT_TINT: 'TLE_INT_TINT' | 'tle_int_tint';
TLE_TEMPORAL_TEMPORAL: 'TLE_TEMPORAL_TEMPORAL' | 'tle_temporal_temporal';
TGT_TFLOAT_FLOAT: 'TGT_TFLOAT_FLOAT' | 'tgt_tfloat_float';
TGT_FLOAT_TFLOAT: 'TGT_FLOAT_TFLOAT' | 'tgt_float_tfloat';
TGT_TINT_INT: 'TGT_TINT_INT' | 'tgt_tint_int';
TGT_INT_TINT: 'TGT_INT_TINT' | 'tgt_int_tint';
TGT_TEMPORAL_TEMPORAL: 'TGT_TEMPORAL_TEMPORAL' | 'tgt_temporal_temporal';
TGE_TFLOAT_FLOAT: 'TGE_TFLOAT_FLOAT' | 'tge_tfloat_float';
TGE_FLOAT_TFLOAT: 'TGE_FLOAT_TFLOAT' | 'tge_float_tfloat';
TGE_TINT_INT: 'TGE_TINT_INT' | 'tge_tint_int';
TGE_INT_TINT: 'TGE_INT_TINT' | 'tge_int_tint';
TGE_TEMPORAL_TEMPORAL: 'TGE_TEMPORAL_TEMPORAL' | 'tge_temporal_temporal';
GEO_FROM_GEOJSON: 'GEO_FROM_GEOJSON' | 'geo_from_geojson';
GEOM_FROM_HEXEWKB: 'GEOM_FROM_HEXEWKB' | 'geom_from_hexewkb';
GEO_AS_HEXEWKB: 'GEO_AS_HEXEWKB' | 'geo_as_hexewkb';
GEOM_MIN_BOUNDING_CENTER: 'GEOM_MIN_BOUNDING_CENTER' | 'geom_min_bounding_center';
GEOM_MIN_BOUNDING_RADIUS: 'GEOM_MIN_BOUNDING_RADIUS' | 'geom_min_bounding_radius';
GEO_TRANSFORM_PIPELINE: 'GEO_TRANSFORM_PIPELINE' | 'geo_transform_pipeline';
GEOM_BUFFER: 'GEOM_BUFFER' | 'geom_buffer';
GEOM_RELATE_PATTERN: 'GEOM_RELATE_PATTERN' | 'geom_relate_pattern';
H3_GS_POINT_TO_CELL: 'H3_GS_POINT_TO_CELL' | 'h3_gs_point_to_cell';
H3INDEX_IN: 'H3INDEX_IN' | 'h3index_in';
EINTERSECTS_TPCPOINT_GEO: 'EINTERSECTS_TPCPOINT_GEO' | 'eintersects_tpcpoint_geo';
NAD_TPCPOINT_GEO: 'NAD_TPCPOINT_GEO' | 'nad_tpcpoint_geo';
JSON_ARRAY_LENGTH: 'JSON_ARRAY_LENGTH' | 'json_array_length';
JSON_TYPEOF: 'JSON_TYPEOF' | 'json_typeof';
JSON_ARRAY_ELEMENT_TEXT: 'JSON_ARRAY_ELEMENT_TEXT' | 'json_array_element_text';
JSON_OBJECT_FIELD_TEXT: 'JSON_OBJECT_FIELD_TEXT' | 'json_object_field_text';
FLOATSPAN_MAKE: 'FLOATSPAN_MAKE' | 'floatspan_make';
INTSPAN_MAKE: 'INTSPAN_MAKE' | 'intspan_make';
/* END CODEGEN LEXER TOKENS */
WATERMARK: 'WATERMARK' | 'watermark';
OFFSET: 'OFFSET' | 'offset';
LOCALHOST: 'LOCALHOST' | 'localhost';
CSV_FORMAT : 'CSV_FORMAT';
AT_MOST_ONCE : 'AT_MOST_ONCE';
AT_LEAST_ONCE : 'AT_LEAST_ONCE';
JSON: 'JSON';
TEXT: 'TEXT';
///--NebulaSQL-KEYWORD-LIST-END
///****************************
/// End of the keywords list
///****************************



BOOLEAN_VALUE: 'true' | 'false';
EQ  : '=' | '==';
NSEQ: '<=>';
NEQ : '<>';
NEQJ: '!=';
LT  : '<';
LTE : '<=' | '!>';
GT  : '>';
GTE : '>=' | '!<';

PLUS: '+';
MINUS: '-';
ASTERISK: '*';
SLASH: '/';
PERCENT: '%';
TILDE: '~';
AMPERSAND: '&';
PIPE: '|';
CONCAT_PIPE: '||';
HAT: '^';

STRING
    : '\'' ( ~('\''|'\\') | ('\\' .) )* '\''
    | '"' ( ~('"'|'\\') | ('\\' .) )* '"'
    ;

INTEGER_VALUE
    : DIGIT+
    ;

FLOAT_LITERAL
    : DIGIT+ EXPONENT?
    | DECIMAL_DIGITS EXPONENT? {isValidDecimal()}?
    ;


fragment DECIMAL_DIGITS
    : DIGIT+ '.' DIGIT*
    | '.' DIGIT+
    ;

fragment EXPONENT
    : 'E' [+-]? DIGIT+
    ;

fragment DIGIT
    : [0-9]
    ;

fragment LETTER
    : ('a'..'z'|'A'..'Z'|'_')
    ;

WS
    : [ \r\n\t]+ -> channel(HIDDEN)
    ;


SINKS: 'SINKS';
SOURCES: 'SOURCES' | 'sources';
QUERIES: 'QUERIES' | 'queries';


DATA_TYPE: INTEGER_SIGNED_TYPE | INTEGER_UNSIGNED_TYPE | FLOATING_POINT_TYPE | CHAR_TYPE | VARSIZED_TYPE | BOOLEAN_TYPE;

INTEGER_UNSIGNED_TYPE: UNSIGNED_TYPE_QUALIFIER INTEGER_BASES_TYPES | 'UINT8' | 'UINT16' | 'UINT32' | 'UINT64';
INTEGER_SIGNED_TYPE: INTEGER_BASES_TYPES | 'INT64' | 'INT32' | 'INT16' | 'INT8';
INTEGER_BASES_TYPES: TINY_INT_TYPE | SMALL_INT_TYPE | NORMAL_INT_TYPE | BIG_INT_TYPE;
TINY_INT_TYPE: 'TINYINT';
SMALL_INT_TYPE: 'SMALLINT';
NORMAL_INT_TYPE: 'INT' | 'INTEGER';
BIG_INT_TYPE: 'BIGINT';
FLOATING_POINT_TYPE: 'FLOAT32' | 'FLOAT64';
CHAR_TYPE: 'CHAR';
VARSIZED_TYPE: 'VARSIZED';
BOOLEAN_TYPE: 'BOOLEAN';

UNSIGNED_TYPE_QUALIFIER: 'UNSIGNED ';



SHOW : 'SHOW';
FORMAT : 'FORMAT';
CREATE : 'CREATE';
SOURCE : 'SOURCE';
LOGICAL: 'LOGICAL';
PHYSICAL: 'PHYSICAL';
SINK : 'SINK';

//Make sure that you add lexer rules for keywords before the identifier rule,
//otherwise it will take priority and your grammars will not work

SIMPLE_COMMENT
    : '--' ('\\\n' | ~[\r\n])* '\r'? '\n'? -> channel(HIDDEN)
    ;

BRACKETED_COMMENT
    : '/*' {!isHint()}? (BRACKETED_COMMENT|.)*? '*/' -> channel(HIDDEN)
    ;

IDENTIFIER
    : LETTER (LETTER | DIGIT | '_')*
    ;

/// Catch-all for anything we can't recognize.
/// We use this to be able to ignore and recover all the text
/// when splitting statements with DelimiterLexer
UNRECOGNIZED
    : .
    ;
