/* @(#) $Id$ */

#ifndef __GWY_APP_STOCK_H__
#define __GWY_APP_STOCK_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_STOCK_ZOOM_IN        "gwy_zoom_in"
#define GWY_STOCK_ZOOM_OUT       "gwy_zoom_out"
#define GWY_STOCK_ZOOM_1_1       "gwy_zoom_1_1"
#define GWY_STOCK_ZOOM_FIT       "gwy_zoom_fit"
#define GWY_STOCK_FIT_PLANE      "gwy_fit_plane"
#define GWY_STOCK_FIT_TRIANGLE   "gwy_fit_triangle"
#define GWY_STOCK_GRAPH          "gwy_graph"
#define GWY_STOCK_CROP           "gwy_crop"
#define GWY_STOCK_SCALE          "gwy_scale"
#define GWY_STOCK_ROTATE         "gwy_rotate"

void gwy_app_register_stock_items(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_STOCK_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

