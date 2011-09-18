/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-image.h
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CCM_IMAGE_H_
#define _CCM_IMAGE_H_

#include "ccm-display.h"
#include "ccm-pixmap.h"

G_BEGIN_DECLS 

typedef struct _CCMImage CCMImage;

CCMImage*           ccm_image_new           (CCMDisplay* display, Visual* visual,
                                             cairo_format_t format,
                                             int width, int height, int depth);
void                ccm_image_destroy       (CCMImage* image);
gboolean            ccm_image_get_image     (CCMImage* image, CCMPixmap* pixmap, 
                                             int x, int y);
gboolean            ccm_image_get_sub_image (CCMImage* image, CCMPixmap* pixmap,
                                             int x, int y,
                                             int width, int height);
gboolean            ccm_image_put_image     (CCMImage* image, CCMPixmap* pixmap, 
                                             int x_src, int y_src, int x, int y, 
                                             int width, int height);
G_GNUC_PURE guchar*   ccm_image_get_data    (CCMImage* image);
G_GNUC_PURE gint      ccm_image_get_width   (CCMImage* image);
G_GNUC_PURE gint      ccm_image_get_height  (CCMImage* image);
G_GNUC_PURE gint      ccm_image_get_stride  (CCMImage* image);

G_END_DECLS

#endif                          /* _CCM_IMAGE_H_ */
