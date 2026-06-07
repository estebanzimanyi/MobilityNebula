/* Wave-31 probe (sequence inputs): the array-of-temporal accessor family is
 * WINDOWED (instant inputs error/empty, confirmed). Verify each op on a
 * continuous SEQUENCE produces a recordable temporal_as_hexwkb(elem,0x04,&sz)
 * result, and inspect the serialization shape (esp. trgeometry elements).
 * Build/run inside the v15 container against the installed libmeos. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <meos.h>
#include <meos_geo.h>
#include <meos_rgeo.h>

static void dump_arr(const char* label, void** arr, int cnt)
{
  if (!arr || cnt <= 0) { printf("  %-32s -> NULL/empty (cnt=%d)\n", label, cnt); return; }
  printf("  %-32s -> cnt=%d : {", label, cnt);
  for (int i = 0; i < cnt; i++)
  {
    size_t sz = 0;
    char* e = temporal_as_hexwkb((const Temporal*) arr[i], 0x04, &sz);
    if (i) printf(", ");
    printf("%.28s%s", e ? e : "(null)", (e && sz > 28) ? ".." : "");
    if (e) free(e);
  }
  printf("}\n");
}

static void dump_one(const char* label, Temporal* t)
{
  if (!t) { printf("  %-32s -> NULL\n", label); return; }
  size_t sz = 0;
  char* e = temporal_as_hexwkb(t, 0x04, &sz);
  printf("  %-32s -> %.44s%s\n", label, e ? e : "(null)", (e && sz > 44) ? ".." : "");
  if (e) free(e);
}

int main(void)
{
  meos_initialize();
  setvbuf(stdout, NULL, _IONBF, 0);

  GSERIALIZED* g = geom_in("Polygon((0 0,1 0,1 1,0 1,0 0))", -1);

  Temporal* tf_seq  = tfloat_in("[1.5@2020-01-01, 2.5@2020-01-02, 3.5@2020-01-03]");
  Temporal* tgp_seq = tgeompoint_in("SRID=4326;[Point(1 1)@2020-01-01, Point(2 2)@2020-01-02, Point(3 1)@2020-01-03]");
  Temporal* tp_seq  = tpose_in("[Pose(Point(1 1),0.5)@2020-01-01, Pose(Point(2 2),0.7)@2020-01-02, Pose(Point(3 1),0.9)@2020-01-03]");
  Temporal* trg_seq = geo_tpose_to_trgeometry(g, tp_seq);

  /* sequenceSet inputs */
  Temporal* tf_ss  = tfloat_in("{[1.5@2020-01-01, 2.5@2020-01-02], [3.5@2020-01-04, 4.5@2020-01-05]}");
  Temporal* tp_ss  = tpose_in("{[Pose(Point(1 1),0.5)@2020-01-01, Pose(Point(2 2),0.7)@2020-01-02], [Pose(Point(3 1),0.9)@2020-01-04, Pose(Point(4 0),0.2)@2020-01-05]}");
  Temporal* trg_ss = geo_tpose_to_trgeometry(g, tp_ss);

  int cnt; void** a;
  /* NB: capture the array, THEN read cnt — never pass fn(&cnt) and cnt in the
   * same call (unspecified arg-eval order reads cnt before the call writes it). */
  printf("== CONTROL (proven) ==\n");
  cnt = 0; a = (void**) temporal_instants(tf_seq, &cnt);   dump_arr("temporal_instants(tf_seq)", a, cnt);
  printf("== ARRAY OPS on single SEQUENCE ==\n");
  cnt = 0; a = (void**) temporal_segments(tf_seq, &cnt);   dump_arr("temporal_segments(tf_seq)", a, cnt);
  cnt = 0; a = (void**) temporal_sequences(tf_seq, &cnt);  dump_arr("temporal_sequences(tf_seq)", a, cnt);
  cnt = 0; a = (void**) tpoint_make_simple(tgp_seq, &cnt); dump_arr("tpoint_make_simple(tgp_seq)", a, cnt);
  printf("== ARRAY OPS on SEQUENCESET ==\n");
  cnt = 0; a = (void**) temporal_segments(tf_ss, &cnt);    dump_arr("temporal_segments(tf_ss)", a, cnt);
  cnt = 0; a = (void**) temporal_sequences(tf_ss, &cnt);   dump_arr("temporal_sequences(tf_ss)", a, cnt);
  cnt = 0; a = (void**) trgeometry_instants(trg_ss, &cnt); dump_arr("trgeometry_instants(trg_ss)", a, cnt);
  cnt = 0; a = (void**) trgeometry_segments(trg_ss, &cnt); dump_arr("trgeometry_segments(trg_ss)", a, cnt);
  cnt = 0; a = (void**) trgeometry_sequences(trg_ss, &cnt);dump_arr("trgeometry_sequences(trg_ss)", a, cnt);
  printf("== single-return on sequence (worked before) ==\n");
  dump_one("trgeometry_sequence_n(trg,1)", (Temporal*) trgeometry_sequence_n(trg_seq, 1));
  dump_one("trgeometry_start_sequence(trg)", (Temporal*) trgeometry_start_sequence(trg_seq));
  dump_one("trgeometry_end_sequence(trg)", (Temporal*) trgeometry_end_sequence(trg_seq));

  meos_finalize();
  return 0;
}
