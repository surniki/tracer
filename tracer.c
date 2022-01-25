
/* TODO: Link traces together if they touch at the ends. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define CHANNELS 3

enum direction {
	NORTH,
	NORTHEAST,
	EAST,
	SOUTHEAST,
	SOUTH,
	SOUTHWEST,
	WEST,
	NORTHWEST,
	NO_DIRECTION
};

struct image {
	int width, height;
	unsigned char buffer[];
};

struct color {
	unsigned char r, g, b;
};

struct pixel {
	int x, y;
	struct color color;
};

struct maybepixel {
	bool valid;
	struct pixel pixel;
};

struct pixelarray {
	int size, length;
	struct pixel array[];
};

struct tracearray {
	int size, length;
	struct pixelarray *array[];
};

struct pixelview {
	enum direction dir;
	struct maybepixel north, northeast, east, southeast, south, southwest, west, northwest;	
};

float pixeldistance(const struct pixel *p1, const struct pixel *p2);
bool intracearray(const struct tracearray *ta, int x, int y);
struct tracearray *createtracearray(int size);
int pushbacktrace(struct tracearray **ta, struct pixelarray *pa);
void destroytracearray(struct tracearray **ta);
void traceborder(const struct image *image, int x, int y, enum direction dir, const struct color *color, struct pixelarray **pa);
enum direction picknextpixel(const struct image *image, const struct pixelview *pv, const struct color *color, struct pixel *p);
void setpixelview(const struct image *image, int x, int y, enum direction dir, struct pixelview *pv);
struct pixelarray *createpixelarray(int size);
int pushbackpixel(struct pixelarray **pa, int x, int y, const struct color *color);
void destroypixelarray(struct pixelarray **pa);
void getpixel(const struct image *image, int x, int y, struct maybepixel *p);
void getpixelcolor(const struct image *image, int x, int y, struct color *color);
void setpixelcolor(struct image *image, int x, int y, const struct color *color);
void copypixel(struct pixel *target, const struct pixel *source);
enum direction borderpixel(const struct image *image, int x, int y, const struct color *color);
bool orientedborderpixel(const struct image *image, int x, int y, const struct color *color, enum direction dir);
bool colorequal(const struct color *c1, const struct color *c2);
bool inimage(const struct image *image, int x, int y);
bool inpixelarray(const struct pixelarray *pa, int x, int y);
struct image *createimage(void);
void writeimage(const struct image *image);
void destroyimage(struct image **image);
void fatalerrimpl(const char *msg, const char *func, const char *file, int line);
#define fatalerr(msg) fatalerrimpl((msg), __func__, __FILE__, __LINE__)

int main(int argc, char **argv)
{
	struct color color;
	struct color bordercolor = { .r = 0, .g = 0, .b = 0 };
	struct pixelarray *pa;
	struct tracearray *ta = createtracearray(1024);

	struct image *image = createimage();	
	
	if (!image)
		fatalerr("unable to create image");
	
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			enum direction dir = borderpixel(image, x, y, &bordercolor);
			if (dir != NO_DIRECTION && !intracearray(ta, x, y)) {
				pa = createpixelarray(1024);
				getpixelcolor(image, x, y, &color);
				pushbackpixel(&pa, x, y, &color);
				traceborder(image, x, y, dir, &bordercolor, &pa);
				pushbacktrace(&ta, pa);
			}
		}
	}
		
	/* color the pixels in each trace a specific color and print the image */
	struct color colors[] = {
		(struct color){ .r = 0, .g = 255, .b = 0 },
		(struct color){ .r = 0, .g = 0, .b = 255 },
		(struct color){ .r = 255, .g = 255, .b = 0 },
		(struct color){ .r = 255, .g = 0, .b = 255 },
		(struct color){ .r = 0, .g = 255, .b = 255 }
	};

	int colorlength = (sizeof colors) / (sizeof *colors);
	int colori = 0;

	for (int i = 0; i < ta->length; i++) {
		for (int j = 0; j < ta->array[i]->length; j++) {
			struct pixel p = ta->array[i]->array[j];
			setpixelcolor(image, p.x, p.y, colors + colori);
		}
		colori = (colori + 1) % colorlength;

	}

	fprintf(stderr, "number of traces: %d\n", ta->length);
	for (int i = 0; i < ta->length; i++) {
		fprintf(stderr, "trace %d, size %d\n", i, ta->array[i]->length);
	}

	/* corner detection */
	unsigned int tuplelength = 7;
	struct color cornercolor = { .r = 255, .g = 0, .b = 0 };
	float cornerangle = 155.0f * M_PI / 180.0f;
	for (int i = 0; i < ta->length; i++) {
		struct pixelarray *pa = ta->array[i];
		if (pa->length >= tuplelength) {
			for (int j = 0; j < pa->length - tuplelength; j++) {
				const struct pixel *p1 = pa->array + j;
				const struct pixel *p2 = pa->array + j + tuplelength/2;
				const struct pixel *p3 = pa->array + j + tuplelength - 1;
				float a = pixeldistance(p1, p3);
				float b = pixeldistance(p1, p2);
				float c = pixeldistance(p2, p3);
				float theta = acosf((b*b + c*c - a*a) / (2*b*c));
				fprintf(stderr, "theta: %f\n", theta);
				if (theta <= cornerangle) {
					setpixelcolor(image, p2->x, p2->y, &cornercolor);
					j += tuplelength/2; /* skip past the view given by the tuple */
				}
			}
		}
	}

	
	writeimage(image);
	
	for (int i = 0; i < ta->length; i++) {
		destroypixelarray(&ta->array[i]);
	}
	
	destroytracearray(&ta);
	destroyimage(&image);
	return 0;
}

void writeimage(const struct image *image)
{
	struct color color;
	printf("P3\n%d %d\n255\n", image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			getpixelcolor(image, x, y, &color);
			printf("%d %d %d ", color.r, color.g, color.b);
		}
		putc('\n', stdout);
	}
}

void getpixel(const struct image *image, int x, int y, struct maybepixel *p)
{
	if (inimage(image, x, y)) {
		p->valid = true;
		p->pixel.x = x;
		p->pixel.y = y;
		getpixelcolor(image, x, y, &p->pixel.color);
	}
	else {
		p->valid = false;
	}
}
	      
void getpixelcolor(const struct image *image, int x, int y, struct color *color)
{
	const unsigned char *p = image->buffer + CHANNELS * (y * image->width + x);
	color->r = *p++;
	color->g = *p++;
	color->b = *p;
}

void setpixelcolor(struct image *image, int x, int y, const struct color *color)
{
	unsigned char *p = image->buffer + CHANNELS * (y * image->width + x);
	*p++ = color->r;
	*p++ = color->g;
	*p   = color->b;
}

struct image *createimage(void)
{
	int width, height, r, g, b;
	width = height = r = g = b = 0;
	
	if (scanf("P3\n%d %d\n255\n", &width, &height) == EOF)
		return NULL;

	struct image *image = malloc(sizeof *image + width * height * CHANNELS);
	if (!image)
		return NULL;
	
	image->width = width;
	image->height = height;
	memset(&image->buffer, 0, width * height * CHANNELS);

	for (int i = 0; i < width * height && scanf("%d %d %d ", &r, &g, &b) != EOF; i++) {
		image->buffer[i * CHANNELS + 0] = r;
		image->buffer[i * CHANNELS + 1] = g;
		image->buffer[i * CHANNELS + 2] = b;
	}
	
	return image;
}

void destroyimage(struct image **image)
{
	assert(image);
	assert(*image);
	free(*image);
	*image = NULL;
}

void fatalerrimpl(const char *msg, const char *func, const char *file, int line)
{
	fprintf(stderr, "fatal error in function %s %s:%d: %s", func, file, line, msg);
	abort();
}

struct pixelarray *createpixelarray(int size)
{
	struct pixelarray *pa = malloc(sizeof *pa + sizeof *pa->array * size);
	if (!pa)
		return NULL;

	pa->size = size;
	pa->length = 0;
	memset(pa->array, 0, size * sizeof *pa->array);

	return pa;
}

int pushbackpixel(struct pixelarray **pa, int x, int y, const struct color *color)
{
	struct pixel p = { x, y, *color };

	if ((*pa)->length >= (*pa)->size) {
		/* resize by doubling the memory */
		struct pixelarray *temp = realloc(*pa, sizeof **pa + sizeof *(*pa)->array * (*pa)->size * 2);
		if (!temp)
			return 1;
		*pa = temp;
		/* set the newly allocated memory to zero */
		memset((*pa)->array + (*pa)->size, 0, (*pa)->size);
		(*pa)->size *= 2;
	}

	(*pa)->array[(*pa)->length++] = p;
	return 0;
}

void destroypixelarray(struct pixelarray **pa)
{
	assert(pa);
	assert(*pa);

	free(*pa);
	*pa = NULL;
}

/* NOTE: Does this need to also check if the pixel to the "right" is not the border color? */
enum direction borderpixel(const struct image *image, int x, int y, const struct color *color)
{
	struct color c;

	if (!inimage(image, x, y))
		return NO_DIRECTION;
	
	getpixelcolor(image, x, y, &c);

	if (colorequal(&c, color)) {
		/* get the north, south, east, and west pixels and test to see if they are not equal */
		struct maybepixel north, south, east, west;
		getpixel(image, x, y - 1, &north);
		getpixel(image, x, y + 1, &south);
		getpixel(image, x + 1, y, &east);
		getpixel(image, x - 1, y, &west);

		if (north.valid && !colorequal(&north.pixel.color, color))
			return WEST;
		else if (south.valid && !colorequal(&south.pixel.color, color))
			return EAST;
		else if (east.valid && !colorequal(&east.pixel.color, color))
			return NORTH;
		else if (west.valid && !colorequal(&west.pixel.color, color))
			return SOUTH;
	}

	return NO_DIRECTION;
}

bool inimage(const struct image *image, int x, int y)
{
	return x >= 0 && x <= image->width && y >= 0 && y <= image->height;
}

bool colorequal(const struct color *c1, const struct color *c2)
{
	return c1->r == c2->r && c1->g == c2->g && c1->b == c2->b;
}

bool intracearray(const struct tracearray *ta, int x, int y)
{
	for (int i = 0; i < ta->length; i++) {
		if (inpixelarray(ta->array[i], x, y)) {
			return true;
		}
	}

	return false;
}

bool inpixelarray(const struct pixelarray *pa, int x, int y)
{
	for (int i = 0; i < pa->length; i++) {
		if (pa->array[i].x == x && pa->array[i].y == y) {
			return true;
		}
	}

	return false;
}

void traceborder(const struct image *image, int x, int y, enum direction dir, const struct color *color, struct pixelarray **pa)
{
	struct pixelview pv;
	struct pixel pixel;

	int startx = x;
	int starty = y;
	
	while (dir != NO_DIRECTION) {
		setpixelview(image, x, y, dir, &pv);
		dir = picknextpixel(image, &pv, color, &pixel);
		if (dir != NO_DIRECTION) {
			x = pixel.x;
			y = pixel.y;

			if (x == startx && y == starty) {
			 	return;
			}
			 
			pushbackpixel(pa, x, y, color);
		}
	}

	bool inpixelarray(const struct pixelarray *pa, int x, int y);
}

void setpixelview(const struct image *image, int x, int y, enum direction dir, struct pixelview *pv)
{
	struct maybepixel northview[8], dirview[8];
	int rotatedindex;

	/* first, define the view of the pixels if approaching the area from the north direction */
	getpixel(image, x, y - 1, northview + 0);     /* north */
	getpixel(image, x + 1, y - 1, northview + 1); /* northeast */
	getpixel(image, x + 1, y, northview + 2);     /* east */
	getpixel(image, x + 1, y + 1, northview + 3); /* southeast */
	getpixel(image, x, y + 1, northview + 4);     /* south */
	getpixel(image, x - 1, y + 1, northview + 5); /* southwest */
	getpixel(image, x - 1, y, northview + 6);     /* west */
	getpixel(image, x - 1, y - 1, northview + 7); /* northwest */

	/* then, 'rotate' the view to correspond with the actual direction 'dir' */
	for (int i = 0; i < 8; i++) {
		dirview[i] = northview[(i + dir) % 8];
	}

	/* finally, set the given pixelview structure to the appropriate values */
	pv->dir = dir;
	pv->north = dirview[0];
	pv->northeast = dirview[1];
	pv->east = dirview[2];
	pv->southeast = dirview[3];
	pv->south = dirview[4];
	pv->southwest = dirview[5];
	pv->west = dirview[6];
	pv->northwest = dirview[7];
}

bool orientedborderpixel(const struct image *image, int x, int y, const struct color *color, enum direction dir)
{
	struct color c;
	struct maybepixel right, northright, southright;
	
	getpixelcolor(image, x, y, &c);

	if (!colorequal(color, &c))
		return false;
	
	switch (dir) {
		case NORTH:
			getpixel(image, x + 1, y, &right);
			getpixel(image, x + 1, y - 1, &northright);
			getpixel(image, x + 1, y + 1, &southright);
			break;
		case NORTHEAST:
			getpixel(image, x + 1, y + 1, &right);
			getpixel(image, x + 1, y, &northright);
			getpixel(image, x, y + 1, &southright);
			break;
		case EAST:
			getpixel(image, x, y + 1, &right);
			getpixel(image, x + 1, y + 1, &northright);
			getpixel(image, x - 1, y + 1, &southright);
			break;
		case SOUTHEAST:
			getpixel(image, x - 1, y + 1, &right);
			getpixel(image, x, y + 1, &northright);
			getpixel(image, x - 1, y, &southright);
			break;
		case SOUTH:
			getpixel(image, x - 1, y, &right);
			getpixel(image, x - 1, y + 1, &northright);
			getpixel(image, x - 1, y - 1, &southright);
			break;
		case SOUTHWEST:
			getpixel(image, x - 1, y - 1, &right);
			getpixel(image, x - 1, y, &northright);
			getpixel(image, x, y - 1, &southright);
			break;
		case WEST:
			getpixel(image, x, y - 1, &right);
			getpixel(image, x - 1, y - 1, &northright);
			getpixel(image, x + 1, y - 1, &southright);
			break;
		case NORTHWEST:
			getpixel(image, x + 1, y - 1, &right);
			getpixel(image, x, y - 1, &northright);
			getpixel(image, x + 1, y, &southright);
			break;
	}
	
	return right.valid && !colorequal(color, &right.pixel.color) ||
		northright.valid && !colorequal(color, &northright.pixel.color) ||
		southright.valid && !colorequal(color, &southright.pixel.color);
}

void copypixel(struct pixel *target, const struct pixel *source)
{
	target->x = source->x;
	target->y = source->y;
	target->color.r = source->color.r;
	target->color.g = source->color.g;
	target->color.b = source->color.b;
}

enum direction picknextpixel(const struct image *image, const struct pixelview *pv, const struct color *color, struct pixel *p)
{
	/* A valid pixelview will describe the following scene:
	 *
	 * ?   ?   ?
	 * ?   C   ? 
	 * ?   #   X
	 * 
	 * where C is the current position, # is black with certainty (since we were just there),
	 * X is off limits (since the right pixel to X is always black), and 
	 * ? are positions that should be tested to see if they are valid.
	 *
	 * Furthermore, do not turn 180 degrees and walk backwards, since that would cause
	 * unbounded iteration when tracing a 1 pixel wide line
	 *
	 * Therefore, the order that we can test each position is:
	 *
	 * 4   3   2
	 * 5   C   1 
	 * 6   #   X
	 *
	 * Once a positive is found, then choose that as the next pixel. If cases [1,6] fail, then
	 * the function returns false.
	 *
	 * Returns the global direction, not the local direction.
	 */
#define ltog(d) (((d) + pv->dir) % NO_DIRECTION)

	if (pv->east.valid &&
	    orientedborderpixel(image, pv->east.pixel.x, pv->east.pixel.y, color, ltog(EAST))) {
		    copypixel(p, &pv->east.pixel);
		    return ltog(EAST); 
	}
	else if (pv->northeast.valid &&
		 orientedborderpixel(image, pv->northeast.pixel.x, pv->northeast.pixel.y, color, ltog(NORTHEAST))) {
		copypixel(p, &pv->northeast.pixel);
		return ltog(NORTHEAST);
	}
	else if (pv->north.valid &&
		 orientedborderpixel(image, pv->north.pixel.x, pv->north.pixel.y, color, ltog(NORTH))) {
		copypixel(p, &pv->north.pixel);
		return ltog(NORTH); 
	}
	else if (pv->northwest.valid &&
		 orientedborderpixel(image, pv->northwest.pixel.x, pv->northwest.pixel.y, color, ltog(NORTHWEST))) {
		copypixel(p, &pv->northwest.pixel);
		return ltog(NORTHWEST);
	}
	else if (pv->west.valid &&
		 orientedborderpixel(image, pv->west.pixel.x, pv->west.pixel.y, color, ltog(WEST))) {
		copypixel(p, &pv->west.pixel);
		return ltog(WEST);
	}
	else if (pv->southwest.valid &&
		 orientedborderpixel(image, pv->southwest.pixel.x, pv->southwest.pixel.y, color, ltog(SOUTHWEST))) {
		copypixel(p, &pv->southwest.pixel);
		return ltog(SOUTHWEST);
	}

	return NO_DIRECTION;
#undef ltog
}

struct tracearray *createtracearray(int size)
{
	struct tracearray *ta = malloc(sizeof *ta + sizeof *ta->array * size);
	if (!ta)
		return NULL;

	ta->size = size;
	ta->length = 0;
	memset(ta->array, 0, size * sizeof *ta->array);

	return ta;
}

int pushbacktrace(struct tracearray **ta, struct pixelarray *pa)
{
	if ((*ta)->length >= (*ta)->size) {
		/* resize by doubling the memory */
		struct tracearray *temp = realloc(*ta, sizeof **ta + sizeof *(*ta)->array * (*ta)->size * 2);
		if (!temp)
			return 1;
		*ta = temp;
		/* set the newly allocated memory to zero */
		memset((*ta)->array + (*ta)->size, 0, (*ta)->size);
		(*ta)->size *= 2;
	}

	(*ta)->array[(*ta)->length++] = pa;
	return 0;

}

void destroytracearray(struct tracearray **ta)
{
	assert(ta);
	assert(*ta);

	free(*ta);
	*ta = NULL;
}

float pixeldistance(const struct pixel *p1, const struct pixel *p2)
{
	int xdiff = p1->x - p2->x;
	int ydiff = p1->y - p2->y;
	return sqrt(xdiff * xdiff + ydiff * ydiff);
}
