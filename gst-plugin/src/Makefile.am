plugin_LTLIBRARIES = libgstwma.la

libgstwma_la_SOURCES = libwma/bitstream.c libwma/fft.c libwma/mdct.c libwma/wmadeci.c libwma/wmafixed.c gstwma.c

libgstwma_la_CFLAGS = -std=c99 \
		$(GST_PLUGINS_BASE_CFLAGS) \
		$(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstwma_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
		$(GST_BASE_LIBS) $(GST_LIBS)
libgstwma_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwma_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstwma.h
