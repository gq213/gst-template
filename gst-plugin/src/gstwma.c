/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2016 gq213 <gaoqiang1211@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-wma
 *
 * WMA audio decoder.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=music.wma ! asfdemux ! wma ! audioconvert ! audioresample ! autoaudiosink
 * ]| Decode and play the wma file.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "libwma/asf.h"
#include "gstwma.h"
#include <gst/audio/audio.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC (wma_debug);
#define GST_CAT_DEFAULT wma_debug



static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-wma, "
        "wmaversion = (int) [ 1, 2 ], "
        "depth = (int) 16, "
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
    );



static gboolean gst_wma_start (GstAudioDecoder * dec);
static gboolean gst_wma_stop (GstAudioDecoder * dec);
static gboolean gst_wma_set_format (GstAudioDecoder * dec, GstCaps * caps);
static GstFlowReturn gst_wma_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);



#define gst_wma_parent_class parent_class
G_DEFINE_TYPE (Gstwma, gst_wma, GST_TYPE_AUDIO_DECODER);



/* initialize the wma's class */
static void
gst_wma_class_init (GstwmaClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_details_simple(element_class, "WMA audio decoder",
    "Codec/Decoder/Audio",
    "fixed point wma decoder",
    "gq213 <gaoqiang1211@gmail.com>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_wma_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_wma_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_wma_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_wma_handle_frame);
}

static void
gst_wma_init (Gstwma * wma)
{
  GstAudioDecoder *dec;

  dec = GST_AUDIO_DECODER (wma);
  gst_audio_decoder_set_tolerance (dec, 20 * GST_MSECOND);
}



static gboolean
gst_wma_start (GstAudioDecoder * dec)
{
  GST_WARNING_OBJECT (dec, "start");

  Gstwma *wma = GST_WMA (dec);

  wma->rate_last = 0;
  wma->channels_last = 0;

  WMADecodeContext *wma_context;
  wma_context = malloc(sizeof(WMADecodeContext));
  memset(wma_context, 0, sizeof(WMADecodeContext));
  wma->context=wma_context;

  return TRUE;
}

static gboolean
gst_wma_stop (GstAudioDecoder * dec)
{
  GST_WARNING_OBJECT (dec, "stop");

  Gstwma *wma = GST_WMA (dec);

  WMADecodeContext *wma_context = wma->context;
  libwma_decode_end(wma_context);

  free(wma_context->extradata);
  free(wma_context);

  return TRUE;
}

static gboolean
gst_wma_set_format (GstAudioDecoder * dec, GstCaps * caps)
{
/*
caps = "audio/x-wma\,\ wmaversion\=\(int\)2\,\ bitrate\=\(int\)64024\,\ depth\=\(int\)16\,\ rate\=\(int\)44100\,\ channels\=\(int\)2\,\ block_align\=\(int\)2973\,\ codec_data\=\(buffer\)008800000f00752e0000"
*/
  //GST_WARNING_OBJECT (dec, "wma caps: %" GST_PTR_FORMAT, caps);

  Gstwma *wma = GST_WMA (dec);
  WMADecodeContext *wma_context = wma->context;

  gint wmaversion, bitrate, rate, channels, block_align;
  GstStructure *str = gst_caps_get_structure (caps, 0);
  if (gst_structure_get_int (str, "wmaversion", &wmaversion) &&
      gst_structure_get_int (str, "bitrate", &bitrate) &&
      gst_structure_get_int (str, "rate", &rate) &&
      gst_structure_get_int (str, "channels", &channels) &&
      gst_structure_get_int (str, "block_align", &block_align))
  {
    /*GST_WARNING_OBJECT (wma, "wma init: sample_rate=%d, nb_channels=%d, bit_rate=%d, block_align=%d, codec_id=%d", 
      rate, channels, bitrate, block_align, wmaversion);*/

      wma_context->sample_rate = rate;
      wma_context->nb_channels = channels;
      wma_context->bit_rate = bitrate;
      wma_context->block_align = block_align;
      if(wmaversion == 2) wma_context->codec_id = ASF_CODEC_ID_WMAV2;
      else wma_context->codec_id = ASF_CODEC_ID_WMAV1;
  } else {
    GST_ERROR_OBJECT (wma, "get param fail");
    return FALSE;
  }

  const GValue *value;
  if ((value = gst_structure_get_value (str, "codec_data"))) {

    GstBuffer *buf;
    buf = gst_value_get_buffer (value);
    g_return_val_if_fail (buf != NULL, FALSE);

    GstMapInfo map;
    gst_buffer_map (buf, &map, GST_MAP_READ);

    /* alloc extra data */
    wma_context->extradata = malloc(map.size);
    wma_context->extradata_size = map.size;
    memcpy(wma_context->extradata, map.data, map.size);

    gst_buffer_unmap (buf, &map);

    /*printf("extradata:");
    for(int i=0; i<wma_context->extradata_size; i++)
      printf(" 0x%02x", wma_context->extradata[i]);
    printf("\n");*/
  }

  /* open it */
  if (libwma_decode_init(wma_context) < 0) {
    GST_ERROR_OBJECT (wma, "libwma init fail");
    return FALSE;
  }

  wma->rate = rate;
  wma->channels = channels;

  GST_WARNING_OBJECT (wma, "libwma init ok");
  return TRUE;
}

static GstFlowReturn
gst_wma_handle_frame (GstAudioDecoder * dec, GstBuffer * buffer)
{
  //GST_WARNING_OBJECT (dec, "handle_frame");

  /* no fancy draining */
  if (G_UNLIKELY (!buffer))
    return GST_FLOW_OK;

  Gstwma *wma = GST_WMA (dec);
  WMADecodeContext *wma_context = wma->context;

  signed short *pcm_buf = NULL;
  int pcm_len = 0;

  GstMapInfo map;
  gsize input_size;
  guchar *input_data;
  gst_buffer_map (buffer, &map, GST_MAP_READ);
  input_data = map.data;
  input_size = map.size;

  int ret = libwma_decode_superframe(wma_context, &pcm_buf, &pcm_len, input_data, input_size);

	gst_buffer_unmap (buffer, &map);
	buffer = NULL;

	/* only set caps if they weren't already set for this continuous stream */
	if (!gst_pad_has_current_caps (GST_AUDIO_DECODER_SRC_PAD (wma))
      		|| wma->channels_last != wma->channels
			|| wma->rate_last != wma->rate)
	{
		//printf("set caps.......................................................\n");

		GstAudioInfo info;
		gst_audio_info_init (&info);

		gst_audio_info_set_format (&info,
		GST_AUDIO_FORMAT_S16, wma->rate, wma->channels, NULL);

		if(gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (wma), &info) != TRUE)
			return GST_FLOW_ERROR;

		wma->channels_last = wma->channels;
		wma->rate_last = wma->rate;
	}

	if(ret <= 0)
	{
		/* give up on frame and bail out */
		gst_audio_decoder_finish_frame (dec, NULL, 1);
		GST_ERROR_OBJECT (wma, "decoding error: %d", ret);
		return GST_FLOW_OK;
	}

	GstBuffer *outbuffer;
	outbuffer = gst_buffer_new_allocate (NULL, pcm_len, NULL);
	GstMapInfo outmap;
	gst_buffer_map (outbuffer, &outmap, GST_MAP_WRITE);

	//printf("pcm_buf(%p), pcm_len(%d).\n", pcm_buf, pcm_len);
	//printf("copy...\n");
	memcpy (outmap.data, (guchar *)pcm_buf, outmap.size);
	//free pcm_buf
	//printf("free...\n");
	free(pcm_buf);
	//printf("free ok.\n");
	//while(1);

	gst_buffer_unmap (outbuffer, &outmap);

	ret = gst_audio_decoder_finish_frame (dec, outbuffer, 1);

	return ret;
}



static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (wma_debug, "wma", 0, "Rockbox-based wma decoding");

  return gst_element_register (plugin, "wma", GST_RANK_PRIMARY, GST_TYPE_WMA);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    wma,
    "Rockbox-based WMA decoder library",
    plugin_init, VERSION, "GPL", PACKAGE_NAME, PACKAGE_URL)
