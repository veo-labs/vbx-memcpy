#include <arm_neon.h>
#include <gst/video/video.h>
#include <stdio.h>
#include <sys/time.h>

extern void *memcpy_arm_128(void *dst, const void *src, size_t size);
extern void *memcpy_neon(void *dst, const void *src, size_t size);
extern void *memcpy_imx(void *dst, const void *src, size_t size);
extern void *memset_imx(void *s, int c, size_t n);

void *memcpy(void *dest, const void *src, size_t n)
{
	memcpy_imx(dest, src, n);
	return dest;
}

static void *memcpy_stride(uint8_t *__restrict__ dst, const uint8_t *__restrict__ src, size_t w_src, size_t h_src, size_t dst_stride)
{
	const uint8_t *s = src;
	uint8_t *d = dst;
	int h;

	for (h = 0; h < h_src; h++) {
		memcpy_imx(d, s, w_src);
		d += dst_stride;
		s += w_src;
	}
	return dst;
}

void *memset(void *s, int c, size_t n)
{
	memset_imx(s, c, n);
	return s;
}

static unsigned tv_diff(struct timeval *tv1, struct timeval *tv2)
{
	return (tv2->tv_sec - tv1->tv_sec) * 1000000 +
		(tv2->tv_usec - tv1->tv_usec);
}

static void gst_compositor_blend_nv12_pip (uint8_t *__restrict__ y_dest, uint8_t *__restrict y_src,
		uint8_t *__restrict__ uv_dest, uint8_t *__restrict__ uv_src,
		gint src_width, gint src_height, gint dest_width)
{
	struct timeval t1, t2;
	gettimeofday(&t1, NULL);
	g_warning("%s : %d x %d -> %dp frame\n", __func__, src_width, src_height, dest_width);
	memcpy_stride (y_dest, y_src, src_width, src_height, dest_width);
	memcpy_stride (uv_dest, uv_src, src_width, src_height / 2, dest_width);
	gettimeofday(&t2, NULL);
	g_warning("%s : %u us / %dp frame -> %lf fps\n", __func__, tv_diff(&t1, &t2), src_height, 1/((float)(tv_diff(&t1, &t2)) / 1000000));
}

void gst_compositor_blend_nv12 (GstVideoFrame * srcframe, gint xpos, gint ypos, gdouble src_alpha, GstVideoFrame * destframe) {
	const guint8 *b_src;
	guint8 *b_dest;
	gint b_src_width;
	gint b_src_height;
	gint xoffset = 0;
	gint yoffset = 0;
	gint src_comp_rowstride, dest_comp_rowstride;
	gint src_comp_height;
	gint src_comp_width;
	gint comp_ypos, comp_xpos;
	gint comp_yoffset, comp_xoffset;
	gint dest_width, dest_height;
	const GstVideoFormatInfo *info;
	gint src_width, src_height;

	src_width = GST_VIDEO_FRAME_WIDTH (srcframe);
	src_height = GST_VIDEO_FRAME_HEIGHT (srcframe);

	info = srcframe->info.finfo;
	dest_width = GST_VIDEO_FRAME_WIDTH (destframe);
	dest_height = GST_VIDEO_FRAME_HEIGHT (destframe);

	xpos = GST_ROUND_UP_2 (xpos);
	ypos = GST_ROUND_UP_2 (ypos);

	b_src_width = src_width;
	b_src_height = src_height;

	/* adjust src pointers for negative sizes */
	if (xpos < 0) {
		xoffset = -xpos;
		b_src_width -= -xpos;
		xpos = 0;
	}
	if (ypos < 0) {
		yoffset += -ypos;
		b_src_height -= -ypos;
		ypos = 0;
	}
	/* If x or y offset are larger then the source it's outside of the picture */
	if (xoffset > src_width || yoffset > src_height) {
		return;
	}

	/* adjust width/height if the src is bigger than dest */
	if (xpos + src_width > dest_width) {
		b_src_width = dest_width - xpos;
	}
	if (ypos + src_height > dest_height) {
		b_src_height = dest_height - ypos;
	}
	if (b_src_width < 0 || b_src_height < 0) {
		return;
	}

	g_warning("%s : %d x %d [+%d, %d]-> %d x %d frame\n", __func__, src_width, src_height, xpos, ypos, dest_width, dest_height);
	if ((xpos == 0) && (ypos == 0)) {
		if ((src_width == dest_width) && (src_height == dest_height)) {
			if (((src_width * src_height * 3 / 2) % 128) == 0) {
				struct timeval t1, t2;
				gettimeofday(&t1, NULL);
				g_warning("MEM_MOD128 : %d x %d -> %dp frame\n", src_width, src_height, dest_width);
				memcpy_arm_128(GST_VIDEO_FRAME_COMP_DATA (destframe, 0),
					GST_VIDEO_FRAME_COMP_DATA (srcframe, 0),
					(src_width * src_height * 3) / 2);
				gettimeofday(&t2, NULL);
				g_warning("%s : %u us / %dp frame -> %lf fps\n", __func__, tv_diff(&t1, &t2), src_height, 1/((float)(tv_diff(&t1, &t2)) / 1000000));
			} else {
				struct timeval t1, t2;
				gettimeofday(&t1, NULL);
				g_warning("MEM_IMX : %d x %d -> %dp frame\n", src_width, src_height, dest_width);
				memcpy_imx(GST_VIDEO_FRAME_COMP_DATA (destframe, 0),
					GST_VIDEO_FRAME_COMP_DATA (srcframe, 0),
					(src_width * src_height * 3) / 2);
				gettimeofday(&t2, NULL);
				g_warning("%s : %u us / %dp frame -> %lf fps\n", __func__, tv_diff(&t1, &t2), src_height, 1/((float)(tv_diff(&t1, &t2)) / 1000000));
			}
		} else {
			gst_compositor_blend_nv12_pip (GST_VIDEO_FRAME_COMP_DATA (destframe, 0),
				GST_VIDEO_FRAME_COMP_DATA (srcframe, 0),
				GST_VIDEO_FRAME_PLANE_DATA (destframe, 1),
				GST_VIDEO_FRAME_PLANE_DATA (srcframe, 1),
				src_width, src_height, dest_width);
		}
	}
	else {
		/* First mix Y, then UV */
		b_src = GST_VIDEO_FRAME_COMP_DATA (srcframe, 0);
		b_dest = GST_VIDEO_FRAME_COMP_DATA (destframe, 0);
		src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0);
		dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0);
		src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 0, b_src_width);
		src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, b_src_height);
		comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xpos);
		comp_ypos = (ypos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, ypos);
		comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xoffset);
		comp_yoffset = (yoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, yoffset);

		memcpy_stride (b_dest + comp_xpos + comp_ypos * dest_comp_rowstride,
			b_src + comp_xoffset + comp_yoffset * src_comp_rowstride,
			src_comp_width, src_comp_height, dest_comp_rowstride);

		b_src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 1);
		b_dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 1);
		src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 1);
		dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 1);
		src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 1, b_src_width);
		src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, b_src_height);
		comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xpos);
		comp_ypos = (ypos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 1, ypos);
		comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xoffset);
		comp_yoffset = (yoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 1, yoffset);


		memcpy_stride (b_dest + comp_xpos * 2 + comp_ypos * dest_comp_rowstride,
			b_src + comp_xoffset * 2 + comp_yoffset * src_comp_rowstride,
			2 * src_comp_width, src_comp_height, dest_comp_rowstride);
	}

}

