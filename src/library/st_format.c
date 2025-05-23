/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
 * Copyright (c) 2008-2010 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/


/**
 * Mesa / Gallium format conversion and format selection code.
 * \author Brian Paul
 */

#include "main/imports.h"
#include "main/context.h"
#include "main/texstore.h"
#include "main/image.h"
#include "main/macros.h"
#include "main/mfeatures.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "util/u_format.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_format.h"

#if !defined(NDEBUG)

#include "rsxgl_assert.h"

#if defined(assert)
#undef assert
#endif

#define assert rsxgl_assert

#endif

static GLuint
format_max_bits(enum pipe_format format)
{
   GLuint size = util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 0);

   size = MAX2(size, util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 1));
   size = MAX2(size, util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 2));
   size = MAX2(size, util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 3));
   size = MAX2(size, util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_ZS, 0));
   size = MAX2(size, util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_ZS, 1));
   return size;
}

/**
 * Return basic GL datatype for the given gallium format.
 */
GLenum
st_format_datatype(enum pipe_format format)
{
   const struct util_format_description *desc;
   int i;

   desc = util_format_description(format);
   assert(desc);

   /* Find the first non-VOID channel. */
   for (i = 0; i < 4; i++) {
       if (desc->channel[i].type != UTIL_FORMAT_TYPE_VOID) {
           break;
       }
   }

   if (desc->layout == UTIL_FORMAT_LAYOUT_PLAIN) {
      if (format == PIPE_FORMAT_B5G5R5A1_UNORM ||
          format == PIPE_FORMAT_B5G6R5_UNORM) {
         return GL_UNSIGNED_SHORT;
      }
      else if (format == PIPE_FORMAT_R11G11B10_FLOAT ||
               format == PIPE_FORMAT_R9G9B9E5_FLOAT) {
         return GL_FLOAT;
      }
      else if (format == PIPE_FORMAT_Z24_UNORM_S8_UINT ||
               format == PIPE_FORMAT_S8_UINT_Z24_UNORM ||
               format == PIPE_FORMAT_Z24X8_UNORM ||
               format == PIPE_FORMAT_X8Z24_UNORM) {
         return GL_UNSIGNED_INT_24_8;
      }
      else if (format == PIPE_FORMAT_Z32_FLOAT_S8X24_UINT) {
         return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
      }
      else {
         const GLuint size = format_max_bits(format);

         assert(i < 4);
         if (i == 4)
            return GL_NONE;

         if (size == 8) {
            if (desc->channel[i].type == UTIL_FORMAT_TYPE_UNSIGNED)
               return GL_UNSIGNED_BYTE;
            else
               return GL_BYTE;
         }
         else if (size == 16) {
            if (desc->channel[i].type == UTIL_FORMAT_TYPE_FLOAT)
               return GL_HALF_FLOAT;
            if (desc->channel[i].type == UTIL_FORMAT_TYPE_UNSIGNED)
               return GL_UNSIGNED_SHORT;
            else
               return GL_SHORT;
         }
         else if (size <= 32) {
            if (desc->channel[i].type == UTIL_FORMAT_TYPE_FLOAT)
               return GL_FLOAT;
            if (desc->channel[i].type == UTIL_FORMAT_TYPE_UNSIGNED)
               return GL_UNSIGNED_INT;
            else
               return GL_INT;
         }
         else {
            assert(size == 64);
            assert(desc->channel[i].type == UTIL_FORMAT_TYPE_FLOAT);
            return GL_DOUBLE;
         }
      }
   }
   else if (format == PIPE_FORMAT_UYVY ||
       format == PIPE_FORMAT_YUYV ||
       format == PIPE_FORMAT_IYUV ||
       format == PIPE_FORMAT_YV12 ||
       format == PIPE_FORMAT_YV16 ||
       format == PIPE_FORMAT_NV12 ||
       format == PIPE_FORMAT_NV21) {
      return GL_UNSIGNED_SHORT;
   }
   else {
      /* probably a compressed format, unsupported anyway */
      return GL_NONE;
   }
}

#if 0
/**
 * Translate Mesa format to Gallium format.
 */
enum pipe_format
st_mesa_format_to_pipe_format(gl_format mesaFormat)
{
   switch (mesaFormat) {
   case MESA_FORMAT_RGBA8888:
      return PIPE_FORMAT_A8B8G8R8_UNORM;
   case MESA_FORMAT_RGBA8888_REV:
      return PIPE_FORMAT_R8G8B8A8_UNORM;
   case MESA_FORMAT_ARGB8888:
      return PIPE_FORMAT_B8G8R8A8_UNORM;
   case MESA_FORMAT_ARGB8888_REV:
      return PIPE_FORMAT_A8R8G8B8_UNORM;
   case MESA_FORMAT_RGBX8888:
      return PIPE_FORMAT_X8B8G8R8_UNORM;
   case MESA_FORMAT_RGBX8888_REV:
      return PIPE_FORMAT_R8G8B8X8_UNORM;
   case MESA_FORMAT_XRGB8888:
      return PIPE_FORMAT_B8G8R8X8_UNORM;
   case MESA_FORMAT_XRGB8888_REV:
      return PIPE_FORMAT_X8R8G8B8_UNORM;
   case MESA_FORMAT_ARGB1555:
      return PIPE_FORMAT_B5G5R5A1_UNORM;
   case MESA_FORMAT_ARGB4444:
      return PIPE_FORMAT_B4G4R4A4_UNORM;
   case MESA_FORMAT_RGB565:
      return PIPE_FORMAT_B5G6R5_UNORM;
   case MESA_FORMAT_RGB332:
      return PIPE_FORMAT_B2G3R3_UNORM;
   case MESA_FORMAT_ARGB2101010:
      return PIPE_FORMAT_B10G10R10A2_UNORM;
   case MESA_FORMAT_AL44:
      return PIPE_FORMAT_L4A4_UNORM;
   case MESA_FORMAT_AL88:
      return PIPE_FORMAT_L8A8_UNORM;
   case MESA_FORMAT_AL1616:
      return PIPE_FORMAT_L16A16_UNORM;
   case MESA_FORMAT_A8:
      return PIPE_FORMAT_A8_UNORM;
   case MESA_FORMAT_A16:
      return PIPE_FORMAT_A16_UNORM;
   case MESA_FORMAT_L8:
      return PIPE_FORMAT_L8_UNORM;
   case MESA_FORMAT_L16:
      return PIPE_FORMAT_L16_UNORM;
   case MESA_FORMAT_I8:
      return PIPE_FORMAT_I8_UNORM;
   case MESA_FORMAT_I16:
      return PIPE_FORMAT_I16_UNORM;
   case MESA_FORMAT_Z16:
      return PIPE_FORMAT_Z16_UNORM;
   case MESA_FORMAT_Z32:
      return PIPE_FORMAT_Z32_UNORM;
   case MESA_FORMAT_Z24_S8:
      return PIPE_FORMAT_S8_UINT_Z24_UNORM;
   case MESA_FORMAT_S8_Z24:
      return PIPE_FORMAT_Z24_UNORM_S8_UINT;
   case MESA_FORMAT_Z24_X8:
      return PIPE_FORMAT_X8Z24_UNORM;
   case MESA_FORMAT_X8_Z24:
      return PIPE_FORMAT_Z24X8_UNORM;
   case MESA_FORMAT_S8:
      return PIPE_FORMAT_S8_UINT;
   case MESA_FORMAT_Z32_FLOAT:
      return PIPE_FORMAT_Z32_FLOAT;
   case MESA_FORMAT_Z32_FLOAT_X24S8:
      return PIPE_FORMAT_Z32_FLOAT_S8X24_UINT;
   case MESA_FORMAT_YCBCR:
      return PIPE_FORMAT_YUYV;
   case MESA_FORMAT_YCBCR_REV:
      return PIPE_FORMAT_UYVY;
#if FEATURE_texture_s3tc
   case MESA_FORMAT_RGB_DXT1:
      return PIPE_FORMAT_DXT1_RGB;
   case MESA_FORMAT_RGBA_DXT1:
      return PIPE_FORMAT_DXT1_RGBA;
   case MESA_FORMAT_RGBA_DXT3:
      return PIPE_FORMAT_DXT3_RGBA;
   case MESA_FORMAT_RGBA_DXT5:
      return PIPE_FORMAT_DXT5_RGBA;
#if FEATURE_EXT_texture_sRGB
   case MESA_FORMAT_SRGB_DXT1:
      return PIPE_FORMAT_DXT1_SRGB;
   case MESA_FORMAT_SRGBA_DXT1:
      return PIPE_FORMAT_DXT1_SRGBA;
   case MESA_FORMAT_SRGBA_DXT3:
      return PIPE_FORMAT_DXT3_SRGBA;
   case MESA_FORMAT_SRGBA_DXT5:
      return PIPE_FORMAT_DXT5_SRGBA;
#endif
#endif
#if FEATURE_EXT_texture_sRGB
   case MESA_FORMAT_SLA8:
      return PIPE_FORMAT_L8A8_SRGB;
   case MESA_FORMAT_SL8:
      return PIPE_FORMAT_L8_SRGB;
   case MESA_FORMAT_SRGB8:
      return PIPE_FORMAT_R8G8B8_SRGB;
   case MESA_FORMAT_SRGBA8:
      return PIPE_FORMAT_A8B8G8R8_SRGB;
   case MESA_FORMAT_SARGB8:
      return PIPE_FORMAT_B8G8R8A8_SRGB;
#endif
   case MESA_FORMAT_RGBA_FLOAT32:
      return PIPE_FORMAT_R32G32B32A32_FLOAT;
   case MESA_FORMAT_RGBA_FLOAT16:
      return PIPE_FORMAT_R16G16B16A16_FLOAT;
   case MESA_FORMAT_RGB_FLOAT32:
      return PIPE_FORMAT_R32G32B32_FLOAT;
   case MESA_FORMAT_RGB_FLOAT16:
      return PIPE_FORMAT_R16G16B16_FLOAT;
   case MESA_FORMAT_LUMINANCE_ALPHA_FLOAT32:
      return PIPE_FORMAT_L32A32_FLOAT;
   case MESA_FORMAT_LUMINANCE_ALPHA_FLOAT16:
      return PIPE_FORMAT_L16A16_FLOAT;
   case MESA_FORMAT_LUMINANCE_FLOAT32:
      return PIPE_FORMAT_L32_FLOAT;
   case MESA_FORMAT_LUMINANCE_FLOAT16:
      return PIPE_FORMAT_L16_FLOAT;
   case MESA_FORMAT_ALPHA_FLOAT32:
      return PIPE_FORMAT_A32_FLOAT;
   case MESA_FORMAT_ALPHA_FLOAT16:
      return PIPE_FORMAT_A16_FLOAT;
   case MESA_FORMAT_INTENSITY_FLOAT32:
      return PIPE_FORMAT_I32_FLOAT;
   case MESA_FORMAT_INTENSITY_FLOAT16:
      return PIPE_FORMAT_I16_FLOAT;
   case MESA_FORMAT_R_FLOAT32:
      return PIPE_FORMAT_R32_FLOAT;
   case MESA_FORMAT_R_FLOAT16:
      return PIPE_FORMAT_R16_FLOAT;
   case MESA_FORMAT_RG_FLOAT32:
      return PIPE_FORMAT_R32G32_FLOAT;
   case MESA_FORMAT_RG_FLOAT16:
      return PIPE_FORMAT_R16G16_FLOAT;

   case MESA_FORMAT_R8:
      return PIPE_FORMAT_R8_UNORM;
   case MESA_FORMAT_R16:
      return PIPE_FORMAT_R16_UNORM;
   case MESA_FORMAT_GR88:
      return PIPE_FORMAT_R8G8_UNORM;
   case MESA_FORMAT_RG1616:
      return PIPE_FORMAT_R16G16_UNORM;
   case MESA_FORMAT_RGBA_16:
      return PIPE_FORMAT_R16G16B16A16_UNORM;

   /* signed int formats */
   case MESA_FORMAT_ALPHA_UINT8:
      return PIPE_FORMAT_A8_UINT;
   case MESA_FORMAT_ALPHA_UINT16:
      return PIPE_FORMAT_A16_UINT;
   case MESA_FORMAT_ALPHA_UINT32:
      return PIPE_FORMAT_A32_UINT;

   case MESA_FORMAT_ALPHA_INT8:
      return PIPE_FORMAT_A8_SINT;
   case MESA_FORMAT_ALPHA_INT16:
      return PIPE_FORMAT_A16_SINT;
   case MESA_FORMAT_ALPHA_INT32:
      return PIPE_FORMAT_A32_SINT;

   case MESA_FORMAT_INTENSITY_UINT8:
      return PIPE_FORMAT_I8_UINT;
   case MESA_FORMAT_INTENSITY_UINT16:
      return PIPE_FORMAT_I16_UINT;
   case MESA_FORMAT_INTENSITY_UINT32:
      return PIPE_FORMAT_I32_UINT;

   case MESA_FORMAT_INTENSITY_INT8:
      return PIPE_FORMAT_I8_SINT;
   case MESA_FORMAT_INTENSITY_INT16:
      return PIPE_FORMAT_I16_SINT;
   case MESA_FORMAT_INTENSITY_INT32:
      return PIPE_FORMAT_I32_SINT;

   case MESA_FORMAT_LUMINANCE_UINT8:
      return PIPE_FORMAT_L8_UINT;
   case MESA_FORMAT_LUMINANCE_UINT16:
      return PIPE_FORMAT_L16_UINT;
   case MESA_FORMAT_LUMINANCE_UINT32:
      return PIPE_FORMAT_L32_UINT;

   case MESA_FORMAT_LUMINANCE_INT8:
      return PIPE_FORMAT_L8_SINT;
   case MESA_FORMAT_LUMINANCE_INT16:
      return PIPE_FORMAT_L16_SINT;
   case MESA_FORMAT_LUMINANCE_INT32:
      return PIPE_FORMAT_L32_SINT;

   case MESA_FORMAT_LUMINANCE_ALPHA_UINT8:
      return PIPE_FORMAT_L8A8_UINT;
   case MESA_FORMAT_LUMINANCE_ALPHA_UINT16:
      return PIPE_FORMAT_L16A16_UINT;
   case MESA_FORMAT_LUMINANCE_ALPHA_UINT32:
      return PIPE_FORMAT_L32A32_UINT;

   case MESA_FORMAT_LUMINANCE_ALPHA_INT8:
      return PIPE_FORMAT_L8A8_SINT;
   case MESA_FORMAT_LUMINANCE_ALPHA_INT16:
      return PIPE_FORMAT_L16A16_SINT;
   case MESA_FORMAT_LUMINANCE_ALPHA_INT32:
      return PIPE_FORMAT_L32A32_SINT;

   case MESA_FORMAT_R_INT8:
      return PIPE_FORMAT_R8_SINT;
   case MESA_FORMAT_RG_INT8:
      return PIPE_FORMAT_R8G8_SINT;
   case MESA_FORMAT_RGB_INT8:
      return PIPE_FORMAT_R8G8B8_SINT;
   case MESA_FORMAT_RGBA_INT8:
      return PIPE_FORMAT_R8G8B8A8_SINT;
   case MESA_FORMAT_R_INT16:
      return PIPE_FORMAT_R16_SINT;
   case MESA_FORMAT_RG_INT16:
      return PIPE_FORMAT_R16G16_SINT;
   case MESA_FORMAT_RGB_INT16:
      return PIPE_FORMAT_R16G16B16_SINT;
   case MESA_FORMAT_RGBA_INT16:
      return PIPE_FORMAT_R16G16B16A16_SINT;
   case MESA_FORMAT_R_INT32:
      return PIPE_FORMAT_R32_SINT;
   case MESA_FORMAT_RG_INT32:
      return PIPE_FORMAT_R32G32_SINT;
   case MESA_FORMAT_RGB_INT32:
      return PIPE_FORMAT_R32G32B32_SINT;
   case MESA_FORMAT_RGBA_INT32:
      return PIPE_FORMAT_R32G32B32A32_SINT;

   /* unsigned int formats */
   case MESA_FORMAT_R_UINT8:
      return PIPE_FORMAT_R8_UINT;
   case MESA_FORMAT_RG_UINT8:
      return PIPE_FORMAT_R8G8_UINT;
   case MESA_FORMAT_RGB_UINT8:
      return PIPE_FORMAT_R8G8B8_UINT;
   case MESA_FORMAT_RGBA_UINT8:
      return PIPE_FORMAT_R8G8B8A8_UINT;
   case MESA_FORMAT_R_UINT16:
      return PIPE_FORMAT_R16_UINT;
   case MESA_FORMAT_RG_UINT16:
      return PIPE_FORMAT_R16G16_UINT;
   case MESA_FORMAT_RGB_UINT16:
      return PIPE_FORMAT_R16G16B16_UINT;
   case MESA_FORMAT_RGBA_UINT16:
      return PIPE_FORMAT_R16G16B16A16_UINT;
   case MESA_FORMAT_R_UINT32:
      return PIPE_FORMAT_R32_UINT;
   case MESA_FORMAT_RG_UINT32:
      return PIPE_FORMAT_R32G32_UINT;
   case MESA_FORMAT_RGB_UINT32:
      return PIPE_FORMAT_R32G32B32_UINT;
   case MESA_FORMAT_RGBA_UINT32:
      return PIPE_FORMAT_R32G32B32A32_UINT;

   case MESA_FORMAT_RED_RGTC1:
      return PIPE_FORMAT_RGTC1_UNORM;
   case MESA_FORMAT_SIGNED_RED_RGTC1:
      return PIPE_FORMAT_RGTC1_SNORM;
   case MESA_FORMAT_RG_RGTC2:
      return PIPE_FORMAT_RGTC2_UNORM;
   case MESA_FORMAT_SIGNED_RG_RGTC2:
      return PIPE_FORMAT_RGTC2_SNORM;

   case MESA_FORMAT_L_LATC1:
      return PIPE_FORMAT_LATC1_UNORM;
   case MESA_FORMAT_SIGNED_L_LATC1:
      return PIPE_FORMAT_LATC1_SNORM;
   case MESA_FORMAT_LA_LATC2:
      return PIPE_FORMAT_LATC2_UNORM;
   case MESA_FORMAT_SIGNED_LA_LATC2:
      return PIPE_FORMAT_LATC2_SNORM;

   case MESA_FORMAT_ETC1_RGB8:
      return PIPE_FORMAT_ETC1_RGB8;

   /* signed normalized formats */
   case MESA_FORMAT_SIGNED_R8:
      return PIPE_FORMAT_R8_SNORM;
   case MESA_FORMAT_SIGNED_RG88_REV:
      return PIPE_FORMAT_R8G8_SNORM;
   case MESA_FORMAT_SIGNED_RGBA8888_REV:
      return PIPE_FORMAT_R8G8B8A8_SNORM;

   case MESA_FORMAT_SIGNED_A8:
      return PIPE_FORMAT_A8_SNORM;
   case MESA_FORMAT_SIGNED_L8:
      return PIPE_FORMAT_L8_SNORM;
   case MESA_FORMAT_SIGNED_AL88:
      return PIPE_FORMAT_L8A8_SNORM;
   case MESA_FORMAT_SIGNED_I8:
      return PIPE_FORMAT_I8_SNORM;

   case MESA_FORMAT_SIGNED_R16:
      return PIPE_FORMAT_R16_SNORM;
   case MESA_FORMAT_SIGNED_GR1616:
      return PIPE_FORMAT_R16G16_SNORM;
   case MESA_FORMAT_SIGNED_RGBA_16:
      return PIPE_FORMAT_R16G16B16A16_SNORM;

   case MESA_FORMAT_SIGNED_A16:
      return PIPE_FORMAT_A16_SNORM;
   case MESA_FORMAT_SIGNED_L16:
      return PIPE_FORMAT_L16_SNORM;
   case MESA_FORMAT_SIGNED_AL1616:
      return PIPE_FORMAT_L16A16_SNORM;
   case MESA_FORMAT_SIGNED_I16:
      return PIPE_FORMAT_I16_SNORM;

   case MESA_FORMAT_RGB9_E5_FLOAT:
      return PIPE_FORMAT_R9G9B9E5_FLOAT;
   case MESA_FORMAT_R11_G11_B10_FLOAT:
      return PIPE_FORMAT_R11G11B10_FLOAT;
   case MESA_FORMAT_ARGB2101010_UINT:
      return PIPE_FORMAT_B10G10R10A2_UINT;
   default:
      assert(0);
      return PIPE_FORMAT_NONE;
   }
}


/**
 * Translate Gallium format to Mesa format.
 */
gl_format
st_pipe_format_to_mesa_format(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_A8B8G8R8_UNORM:
      return MESA_FORMAT_RGBA8888;
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      return MESA_FORMAT_RGBA8888_REV;
   case PIPE_FORMAT_B8G8R8A8_UNORM:
      return MESA_FORMAT_ARGB8888;
   case PIPE_FORMAT_A8R8G8B8_UNORM:
      return MESA_FORMAT_ARGB8888_REV;
   case PIPE_FORMAT_X8B8G8R8_UNORM:
      return MESA_FORMAT_RGBX8888;
   case PIPE_FORMAT_R8G8B8X8_UNORM:
      return MESA_FORMAT_RGBX8888_REV;
   case PIPE_FORMAT_B8G8R8X8_UNORM:
      return MESA_FORMAT_XRGB8888;
   case PIPE_FORMAT_X8R8G8B8_UNORM:
      return MESA_FORMAT_XRGB8888_REV;
   case PIPE_FORMAT_B5G5R5A1_UNORM:
      return MESA_FORMAT_ARGB1555;
   case PIPE_FORMAT_B4G4R4A4_UNORM:
      return MESA_FORMAT_ARGB4444;
   case PIPE_FORMAT_B5G6R5_UNORM:
      return MESA_FORMAT_RGB565;
   case PIPE_FORMAT_B2G3R3_UNORM:
      return MESA_FORMAT_RGB332;
   case PIPE_FORMAT_B10G10R10A2_UNORM:
      return MESA_FORMAT_ARGB2101010;
   case PIPE_FORMAT_L4A4_UNORM:
      return MESA_FORMAT_AL44;
   case PIPE_FORMAT_L8A8_UNORM:
      return MESA_FORMAT_AL88;
   case PIPE_FORMAT_L16A16_UNORM:
      return MESA_FORMAT_AL1616;
   case PIPE_FORMAT_A8_UNORM:
      return MESA_FORMAT_A8;
   case PIPE_FORMAT_A16_UNORM:
      return MESA_FORMAT_A16;
   case PIPE_FORMAT_L8_UNORM:
      return MESA_FORMAT_L8;
   case PIPE_FORMAT_L16_UNORM:
      return MESA_FORMAT_L16;
   case PIPE_FORMAT_I8_UNORM:
      return MESA_FORMAT_I8;
   case PIPE_FORMAT_I16_UNORM:
      return MESA_FORMAT_I16;
   case PIPE_FORMAT_S8_UINT:
      return MESA_FORMAT_S8;

   case PIPE_FORMAT_R16G16B16A16_UNORM:
      return MESA_FORMAT_RGBA_16;

   case PIPE_FORMAT_Z16_UNORM:
      return MESA_FORMAT_Z16;
   case PIPE_FORMAT_Z32_UNORM:
      return MESA_FORMAT_Z32;
   case PIPE_FORMAT_S8_UINT_Z24_UNORM:
      return MESA_FORMAT_Z24_S8;
   case PIPE_FORMAT_X8Z24_UNORM:
      return MESA_FORMAT_Z24_X8;
   case PIPE_FORMAT_Z24X8_UNORM:
      return MESA_FORMAT_X8_Z24;
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
      return MESA_FORMAT_S8_Z24;
   case PIPE_FORMAT_Z32_FLOAT:
      return MESA_FORMAT_Z32_FLOAT;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
      return MESA_FORMAT_Z32_FLOAT_X24S8;

   case PIPE_FORMAT_YUYV:
   case PIPE_FORMAT_IYUV:
   case PIPE_FORMAT_YV12:
   case PIPE_FORMAT_YV16:
   case PIPE_FORMAT_NV12:
   case PIPE_FORMAT_NV21:
      return MESA_FORMAT_YCBCR;
   case PIPE_FORMAT_UYVY:
      return MESA_FORMAT_YCBCR_REV;
#if FEATURE_texture_s3tc
   case PIPE_FORMAT_DXT1_RGB:
      return MESA_FORMAT_RGB_DXT1;
   case PIPE_FORMAT_DXT1_RGBA:
      return MESA_FORMAT_RGBA_DXT1;
   case PIPE_FORMAT_DXT3_RGBA:
      return MESA_FORMAT_RGBA_DXT3;
   case PIPE_FORMAT_DXT5_RGBA:
      return MESA_FORMAT_RGBA_DXT5;
#if FEATURE_EXT_texture_sRGB
   case PIPE_FORMAT_DXT1_SRGB:
      return MESA_FORMAT_SRGB_DXT1;
   case PIPE_FORMAT_DXT1_SRGBA:
      return MESA_FORMAT_SRGBA_DXT1;
   case PIPE_FORMAT_DXT3_SRGBA:
      return MESA_FORMAT_SRGBA_DXT3;
   case PIPE_FORMAT_DXT5_SRGBA:
      return MESA_FORMAT_SRGBA_DXT5;
#endif
#endif

#if FEATURE_EXT_texture_sRGB
   case PIPE_FORMAT_L8A8_SRGB:
      return MESA_FORMAT_SLA8;
   case PIPE_FORMAT_L8_SRGB:
      return MESA_FORMAT_SL8;
   case PIPE_FORMAT_R8G8B8_SRGB:
      return MESA_FORMAT_SRGB8;
   case PIPE_FORMAT_A8B8G8R8_SRGB:
      return MESA_FORMAT_SRGBA8;
   case PIPE_FORMAT_B8G8R8A8_SRGB:
      return MESA_FORMAT_SARGB8;
#endif
   case PIPE_FORMAT_R32G32B32A32_FLOAT:
      return MESA_FORMAT_RGBA_FLOAT32;
   case PIPE_FORMAT_R16G16B16A16_FLOAT:
      return MESA_FORMAT_RGBA_FLOAT16;
   case PIPE_FORMAT_R32G32B32_FLOAT:
      return MESA_FORMAT_RGB_FLOAT32;
   case PIPE_FORMAT_R16G16B16_FLOAT:
      return MESA_FORMAT_RGB_FLOAT16;
   case PIPE_FORMAT_L32A32_FLOAT:
      return MESA_FORMAT_LUMINANCE_ALPHA_FLOAT32;
   case PIPE_FORMAT_L16A16_FLOAT:
      return MESA_FORMAT_LUMINANCE_ALPHA_FLOAT16;
   case PIPE_FORMAT_L32_FLOAT:
      return MESA_FORMAT_LUMINANCE_FLOAT32;
   case PIPE_FORMAT_L16_FLOAT:
      return MESA_FORMAT_LUMINANCE_FLOAT16;
   case PIPE_FORMAT_A32_FLOAT:
      return MESA_FORMAT_ALPHA_FLOAT32;
   case PIPE_FORMAT_A16_FLOAT:
      return MESA_FORMAT_ALPHA_FLOAT16;
   case PIPE_FORMAT_I32_FLOAT:
      return MESA_FORMAT_INTENSITY_FLOAT32;
   case PIPE_FORMAT_I16_FLOAT:
      return MESA_FORMAT_INTENSITY_FLOAT16;
   case PIPE_FORMAT_R32_FLOAT:
      return MESA_FORMAT_R_FLOAT32;
   case PIPE_FORMAT_R16_FLOAT:
      return MESA_FORMAT_R_FLOAT16;
   case PIPE_FORMAT_R32G32_FLOAT:
      return MESA_FORMAT_RG_FLOAT32;
   case PIPE_FORMAT_R16G16_FLOAT:
      return MESA_FORMAT_RG_FLOAT16;

   case PIPE_FORMAT_R8_UNORM:
      return MESA_FORMAT_R8;
   case PIPE_FORMAT_R16_UNORM:
      return MESA_FORMAT_R16;
   case PIPE_FORMAT_R8G8_UNORM:
      return MESA_FORMAT_GR88;
   case PIPE_FORMAT_R16G16_UNORM:
      return MESA_FORMAT_RG1616;

   case PIPE_FORMAT_A8_UINT:
      return MESA_FORMAT_ALPHA_UINT8;
   case PIPE_FORMAT_A16_UINT:
      return MESA_FORMAT_ALPHA_UINT16;
   case PIPE_FORMAT_A32_UINT:
      return MESA_FORMAT_ALPHA_UINT32;
   case PIPE_FORMAT_A8_SINT:
      return MESA_FORMAT_ALPHA_INT8;
   case PIPE_FORMAT_A16_SINT:
      return MESA_FORMAT_ALPHA_INT16;
   case PIPE_FORMAT_A32_SINT:
      return MESA_FORMAT_ALPHA_INT32;

   case PIPE_FORMAT_I8_UINT:
      return MESA_FORMAT_INTENSITY_UINT8;
   case PIPE_FORMAT_I16_UINT:
      return MESA_FORMAT_INTENSITY_UINT16;
   case PIPE_FORMAT_I32_UINT:
      return MESA_FORMAT_INTENSITY_UINT32;
   case PIPE_FORMAT_I8_SINT:
      return MESA_FORMAT_INTENSITY_INT8;
   case PIPE_FORMAT_I16_SINT:
      return MESA_FORMAT_INTENSITY_INT16;
   case PIPE_FORMAT_I32_SINT:
      return MESA_FORMAT_INTENSITY_INT32;

  case PIPE_FORMAT_L8_UINT:
      return MESA_FORMAT_LUMINANCE_UINT8;
   case PIPE_FORMAT_L16_UINT:
      return MESA_FORMAT_LUMINANCE_UINT16;
   case PIPE_FORMAT_L32_UINT:
      return MESA_FORMAT_LUMINANCE_UINT32;
   case PIPE_FORMAT_L8_SINT:
      return MESA_FORMAT_LUMINANCE_INT8;
   case PIPE_FORMAT_L16_SINT:
      return MESA_FORMAT_LUMINANCE_INT16;
   case PIPE_FORMAT_L32_SINT:
      return MESA_FORMAT_LUMINANCE_INT32;

   case PIPE_FORMAT_L8A8_UINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_UINT8;
   case PIPE_FORMAT_L16A16_UINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_UINT16;
   case PIPE_FORMAT_L32A32_UINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_UINT32;
   case PIPE_FORMAT_L8A8_SINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_INT8;
   case PIPE_FORMAT_L16A16_SINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_INT16;
   case PIPE_FORMAT_L32A32_SINT:
      return MESA_FORMAT_LUMINANCE_ALPHA_INT32;

   case PIPE_FORMAT_R8_SINT:
      return MESA_FORMAT_R_INT8;
   case PIPE_FORMAT_R8G8_SINT:
      return MESA_FORMAT_RG_INT8;
   case PIPE_FORMAT_R8G8B8_SINT:
      return MESA_FORMAT_RGB_INT8;
   case PIPE_FORMAT_R8G8B8A8_SINT:
      return MESA_FORMAT_RGBA_INT8;

   case PIPE_FORMAT_R16_SINT:
      return MESA_FORMAT_R_INT16;
   case PIPE_FORMAT_R16G16_SINT:
      return MESA_FORMAT_RG_INT16;
   case PIPE_FORMAT_R16G16B16_SINT:
      return MESA_FORMAT_RGB_INT16;
   case PIPE_FORMAT_R16G16B16A16_SINT:
      return MESA_FORMAT_RGBA_INT16;

   case PIPE_FORMAT_R32_SINT:
      return MESA_FORMAT_R_INT32;
   case PIPE_FORMAT_R32G32_SINT:
      return MESA_FORMAT_RG_INT32;
   case PIPE_FORMAT_R32G32B32_SINT:
      return MESA_FORMAT_RGB_INT32;
   case PIPE_FORMAT_R32G32B32A32_SINT:
      return MESA_FORMAT_RGBA_INT32;

   /* unsigned int formats */
   case PIPE_FORMAT_R8_UINT:
      return MESA_FORMAT_R_UINT8;
   case PIPE_FORMAT_R8G8_UINT:
      return MESA_FORMAT_RG_UINT8;
   case PIPE_FORMAT_R8G8B8_UINT:
      return MESA_FORMAT_RGB_UINT8;
   case PIPE_FORMAT_R8G8B8A8_UINT:
      return MESA_FORMAT_RGBA_UINT8;

   case PIPE_FORMAT_R16_UINT:
      return MESA_FORMAT_R_UINT16;
   case PIPE_FORMAT_R16G16_UINT:
      return MESA_FORMAT_RG_UINT16;
   case PIPE_FORMAT_R16G16B16_UINT:
      return MESA_FORMAT_RGB_UINT16;
   case PIPE_FORMAT_R16G16B16A16_UINT:
      return MESA_FORMAT_RGBA_UINT16;

   case PIPE_FORMAT_R32_UINT:
      return MESA_FORMAT_R_UINT32;
   case PIPE_FORMAT_R32G32_UINT:
      return MESA_FORMAT_RG_UINT32;
   case PIPE_FORMAT_R32G32B32_UINT:
      return MESA_FORMAT_RGB_UINT32;
   case PIPE_FORMAT_R32G32B32A32_UINT:
      return MESA_FORMAT_RGBA_UINT32;

   case PIPE_FORMAT_RGTC1_UNORM:
      return MESA_FORMAT_RED_RGTC1;
   case PIPE_FORMAT_RGTC1_SNORM:
      return MESA_FORMAT_SIGNED_RED_RGTC1;
   case PIPE_FORMAT_RGTC2_UNORM:
      return MESA_FORMAT_RG_RGTC2;
   case PIPE_FORMAT_RGTC2_SNORM:
      return MESA_FORMAT_SIGNED_RG_RGTC2;

   case PIPE_FORMAT_LATC1_UNORM:
      return MESA_FORMAT_L_LATC1;
   case PIPE_FORMAT_LATC1_SNORM:
      return MESA_FORMAT_SIGNED_L_LATC1;
   case PIPE_FORMAT_LATC2_UNORM:
      return MESA_FORMAT_LA_LATC2;
   case PIPE_FORMAT_LATC2_SNORM:
      return MESA_FORMAT_SIGNED_LA_LATC2;

   case PIPE_FORMAT_ETC1_RGB8:
      return MESA_FORMAT_ETC1_RGB8;

   /* signed normalized formats */
   case PIPE_FORMAT_R8_SNORM:
      return MESA_FORMAT_SIGNED_R8;
   case PIPE_FORMAT_R8G8_SNORM:
      return MESA_FORMAT_SIGNED_RG88_REV;
   case PIPE_FORMAT_R8G8B8A8_SNORM:
      return MESA_FORMAT_SIGNED_RGBA8888_REV;

   case PIPE_FORMAT_A8_SNORM:
      return MESA_FORMAT_SIGNED_A8;
   case PIPE_FORMAT_L8_SNORM:
      return MESA_FORMAT_SIGNED_L8;
   case PIPE_FORMAT_L8A8_SNORM:
      return MESA_FORMAT_SIGNED_AL88;
   case PIPE_FORMAT_I8_SNORM:
      return MESA_FORMAT_SIGNED_I8;

   case PIPE_FORMAT_R16_SNORM:
      return MESA_FORMAT_SIGNED_R16;
   case PIPE_FORMAT_R16G16_SNORM:
      return MESA_FORMAT_SIGNED_GR1616;
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      return MESA_FORMAT_SIGNED_RGBA_16;

   case PIPE_FORMAT_A16_SNORM:
      return MESA_FORMAT_SIGNED_A16;
   case PIPE_FORMAT_L16_SNORM:
      return MESA_FORMAT_SIGNED_L16;
   case PIPE_FORMAT_L16A16_SNORM:
      return MESA_FORMAT_SIGNED_AL1616;
   case PIPE_FORMAT_I16_SNORM:
      return MESA_FORMAT_SIGNED_I16;

   case PIPE_FORMAT_R9G9B9E5_FLOAT:
      return MESA_FORMAT_RGB9_E5_FLOAT;
   case PIPE_FORMAT_R11G11B10_FLOAT:
      return MESA_FORMAT_R11_G11_B10_FLOAT;

   case PIPE_FORMAT_B10G10R10A2_UINT:
      return MESA_FORMAT_ARGB2101010_UINT;
   default:
      assert(0);
      return MESA_FORMAT_NONE;
   }
}
#endif


/**
 * Map GL texture formats to Gallium pipe formats.
 */
struct format_mapping
{
   GLenum glFormats[18];       /**< list of GLenum formats, 0-terminated */
   enum pipe_format pipeFormats[10]; /**< list of pipe formats, 0-terminated */
};


#define DEFAULT_RGBA_FORMATS \
      PIPE_FORMAT_B8G8R8A8_UNORM, \
      PIPE_FORMAT_A8R8G8B8_UNORM, \
      PIPE_FORMAT_A8B8G8R8_UNORM, \
      PIPE_FORMAT_B5G6R5_UNORM, \
      0

#define DEFAULT_RGB_FORMATS \
      PIPE_FORMAT_B8G8R8X8_UNORM, \
      PIPE_FORMAT_X8R8G8B8_UNORM, \
      PIPE_FORMAT_X8B8G8R8_UNORM, \
      PIPE_FORMAT_B8G8R8A8_UNORM, \
      PIPE_FORMAT_A8R8G8B8_UNORM, \
      PIPE_FORMAT_A8B8G8R8_UNORM, \
      PIPE_FORMAT_B5G6R5_UNORM, \
      0

#define DEFAULT_SRGBA_FORMATS \
      PIPE_FORMAT_B8G8R8A8_SRGB, \
      PIPE_FORMAT_A8R8G8B8_SRGB, \
      PIPE_FORMAT_A8B8G8R8_SRGB, \
      0

#define DEFAULT_DEPTH_FORMATS \
      PIPE_FORMAT_Z24X8_UNORM, \
      PIPE_FORMAT_X8Z24_UNORM, \
      PIPE_FORMAT_Z16_UNORM, \
      PIPE_FORMAT_Z24_UNORM_S8_UINT, \
      PIPE_FORMAT_S8_UINT_Z24_UNORM, \
      0

#define DEFAULT_YUV_FORMATS \
      PIPE_FORMAT_UYVY, \
      PIPE_FORMAT_YUYV, \
      PIPE_FORMAT_YV12, \
      PIPE_FORMAT_YV16, \
      PIPE_FORMAT_IYUV, \
      PIPE_FORMAT_NV12, \
      PIPE_FORMAT_NV21, 0

/**
 * This table maps OpenGL texture format enums to Gallium pipe_format enums.
 * Multiple GL enums might map to multiple pipe_formats.
 * The first pipe format in the list that's supported is the one that's chosen.
 */
static const struct format_mapping format_map[] = {
   /* Basic RGB, RGBA formats */
   {
      { GL_RGB10, GL_RGB10_A2, 0 },
      { PIPE_FORMAT_B10G10R10A2_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGBA, GL_RGBA8, 0 },
      { PIPE_FORMAT_R8G8B8A8_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_BGRA, 0 },
      { PIPE_FORMAT_B8G8R8A8_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGB, GL_RGB8, 0 },
      { PIPE_FORMAT_R8G8B8X8_UNORM, DEFAULT_RGB_FORMATS }
   },
   {
      { GL_RGB12, GL_RGB16, GL_RGBA12, GL_RGBA16, 0 },
      { PIPE_FORMAT_R16G16B16A16_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGBA4, GL_RGBA2, 0 },
      { PIPE_FORMAT_B4G4R4A4_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGB5_A1, 0 },
      { PIPE_FORMAT_B5G5R5A1_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_R3_G3_B2, 0 },
      { PIPE_FORMAT_B2G3R3_UNORM, PIPE_FORMAT_B5G6R5_UNORM,
        PIPE_FORMAT_B5G5R5A1_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGB5, GL_RGB4 },
      { PIPE_FORMAT_B5G6R5_UNORM, PIPE_FORMAT_B5G5R5A1_UNORM,
        DEFAULT_RGBA_FORMATS }
   },

   /* basic Alpha formats */
   {
      { GL_ALPHA12, GL_ALPHA16, 0 },
      { PIPE_FORMAT_A16_UNORM, PIPE_FORMAT_A8_UNORM,
        DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_ALPHA, GL_ALPHA4, GL_ALPHA8, GL_COMPRESSED_ALPHA, 0 },
      { PIPE_FORMAT_A8_UNORM, DEFAULT_RGBA_FORMATS }
   },

   /* basic Luminance formats */
   {
      { GL_LUMINANCE12, GL_LUMINANCE16, 0 },
      { PIPE_FORMAT_L16_UNORM, PIPE_FORMAT_L8_UNORM, DEFAULT_RGB_FORMATS }
   },
   {
      { GL_LUMINANCE, GL_LUMINANCE4, GL_LUMINANCE8, 0 },
      { PIPE_FORMAT_L8_UNORM, DEFAULT_RGB_FORMATS }
   },

   /* basic Luminance/Alpha formats */
   {
      { GL_LUMINANCE12_ALPHA4, GL_LUMINANCE12_ALPHA12,
        GL_LUMINANCE16_ALPHA16, 0},
      { PIPE_FORMAT_L16A16_UNORM, PIPE_FORMAT_L8A8_UNORM,
        DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_LUMINANCE_ALPHA, GL_LUMINANCE6_ALPHA2, GL_LUMINANCE8_ALPHA8, 0 },
      { PIPE_FORMAT_L8A8_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_LUMINANCE4_ALPHA4, 0 },
      { PIPE_FORMAT_L4A4_UNORM, PIPE_FORMAT_L8A8_UNORM,
        DEFAULT_RGBA_FORMATS }
   },

   /* basic Intensity formats */
   {
      { GL_INTENSITY12, GL_INTENSITY16, 0 },
      { PIPE_FORMAT_I16_UNORM, PIPE_FORMAT_I8_UNORM, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_INTENSITY, GL_INTENSITY4, GL_INTENSITY8,
        GL_COMPRESSED_INTENSITY, 0 },
      { PIPE_FORMAT_I8_UNORM, DEFAULT_RGBA_FORMATS }
   },

   /* YCbCr */
   {
      { GL_YCBCR_MESA, 0 },
      { DEFAULT_YUV_FORMATS }
   },

   /* compressed formats */ /* XXX PIPE_BIND_SAMPLER_VIEW only */
   {
      { GL_COMPRESSED_RGB, 0 },
      { PIPE_FORMAT_DXT1_RGB, DEFAULT_RGB_FORMATS }
   },
   {
      { GL_COMPRESSED_RGBA, 0 },
      { PIPE_FORMAT_DXT5_RGBA, DEFAULT_RGBA_FORMATS }
   },
   {
      { GL_RGB_S3TC, GL_RGB4_S3TC, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0 },
      { PIPE_FORMAT_DXT1_RGB, 0 }
   },
   {
      { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0 },
      { PIPE_FORMAT_DXT1_RGBA, 0 }
   },
   {
      { GL_RGBA_S3TC, GL_RGBA4_S3TC, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0 },
      { PIPE_FORMAT_DXT3_RGBA, 0 }
   },
   {
      { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0 },
      { PIPE_FORMAT_DXT5_RGBA, 0 }
   },

#if 0
   {
      { GL_COMPRESSED_RGB_FXT1_3DFX, 0 },
      { PIPE_FORMAT_RGB_FXT1, 0 }
   },
   {
      { GL_COMPRESSED_RGBA_FXT1_3DFX, 0 },
      { PIPE_FORMAT_RGBA_FXT1, 0 }
   },
#endif

   /* Depth formats */
   {
      { GL_DEPTH_COMPONENT16, 0 },
      { PIPE_FORMAT_Z16_UNORM, DEFAULT_DEPTH_FORMATS }
   },
   {
      { GL_DEPTH_COMPONENT24, 0 },
      { PIPE_FORMAT_Z24X8_UNORM, PIPE_FORMAT_X8Z24_UNORM,
        DEFAULT_DEPTH_FORMATS }
   },
   {
      { GL_DEPTH_COMPONENT32, 0 },
      { PIPE_FORMAT_Z32_UNORM, DEFAULT_DEPTH_FORMATS }
   },
   {
      { GL_DEPTH_COMPONENT, 0 },
      { DEFAULT_DEPTH_FORMATS }
   },
   {
      { GL_DEPTH_COMPONENT32F, 0 },
      { PIPE_FORMAT_Z32_FLOAT, 0 }
   },

   /* stencil formats */
   {
      { GL_STENCIL_INDEX, GL_STENCIL_INDEX1_EXT, GL_STENCIL_INDEX4_EXT,
        GL_STENCIL_INDEX8_EXT, GL_STENCIL_INDEX16_EXT, 0 },
      {
         PIPE_FORMAT_S8_UINT, PIPE_FORMAT_Z24_UNORM_S8_UINT,
         PIPE_FORMAT_S8_UINT_Z24_UNORM, 0
      }
   },

   /* Depth / Stencil formats */
   {
      { GL_DEPTH_STENCIL_EXT, GL_DEPTH24_STENCIL8_EXT, 0 },
      { PIPE_FORMAT_Z24_UNORM_S8_UINT, PIPE_FORMAT_S8_UINT_Z24_UNORM, 0 }
   },
   {
      { GL_DEPTH32F_STENCIL8, 0 },
      { PIPE_FORMAT_Z32_FLOAT_S8X24_UINT, 0 }
   },

   /* sRGB formats */
   {
      { GL_SRGB_EXT, GL_SRGB8_EXT, GL_SRGB_ALPHA_EXT, GL_SRGB8_ALPHA8_EXT, 0 },
      { DEFAULT_SRGBA_FORMATS }
   },
   {
      { GL_COMPRESSED_SRGB_EXT, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 0 },
      { PIPE_FORMAT_DXT1_SRGB, DEFAULT_SRGBA_FORMATS }
   },
   {
      { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0 },
      { PIPE_FORMAT_DXT1_SRGBA, 0 }
   },
   {
      { GL_COMPRESSED_SRGB_ALPHA_EXT,
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0 },
      { PIPE_FORMAT_DXT3_SRGBA, DEFAULT_SRGBA_FORMATS }
   },
   {
      { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0 },
      { PIPE_FORMAT_DXT5_SRGBA, 0 }
   },
   {
      { GL_SLUMINANCE_ALPHA_EXT, GL_SLUMINANCE8_ALPHA8_EXT,
        GL_COMPRESSED_SLUMINANCE_EXT, GL_COMPRESSED_SLUMINANCE_ALPHA_EXT, 0 },
      { PIPE_FORMAT_L8A8_SRGB, DEFAULT_SRGBA_FORMATS }
   },
   {
      { GL_SLUMINANCE_EXT, GL_SLUMINANCE8_EXT, 0 },
      { PIPE_FORMAT_L8_SRGB, DEFAULT_SRGBA_FORMATS }
   },

   /* 16-bit float formats */
   {
      { GL_RGBA16F_ARB, 0 },
      { PIPE_FORMAT_R16G16B16A16_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_RGB16F_ARB, 0 },
      { PIPE_FORMAT_R16G16B16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_R32G32B32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA16F_ARB, 0 },
      { PIPE_FORMAT_L16A16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_L32A32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_ALPHA16F_ARB, 0 },
      { PIPE_FORMAT_A16_FLOAT, PIPE_FORMAT_L16A16_FLOAT,
        PIPE_FORMAT_A32_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_L32A32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_INTENSITY16F_ARB, 0 },
      { PIPE_FORMAT_I16_FLOAT, PIPE_FORMAT_L16A16_FLOAT,
        PIPE_FORMAT_I32_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_L32A32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_LUMINANCE16F_ARB, 0 },
      { PIPE_FORMAT_L16_FLOAT, PIPE_FORMAT_L16A16_FLOAT,
        PIPE_FORMAT_L32_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_L32A32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_R16F, 0 },
      { PIPE_FORMAT_R16_FLOAT, PIPE_FORMAT_R16G16_FLOAT,
        PIPE_FORMAT_R32_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_R32G32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },
   {
      { GL_RG16F, 0 },
      { PIPE_FORMAT_R16G16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT,
        PIPE_FORMAT_R32G32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT, 0 }
   },

   /* 32-bit float formats */
   {
      { GL_RGBA32F_ARB, 0 },
      { PIPE_FORMAT_R32G32B32A32_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_RGB32F_ARB, 0 },
      { PIPE_FORMAT_R32G32B32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT,
        PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA32F_ARB, 0 },
      { PIPE_FORMAT_L32A32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT,
        PIPE_FORMAT_L16A16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_ALPHA32F_ARB, 0 },
      { PIPE_FORMAT_A32_FLOAT, PIPE_FORMAT_L32A32_FLOAT,
        PIPE_FORMAT_R32G32B32A32_FLOAT, PIPE_FORMAT_A16_FLOAT,
        PIPE_FORMAT_L16A16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_INTENSITY32F_ARB, 0 },
      { PIPE_FORMAT_I32_FLOAT, PIPE_FORMAT_L32A32_FLOAT,
        PIPE_FORMAT_R32G32B32A32_FLOAT, PIPE_FORMAT_I16_FLOAT,
        PIPE_FORMAT_L16A16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_LUMINANCE32F_ARB, 0 },
      { PIPE_FORMAT_L32_FLOAT, PIPE_FORMAT_L32A32_FLOAT,
        PIPE_FORMAT_R32G32B32A32_FLOAT, PIPE_FORMAT_L16_FLOAT,
        PIPE_FORMAT_L16A16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_R32F, 0 },
      { PIPE_FORMAT_R32_FLOAT, PIPE_FORMAT_R32G32_FLOAT,
        PIPE_FORMAT_R32G32B32A32_FLOAT, PIPE_FORMAT_R16_FLOAT,
        PIPE_FORMAT_R16G16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },
   {
      { GL_RG32F, 0 },
      { PIPE_FORMAT_R32G32_FLOAT, PIPE_FORMAT_R32G32B32A32_FLOAT,
        PIPE_FORMAT_R16G16_FLOAT, PIPE_FORMAT_R16G16B16A16_FLOAT, 0 }
   },

   /* R, RG formats */
   {
      { GL_RED, GL_R8, 0 },
      { PIPE_FORMAT_R8_UNORM, 0 }
   },
   {
      { GL_RG, GL_RG8, 0 },
      { PIPE_FORMAT_R8G8_UNORM, 0 }
   },
   {
      { GL_R16, 0 },
      { PIPE_FORMAT_R16_UNORM, 0 }
   },
   {
      { GL_RG16, 0 },
      { PIPE_FORMAT_R16G16_UNORM, 0 }
   },

   /* compressed R, RG formats */
   {
      { GL_COMPRESSED_RED, GL_COMPRESSED_RED_RGTC1, 0 },
      { PIPE_FORMAT_RGTC1_UNORM, PIPE_FORMAT_R8_UNORM, 0 }
   },
   {
      { GL_COMPRESSED_SIGNED_RED_RGTC1, 0 },
      { PIPE_FORMAT_RGTC1_SNORM, 0 }
   },
   {
      { GL_COMPRESSED_RG, GL_COMPRESSED_RG_RGTC2, 0 },
      { PIPE_FORMAT_RGTC2_UNORM, PIPE_FORMAT_R8G8_UNORM, 0 }
   },
   {
      { GL_COMPRESSED_SIGNED_RG_RGTC2, 0 },
      { PIPE_FORMAT_RGTC2_SNORM, 0 }
   },
   {
      { GL_COMPRESSED_LUMINANCE, GL_COMPRESSED_LUMINANCE_LATC1_EXT, 0 },
      { PIPE_FORMAT_LATC1_UNORM, PIPE_FORMAT_L8_UNORM, 0 }
   },
   {
      { GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT, 0 },
      { PIPE_FORMAT_LATC1_SNORM, 0 }
   },
   {
      { GL_COMPRESSED_LUMINANCE_ALPHA, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT,
        GL_COMPRESSED_LUMINANCE_ALPHA_3DC_ATI, 0 },
      { PIPE_FORMAT_LATC2_UNORM, PIPE_FORMAT_L8A8_UNORM, 0 }
   },
   {
      { GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT, 0 },
      { PIPE_FORMAT_LATC2_SNORM, 0 }
   },

   /* ETC1 */
   {
      { GL_ETC1_RGB8_OES, 0 },
      { PIPE_FORMAT_ETC1_RGB8, 0 }
   },

   /* signed/unsigned integer formats.
    */
   {
      { GL_RED_INTEGER_EXT,
        GL_GREEN_INTEGER_EXT,
        GL_BLUE_INTEGER_EXT,
        GL_RGBA_INTEGER_EXT,
        GL_BGRA_INTEGER_EXT,
        GL_RGBA8I_EXT, 0 },
      { PIPE_FORMAT_R8G8B8A8_SINT, 0 }
   },
   {
      { GL_RGB_INTEGER_EXT, 
        GL_BGR_INTEGER_EXT,
        GL_RGB8I_EXT, 0 },
      { PIPE_FORMAT_R8G8B8_SINT,
        PIPE_FORMAT_R8G8B8A8_SINT, 0 }
   },
   {
      { GL_ALPHA_INTEGER_EXT,
        GL_ALPHA8I_EXT, 0 },
      { PIPE_FORMAT_A8_SINT, 0 }
   },
   {
      { GL_ALPHA16I_EXT, 0 },
      { PIPE_FORMAT_A16_SINT, 0 }
   },
   {
      { GL_ALPHA32I_EXT, 0 },
      { PIPE_FORMAT_A32_SINT, 0 }
   },
   {
      { GL_ALPHA8UI_EXT, 0 },
      { PIPE_FORMAT_A8_UINT, 0 }
   },
   {
      { GL_ALPHA16UI_EXT, 0 },
      { PIPE_FORMAT_A16_UINT, 0 }
   },
   {
      { GL_ALPHA32UI_EXT, 0 },
      { PIPE_FORMAT_A32_UINT, 0 }
   },
   {
      { GL_INTENSITY8I_EXT, 0 },
      { PIPE_FORMAT_I8_SINT, 0 }
   },
   {
      { GL_INTENSITY16I_EXT, 0 },
      { PIPE_FORMAT_I16_SINT, 0 }
   },
   {
      { GL_INTENSITY32I_EXT, 0 },
      { PIPE_FORMAT_I32_SINT, 0 }
   },
   {
      { GL_INTENSITY8UI_EXT, 0 },
      { PIPE_FORMAT_I8_UINT, 0 }
   },
   {
      { GL_INTENSITY16UI_EXT, 0 },
      { PIPE_FORMAT_I16_UINT, 0 }
   },
   {
      { GL_INTENSITY32UI_EXT, 0 },
      { PIPE_FORMAT_I32_UINT, 0 }
   },
   {
      { GL_LUMINANCE8I_EXT, 0 },
      { PIPE_FORMAT_L8_SINT, 0 }
   },
   {
      { GL_LUMINANCE16I_EXT, 0 },
      { PIPE_FORMAT_L16_SINT, 0 }
   },
   {
      { GL_LUMINANCE32I_EXT, 0 },
      { PIPE_FORMAT_L32_SINT, 0 }
   },
   {
      { GL_LUMINANCE_INTEGER_EXT,
        GL_LUMINANCE8UI_EXT, 0 },
      { PIPE_FORMAT_L8_UINT, 0 }
   },
   {
      { GL_LUMINANCE16UI_EXT, 0 },
      { PIPE_FORMAT_L16_UINT, 0 }
   },
   {
      { GL_LUMINANCE32UI_EXT, 0 },
      { PIPE_FORMAT_L32_UINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA_INTEGER_EXT,
        GL_LUMINANCE_ALPHA8I_EXT, 0 },
      { PIPE_FORMAT_L8A8_SINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA16I_EXT, 0 },
      { PIPE_FORMAT_L16A16_SINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA32I_EXT, 0 },
      { PIPE_FORMAT_L32A32_SINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA8UI_EXT, 0 },
      { PIPE_FORMAT_L8A8_UINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA16UI_EXT, 0 },
      { PIPE_FORMAT_L16A16_UINT, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA32UI_EXT, 0 },
      { PIPE_FORMAT_L32A32_UINT, 0 }
   },
   {
      { GL_RGB16I_EXT, 0 },
      { PIPE_FORMAT_R16G16B16_SINT,
        PIPE_FORMAT_R16G16B16A16_SINT, 0 },
   },
   {
      { GL_RGBA16I_EXT, 0 },
      { PIPE_FORMAT_R16G16B16A16_SINT, 0 },
   },
   {
      { GL_RGB32I_EXT, 0 },
      { PIPE_FORMAT_R32G32B32_SINT,
        PIPE_FORMAT_R32G32B32A32_SINT, 0 },
   },
   {
      { GL_RGBA32I_EXT, 0 },
      { PIPE_FORMAT_R32G32B32A32_SINT, 0 }
   },
   {
      { GL_RGBA8UI_EXT, 0 },
      { PIPE_FORMAT_R8G8B8A8_UINT, 0 }
   },
   {
      { GL_RGB8UI_EXT, 0 },
      { PIPE_FORMAT_R8G8B8_UINT,
        PIPE_FORMAT_R8G8B8A8_UINT, 0 }
   },
   {
      { GL_RGB16UI_EXT, 0 },
      { PIPE_FORMAT_R16G16B16_UINT,
        PIPE_FORMAT_R16G16B16A16_UINT, 0 }
   },
   {
      { GL_RGBA16UI_EXT, 0 },
      { PIPE_FORMAT_R16G16B16A16_UINT, 0 }
   },
   {
      { GL_RGB32UI_EXT, 0},
      { PIPE_FORMAT_R32G32B32_UINT,
        PIPE_FORMAT_R32G32B32A32_UINT, 0 }
   },
   {
      { GL_RGBA32UI_EXT, 0},
      { PIPE_FORMAT_R32G32B32A32_UINT, 0 }
   },
   {
     { GL_R8I, 0},
     { PIPE_FORMAT_R8_SINT, 0},
   },
   {
     { GL_R16I, 0},
     { PIPE_FORMAT_R16_SINT, 0},
   },
   {
     { GL_R32I, 0},
     { PIPE_FORMAT_R32_SINT, 0},
   },
  {
     { GL_R8UI, 0},
     { PIPE_FORMAT_R8_UINT, 0},
   },
   {
     { GL_R16UI, 0},
     { PIPE_FORMAT_R16_UINT, 0},
   },
   {
     { GL_R32UI, 0},
     { PIPE_FORMAT_R32_UINT, 0},
   },
   {
     { GL_RG8I, 0},
     { PIPE_FORMAT_R8G8_SINT, 0},
   },
   {
     { GL_RG16I, 0},
     { PIPE_FORMAT_R16G16_SINT, 0},
   },
   {
     { GL_RG32I, 0},
     { PIPE_FORMAT_R32G32_SINT, 0},
   },
  {
     { GL_RG8UI, 0},
     { PIPE_FORMAT_R8G8_UINT, 0},
   },
   {
     { GL_RG16UI, 0},
     { PIPE_FORMAT_R16G16_UINT, 0},
   },
   {
     { GL_RG32UI, 0},
     { PIPE_FORMAT_R32G32_UINT, 0},
   },
   /* signed normalized formats */
   {
      { GL_RED_SNORM, GL_R8_SNORM, 0 },
      { PIPE_FORMAT_R8_SNORM, PIPE_FORMAT_R8G8_SNORM,
        PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_R16_SNORM, 0 },
      { PIPE_FORMAT_R16_SNORM,
        PIPE_FORMAT_R16G16_SNORM,
        PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_R8_SNORM,
        PIPE_FORMAT_R8G8_SNORM,
        PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_RG_SNORM, GL_RG8_SNORM, 0 },
      { PIPE_FORMAT_R8G8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_RG16_SNORM, 0 },
      { PIPE_FORMAT_R16G16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_R8G8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_RGB_SNORM, GL_RGB8_SNORM, GL_RGBA_SNORM, GL_RGBA8_SNORM, 0 },
      { PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_RGB16_SNORM, GL_RGBA16_SNORM, 0 },
      { PIPE_FORMAT_R16G16B16A16_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_ALPHA_SNORM, GL_ALPHA8_SNORM, 0 },
      { PIPE_FORMAT_A8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_ALPHA16_SNORM, 0 },
      { PIPE_FORMAT_A16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_A8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_LUMINANCE_SNORM, GL_LUMINANCE8_SNORM, 0 },
      { PIPE_FORMAT_L8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_LUMINANCE16_SNORM, 0 },
      { PIPE_FORMAT_L16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_L8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_LUMINANCE_ALPHA_SNORM, GL_LUMINANCE8_ALPHA8_SNORM, 0 },
      { PIPE_FORMAT_L8A8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_LUMINANCE16_ALPHA16_SNORM, 0 },
      { PIPE_FORMAT_L16A16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_L8A8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_INTENSITY_SNORM, GL_INTENSITY8_SNORM, 0 },
      { PIPE_FORMAT_I8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_INTENSITY16_SNORM, 0 },
      { PIPE_FORMAT_I16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM,
        PIPE_FORMAT_I8_SNORM, PIPE_FORMAT_R8G8B8A8_SNORM, 0 }
   },
   {
      { GL_RGB9_E5, 0 },
      { PIPE_FORMAT_R9G9B9E5_FLOAT, 0 }
   },
   {
      { GL_R11F_G11F_B10F, 0 },
      { PIPE_FORMAT_R11G11B10_FLOAT, 0 }
   },
   {
      { GL_RGB10_A2UI, 0 },
      { PIPE_FORMAT_B10G10R10A2_UINT, 0 }
   },
};


/**
 * Return first supported format from the given list.
 */
static enum pipe_format
find_supported_format(struct pipe_screen *screen,
                      const enum pipe_format formats[],
                      enum pipe_texture_target target,
                      unsigned sample_count,
                      unsigned tex_usage)
{
   uint i;
   for (i = 0; formats[i]; i++) {
      if (screen->is_format_supported(screen, formats[i], target,
                                      sample_count, tex_usage)) {
         return formats[i];
      }
   }
   return PIPE_FORMAT_NONE;
}

struct exact_format_mapping
{
   GLenum format;
   GLenum type;
   enum pipe_format pformat;
};

static const struct exact_format_mapping rgba8888_tbl[] =
{
   { GL_RGBA,     GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_A8B8G8R8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_A8B8G8R8_UNORM },
   { GL_RGBA,     GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_R8G8B8A8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_R8G8B8A8_UNORM },
   { GL_BGRA,     GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_A8R8G8B8_UNORM },
   { GL_BGRA,     GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_B8G8R8A8_UNORM },
   { GL_RGBA,     GL_UNSIGNED_BYTE,               PIPE_FORMAT_R8G8B8A8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_BYTE,               PIPE_FORMAT_A8B8G8R8_UNORM },
   { GL_BGRA,     GL_UNSIGNED_BYTE,               PIPE_FORMAT_B8G8R8A8_UNORM },
   { 0,           0,                              0                          }
};

static const struct exact_format_mapping rgbx8888_tbl[] =
{
   { GL_BGRA,     GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_X8R8G8B8_UNORM },
   { GL_BGRA,     GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_B8G8R8X8_UNORM },
   { GL_BGRA,     GL_UNSIGNED_BYTE,               PIPE_FORMAT_B8G8R8X8_UNORM },
   /* No Mesa formats for these Gallium formats:
   { GL_RGBA,     GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_X8B8G8R8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_X8B8G8R8_UNORM },
   { GL_RGBA,     GL_UNSIGNED_INT_8_8_8_8_REV,    PIPE_FORMAT_R8G8B8X8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT_8_8_8_8,        PIPE_FORMAT_R8G8B8X8_UNORM },
   { GL_RGBA,     GL_UNSIGNED_BYTE,               PIPE_FORMAT_R8G8B8X8_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_BYTE,               PIPE_FORMAT_X8B8G8R8_UNORM },
   */
   { 0,           0,                              0                          }
};

static const struct exact_format_mapping rgba1010102_tbl[] =
{
   { GL_BGRA,     GL_UNSIGNED_INT_2_10_10_10_REV, PIPE_FORMAT_B10G10R10A2_UNORM },
   /* No Mesa formats for these Gallium formats:
   { GL_RGBA,     GL_UNSIGNED_INT_2_10_10_10_REV, PIPE_FORMAT_R10G10B10A2_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT_10_10_10_2,     PIPE_FORMAT_R10G10B10A2_UNORM },
   { GL_ABGR_EXT, GL_UNSIGNED_INT,                PIPE_FORMAT_R10G10B10A2_UNORM },
   */
   { 0,           0,                              0                             }
};

/**
 * If there is an exact pipe_format match for {internalFormat, format, type}
 * return that, otherwise return PIPE_FORMAT_NONE so we can do fuzzy matching.
 */
static enum pipe_format
find_exact_format(GLint internalFormat, GLenum format, GLenum type)
{
   uint i;
   const struct exact_format_mapping* tbl;

   if (format == GL_NONE || type == GL_NONE)
      return PIPE_FORMAT_NONE;

   switch (internalFormat) {
   case 4:
   case GL_RGBA:
   case GL_RGBA8:
      tbl = rgba8888_tbl;
      break;
   case 3:
   case GL_RGB:
   case GL_RGB8:
      tbl = rgbx8888_tbl;
      break;
   case GL_RGB10_A2:
      tbl = rgba1010102_tbl;
      break;
   default:
      return PIPE_FORMAT_NONE;
   }

   for (i = 0; tbl[i].format; i++)
      if (tbl[i].format == format && tbl[i].type == type)
         return tbl[i].pformat;

   return PIPE_FORMAT_NONE;
}

/**
 * Given an OpenGL internalFormat value for a texture or surface, return
 * the best matching PIPE_FORMAT_x, or PIPE_FORMAT_NONE if there's no match.
 * This is called during glTexImage2D, for example.
 *
 * The bindings parameter typically has PIPE_BIND_SAMPLER_VIEW set, plus
 * either PIPE_BINDING_RENDER_TARGET or PIPE_BINDING_DEPTH_STENCIL if
 * we want render-to-texture ability.
 *
 * \param internalFormat  the user value passed to glTexImage2D
 * \param target  one of PIPE_TEXTURE_x
 * \param bindings  bitmask of PIPE_BIND_x flags.
 */
enum pipe_format
rsxgl_choose_format(struct pipe_screen *screen, GLenum internalFormat,
		    GLenum format, GLenum type,
		    enum pipe_texture_target target, unsigned sample_count,
		    unsigned bindings)
{
#if 0
   GET_CURRENT_CONTEXT(ctx); /* XXX this should be a function parameter */
#endif
   int i, j;
   enum pipe_format pf;

#if 0
   /* can't render to compressed formats at this time */
   if (_mesa_is_compressed_format(ctx, internalFormat)
       && (bindings & ~PIPE_BIND_SAMPLER_VIEW)) {
      return PIPE_FORMAT_NONE;
   }
#endif

   /* search for exact matches */
   pf = find_exact_format(internalFormat, format, type);
   if (pf != PIPE_FORMAT_NONE &&
       screen->is_format_supported(screen, pf,
                                   target, sample_count, bindings))
      return pf;

   /* search table for internalFormat */
   for (i = 0; i < Elements(format_map); i++) {
      const struct format_mapping *mapping = &format_map[i];
      for (j = 0; mapping->glFormats[j]; j++) {
         if (mapping->glFormats[j] == internalFormat) {
            /* Found the desired internal format.  Find first pipe format
             * which is supported by the driver.
             */
            return find_supported_format(screen, mapping->pipeFormats,
                                         target, sample_count, bindings);
         }
      }
   }

#if 0
   _mesa_problem(NULL, "unhandled format!\n");
#endif
   return PIPE_FORMAT_NONE;
}

GLenum
rsxgl_format_internalformat(enum pipe_format pformat)
{
  int i, j;

  for(i = 0;i < Elements(format_map);++i) {
    const struct format_mapping *mapping = &format_map[i];
    if(mapping -> pipeFormats[0] == pformat) {
      return mapping -> glFormats[0];
    }
  }

  return GL_NONE;
}

int
rsxgl_get_format_color_bit_depth(enum pipe_format pformat,int _channel)
{
  if(!util_format_is_depth_or_stencil(pformat)) {
    const struct util_format_description * desc = util_format_description(pformat);
    if(desc != 0) {
      int channel = desc -> swizzle[_channel];
      if(desc -> layout == UTIL_FORMAT_LAYOUT_PLAIN && channel < desc -> nr_channels) {
	return desc -> channel[channel].size;
      }
    }
  }
  return 0;
}

int
rsxgl_get_format_depth_bit_depth(enum pipe_format pformat,int _channel)
{
  if(util_format_is_depth_or_stencil(pformat)) {
    const struct util_format_description * desc = util_format_description(pformat);
    if(desc != 0) {
      int channel = desc -> swizzle[_channel];
      if(desc -> layout == UTIL_FORMAT_LAYOUT_PLAIN && channel < desc -> nr_channels) {
	return desc -> channel[channel].size;
      }
    }
  }
  return 0;
}

enum pipe_format
rsxgl_choose_source_format(GLenum format,GLenum type)
{
  if(format == GL_RED) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_R8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_R8_SNORM;
    }
    else if(type == GL_UNSIGNED_SHORT) {
      return PIPE_FORMAT_R16_UNORM;
    }
    else if(type == GL_SHORT) {
      return PIPE_FORMAT_R16_SNORM;
    }
    else if(type == GL_UNSIGNED_INT) {
      return PIPE_FORMAT_R32_UNORM;
    }
    else if(type == GL_INT) {
      return PIPE_FORMAT_R32_SNORM;
    }
    else if(type == GL_FLOAT) {
      return PIPE_FORMAT_R32_FLOAT;
    }
  }
  else if(format == GL_RG) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_R8G8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_R8G8_SNORM;
    }
    else if(type == GL_UNSIGNED_SHORT) {
      return PIPE_FORMAT_R16G16_UNORM;
    }
    else if(type == GL_SHORT) {
      return PIPE_FORMAT_R16G16_SNORM;
    }
    else if(type == GL_UNSIGNED_INT) {
      return PIPE_FORMAT_R32G32_UNORM;
    }
    else if(type == GL_INT) {
      return PIPE_FORMAT_R32G32_SNORM;
    }
    else if(type == GL_FLOAT) {
      return PIPE_FORMAT_R32G32_FLOAT;
    }
  }
  else if(format == GL_RGB) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_R8G8B8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_R8G8B8_SNORM;
    }
    else if(type == GL_UNSIGNED_SHORT) {
      return PIPE_FORMAT_R16G16B16_UNORM;
    }
    else if(type == GL_SHORT) {
      return PIPE_FORMAT_R16G16B16_SNORM;
    }
    else if(type == GL_UNSIGNED_INT) {
      return PIPE_FORMAT_R32G32B32_UNORM;
    }
    else if(type == GL_INT) {
      return PIPE_FORMAT_R32G32B32_SNORM;
    }
    else if(type == GL_FLOAT) {
      return PIPE_FORMAT_R32G32B32_FLOAT;
    }
  }
  else if(format == GL_BGR) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_X8R8G8B8_UNORM;
    }    
  }
  else if(format == GL_RGBA) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_R8G8B8A8_UNORM;
    }
    if(type == GL_UNSIGNED_INT_8_8_8_8) {
      return PIPE_FORMAT_A8B8G8R8_UNORM;
    }
    if(type == GL_UNSIGNED_INT_8_8_8_8) {
      return PIPE_FORMAT_R8G8B8A8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_R8G8B8A8_SNORM;
    }
    else if(type == GL_UNSIGNED_SHORT) {
      return PIPE_FORMAT_R16G16B16A16_UNORM;
    }
    else if(type == GL_SHORT) {
      return PIPE_FORMAT_R16G16B16A16_SNORM;
    }
    else if(type == GL_UNSIGNED_INT) {
      return PIPE_FORMAT_R32G32B32A32_UNORM;
    }
    else if(type == GL_INT) {
      return PIPE_FORMAT_R32G32B32A32_SNORM;
    }
    else if(type == GL_FLOAT) {
      return PIPE_FORMAT_R32G32B32A32_FLOAT;
    }
  }
  else if(format == GL_BGRA) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_A8R8G8B8_UNORM;
    }
  }
  else if(format == GL_ALPHA) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_A8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_A8_SNORM;
    }
  }
  else if(format == GL_LUMINANCE) {
    if(type == GL_UNSIGNED_BYTE) {
      return PIPE_FORMAT_L8_UNORM;
    }
    else if(type == GL_BYTE) {
      return PIPE_FORMAT_L8_SNORM;
    }
  }

  return PIPE_FORMAT_NONE;
}
