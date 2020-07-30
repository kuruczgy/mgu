#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mgu/gl.h>

static int ff_read_header(FILE *f, uint32_t *w, uint32_t *h) {
	uint32_t hdr[4];

	if (fread(hdr, sizeof(*hdr), 4, f) != 4) {
		return -1;
	}

	if (memcmp("farbfeld", hdr, sizeof("farbfeld") - 1)) {
		return -1;
	}

	*w = ntohl(hdr[2]);
	*h = ntohl(hdr[3]);

	return 0;
}

GLuint mgu_tex_mem(struct mgu_pixel *data, uint32_t size[static 2]) {
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

GLuint mgu_tex_farbfeld(const char *filename) {
	FILE *f = fopen(filename, "rb");
	if (!f) {
		return 0;
	}

	uint32_t w, h;
	if (ff_read_header(f, &w, &h) != 0) {
		return 0;
	}

	struct mgu_pixel *image = malloc(sizeof(struct mgu_pixel) * w * h);
	for (size_t i = 0; i < w * h; ++i) {
		uint16_t val[4];
		if (fread(val, sizeof(*val), 4, f) != 4) {
			free(image);
			return 0;
		}
		image[i] = (struct mgu_pixel){
			ntohs(val[0]) >> 8,
			ntohs(val[1]) >> 8,
			ntohs(val[2]) >> 8,
			ntohs(val[3]) >> 8
		};
	}
	fclose(f);

	GLuint tex = mgu_tex_mem(image, (uint32_t[]){ w, h });
	free(image);
	return tex;
}
