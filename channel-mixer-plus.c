/* This file is an image processing operation for GEGL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Modified from official GEGL channel-mixer, add RGB offset, remove preserve luminosity
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

/* Red output channel */
property_double (rr_gain, _("Red in Red channel"), 1.0)
  description(_("Set the red amount for the red channel"))
  value_range (-2.0, 2.0)

property_double (rg_gain, _("Green in Red channel"), 0.0)
  description(_("Set the green amount for the red channel"))
  value_range (-2.0, 2.0)

property_double (rb_gain, _("Blue in Red channel"), 0.0)
  description(_("Set the blue amount for the red channel"))
  value_range (-2.0, 2.0)

property_double (r_offset, _("Red Offset"), 0.0)
  description(_("Constant offset added to red output channel"))
  value_range (-1.0, 1.0)

/* Green output channel */
property_double (gr_gain, _("Red in Green channel"), 0.0)
  description(_("Set the red amount for the green channel"))
  value_range (-2.0, 2.0)

property_double (gg_gain, _("Green for Green channel"), 1.0)
  description(_("Set the green amount for the green channel"))
  value_range (-2.0, 2.0)

property_double (gb_gain, _("Blue in Green channel"), 0.0)
  description(_("Set the blue amount for the green channel"))
  value_range (-2.0, 2.0)

property_double (g_offset, _("Green Offset"), 0.0)
  description(_("Constant offset added to green output channel"))
  value_range (-1.0, 1.0)

/* Blue output channel */
property_double (br_gain, _("Red in Blue channel"), 0.0)
  description(_("Set the red amount for the blue channel"))
  value_range (-2.0, 2.0)

property_double (bg_gain, _("Green in Blue channel"), 0.0)
  description(_("Set the green amount for the blue channel"))
  value_range (-2.0, 2.0)

property_double (bb_gain, _("Blue in Blue channel"), 1.0)
  description(_("Set the blue amount for the blue channel"))
  value_range (-2.0, 2.0)

property_double (b_offset, _("Blue Offset"), 0.0)
  description(_("Constant offset added to blue output channel"))
  value_range (-1.0, 1.0)

#else

#define GEGL_OP_POINT_FILTER
#define GEGL_OP_NAME     channel_mixer_plus
#define GEGL_OP_C_SOURCE channel-mixer-plus.c

#include "gegl-op.h"

typedef struct
{
  gdouble       red_gain;
  gdouble       green_gain;
  gdouble       blue_gain;
  gdouble       offset;
} CmChannelType;

typedef struct
{
  CmChannelType  red;
  CmChannelType  green;
  CmChannelType  blue;

  gboolean       has_alpha;
} CmParamsType;

static void prepare (GeglOperation *operation)
{
  const Babl *input_format = gegl_operation_get_source_format (operation, "input");
  GeglProperties *o = GEGL_PROPERTIES (operation);
  CmParamsType *mix;
  const Babl *format;

  if (o->user_data == NULL)
    o->user_data = g_slice_new0 (CmParamsType);

  mix = (CmParamsType*) o->user_data;

  mix->red.red_gain     = o->rr_gain;
  mix->red.green_gain   = o->rg_gain;
  mix->red.blue_gain    = o->rb_gain;
  mix->red.offset       = o->r_offset;

  mix->green.red_gain   = o->gr_gain;
  mix->green.green_gain = o->gg_gain;
  mix->green.blue_gain  = o->gb_gain;
  mix->green.offset     = o->g_offset;

  mix->blue.red_gain    = o->br_gain;
  mix->blue.green_gain  = o->bg_gain;
  mix->blue.blue_gain   = o->bb_gain;
  mix->blue.offset      = o->b_offset;

  if (input_format == NULL || babl_format_has_alpha (input_format))
    {
      mix->has_alpha = TRUE;
      format = babl_format_with_space ("RGBA float", input_format);
    }
  else
    {
      mix->has_alpha = FALSE;
      format = babl_format_with_space ("RGB float", input_format);
    }

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}

static void finalize (GObject *object)
{
  GeglOperation *op = (void*) object;
  GeglProperties *o = GEGL_PROPERTIES (op);

  if (o->user_data)
    {
      g_slice_free (CmParamsType, o->user_data);
      o->user_data = NULL;
    }

  G_OBJECT_CLASS (gegl_op_parent_class)->finalize (object);
}

static inline gfloat
cm_mix_pixel (CmChannelType *ch,
              gfloat         r,
              gfloat         g,
              gfloat         b)
{
  gdouble c = ch->red_gain * r + ch->green_gain * g + ch->blue_gain * b;
  c += ch->offset;
  return (gfloat) c;
}

static inline void
cm_process_pixel (CmParamsType  *mix,
                  const gfloat  *s,
                  gfloat        *d)
{
  d[0] = cm_mix_pixel (&mix->red,   s[0], s[1], s[2]);
  d[1] = cm_mix_pixel (&mix->green, s[0], s[1], s[2]);
  d[2] = cm_mix_pixel (&mix->blue,  s[0], s[1], s[2]);
}

static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                samples,
         const GeglRectangle *roi,
         gint                 level)
{
  GeglProperties   *o = GEGL_PROPERTIES (op);
  CmParamsType *mix = (CmParamsType*) o->user_data;

  gfloat       *in, *out;

  g_assert (mix != NULL);

  in = in_buf;
  out = out_buf;

  if (mix->has_alpha)
    {
      while (samples--)
        {
          cm_process_pixel (mix, in, out);
          out[3] = in[3];

          in += 4;
          out += 4;
        }
    }
  else
    {
      while (samples--)
        {
          cm_process_pixel (mix, in, out);
          in += 3;
          out += 3;
        }
    }

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass            *oc = GEGL_OPERATION_CLASS (klass);
  GeglOperationPointFilterClass *cc = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  oc->prepare = prepare;
  cc->process = process;
  G_OBJECT_CLASS (klass)->finalize = finalize;

  gegl_operation_class_set_keys (oc,
    "name",        "lb:channel-mixer-plus",
    "title",       _("Channel Mixer Plus"),
    "description", _("Mix RGB channels with independent constant offsets, no luminosity preservation"),
    "gimp:menu-path", "<Image>/Colors/myfilters",
    "gimp:menu-label", _("Channel Mixer Plus..."),
    NULL);
}

#endif