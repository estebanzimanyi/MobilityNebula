/* Wave-25 probe: verify the six candidate MEOS calls + their marshalling
 * compile, run, and return sane values BEFORE the ~90-min codegen build.
 * Build/run inside the v15 container against the installed libmeos. */
#include <stdio.h>
#include <stdlib.h>
#include <meos.h>
#include <meos_geo.h>
#include <meos_cbuffer.h>
#include <meos_pose.h>

int main(void)
{
  meos_initialize();

  /* 1. minus_date_date(DateADT, DateADT) -> int */
  DateADT d1 = date_in("2020-01-05");
  DateADT d2 = date_in("2020-01-01");
  int dd = minus_date_date(d1, d2);
  printf("minus_date_date(2020-01-05, 2020-01-01) = %d\n", dd);

  /* 2. minus_timestamptz_timestamptz(TimestampTz, TimestampTz) -> Interval* */
  TimestampTz t1 = timestamptz_in("2020-01-05 00:00:00+00", -1);
  TimestampTz t2 = timestamptz_in("2020-01-01 00:00:00+00", -1);
  Interval *iv = minus_timestamptz_timestamptz(t1, t2);
  char *ivs = interval_out(iv);
  printf("minus_timestamptz_timestamptz = %s\n", ivs);
  free(ivs); free(iv);

  /* 3. box3d_make(xmin,xmax,ymin,ymax,zmin,zmax,srid) -> BOX3D* */
  BOX3D *b3 = box3d_make(1.0, 3.0, 1.0, 3.0, 0.0, 5.0, 4326);
  char *b3s = box3d_out(b3, 6);
  printf("box3d_make = %s\n", b3s);
  free(b3s); free(b3);

  /* 4. gbox_make(hasz,xmin,xmax,ymin,ymax,zmin,zmax) -> GBOX* */
  GBOX *gb = gbox_make(true, 1.0, 3.0, 1.0, 3.0, 0.0, 5.0);
  char *gbs = gbox_out(gb, 6);
  printf("gbox_make = %s\n", gbs);
  free(gbs); free(gb);

  /* 5. tcbuffer_make(tpoint, tfloat) -> Temporal* */
  Temporal *tp = tgeompoint_in("[POINT(1 1)@2020-01-01, POINT(2 2)@2020-01-02]");
  Temporal *tf = tfloat_in("[1.0@2020-01-01, 2.0@2020-01-02]");
  Temporal *tcb = tcbuffer_make(tp, tf);
  size_t hsz;
  char *tcbh = temporal_as_hexwkb(tcb, 0x04 /* WKB_EXTENDED */, &hsz);
  printf("tcbuffer_make hexwkb len = %zu (head %.16s)\n", hsz, tcbh);
  free(tcbh); free(tcb);

  /* 6. tpose_make(tpoint, tradius) -> Temporal* */
  Temporal *tp2 = tgeompoint_in("[POINT(1 1)@2020-01-01, POINT(2 2)@2020-01-02]");
  Temporal *tr  = tfloat_in("[0.5@2020-01-01, 0.7@2020-01-02]");
  Temporal *tps = tpose_make(tp2, tr);
  char *tpsh = temporal_as_hexwkb(tps, 0x04, &hsz);
  printf("tpose_make hexwkb len = %zu (head %.16s)\n", hsz, tpsh);
  free(tpsh); free(tps);
  free(tp); free(tf); free(tp2); free(tr);

  meos_finalize();
  printf("PROBE OK\n");
  return 0;
}
