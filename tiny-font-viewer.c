#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

#define HB_EXPERIMENTAL_API
#include "hb.h"
#include "hb-ot.h"

uiWindow *mainwin;
uiArea *canvas;
uiAreaHandler handler;
uiEditableCombobox *ecbox;
uiForm *entryForm;

unsigned size = 200;

hb_variation_t *vars = NULL;
unsigned num_coords = 0;

hb_font_t *font;
hb_draw_funcs_t *draw_funcs;

typedef struct
{
	uiDrawPath *path;
	uiDrawContext *context;
	uiDrawBrush *brush;
	hb_position_t x_offset;
	hb_position_t y_offset;
} draw_user_data_t;

static void _move_to(hb_position_t to_x, hb_position_t to_y, draw_user_data_t *user_data)
{
	user_data->path = uiDrawNewPath(uiDrawFillModeAlternate); // uiDrawFillModeWinding
	uiDrawPathNewFigure(user_data->path, (user_data->x_offset + to_x) / 64., -(user_data->y_offset + to_y) / 64.);
}

static void _line_to(hb_position_t to_x, hb_position_t to_y, draw_user_data_t *user_data)
{
	uiDrawPathLineTo(user_data->path, (user_data->x_offset + to_x) / 64., -(user_data->y_offset + to_y) / 64.);
}

static void _cubic_to(hb_position_t control1_x, hb_position_t control1_y,
					  hb_position_t control2_x, hb_position_t control2_y,
					  hb_position_t to_x, hb_position_t to_y, draw_user_data_t *user_data)
{
	uiDrawPathBezierTo(user_data->path, (user_data->x_offset + control1_x) / 64., -(user_data->y_offset + control1_y) / 64.,
		(user_data->x_offset + control2_x) / 64., -(user_data->y_offset + control2_y) / 64.,
		(user_data->x_offset + to_x) / 64., -(user_data->y_offset + to_y) / 64.);
}

static void _close_path(draw_user_data_t *user_data)
{
	uiDrawPathEnd(user_data->path);
	uiDrawFill(user_data->context, user_data->path, user_data->brush);
	uiDrawFreePath(user_data->path);
}

static void handlerDraw(uiAreaHandler *a, uiArea *area, uiAreaDrawParams *p)
{
	hb_font_set_variations(font, vars, num_coords);
	hb_font_set_scale(font, size * 64, size * 64);

	// fill the area with white
	uiDrawBrush brush = {0};
	brush.R = brush.G = brush.B = brush.A = 1;
	uiDrawPath *path = uiDrawNewPath(uiDrawFillModeAlternate);
	uiDrawPathAddRectangle(path, 0, 0, p->AreaWidth, p->AreaHeight);
	uiDrawPathEnd(path);
	uiDrawFill(p->Context, path, &brush);
	uiDrawFreePath(path);

	// now transform the coordinate space
	uiDrawMatrix m;
	uiDrawMatrixSetIdentity(&m);
	uiDrawMatrixTranslate(&m, 20, 20 + size);
	uiDrawTransform(p->Context, &m);

	// HarfBuzz shape and draw
	brush.R = brush.G = brush.B = 0;
	draw_user_data_t draw = {
		.context = p->Context,
		.brush = &brush
	};
	hb_buffer_t *buffer = hb_buffer_create();
	hb_buffer_add_utf8(buffer, uiEditableComboboxText(ecbox), -1, 0, -1);
	hb_buffer_guess_segment_properties(buffer);
	hb_shape(font, buffer, NULL, 0);
	unsigned length;
	hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &length);
	hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, NULL);
	hb_position_t x_advance = 0, y_advance = 0;

	for (unsigned i = 0; i < length; ++i)
	{
		draw.x_offset = x_advance + positions[i].x_offset;
		draw.y_offset = y_advance + positions[i].y_offset;
		hb_font_draw_glyph(font, infos[i].codepoint, draw_funcs, &draw);
		x_advance += positions[i].x_advance;
		y_advance += positions[i].y_advance;
	}
	hb_buffer_destroy(buffer);
}

static int onClosing(uiWindow *w, void *data)
{
	uiControlDestroy(uiControl(mainwin));
	uiQuit();
	return 0;
}

static int shouldQuit(void *data)
{
	uiControlDestroy(uiControl(mainwin));
	return 1;
}

static void onEditChange(uiEditableCombobox *ecbox, void *data)
{
	uiAreaQueueRedrawAll(canvas);
}

static void onVariationSliderChange(uiSlider *slider, void *data)
{
	((hb_variation_t *) data)->value = uiSliderValue(slider);
	uiAreaQueueRedrawAll(canvas);
}

static void onFontSizeSliderChange(uiSlider *slider, void *data)
{
	size = uiSliderValue(slider);
	uiAreaQueueRedrawAll(canvas);
}

static void openFont(char *filename) {
	for (unsigned i = 0; i < num_coords; ++i) {
		uiFormDelete(entryForm, /*TODO: ugly*/2 + i);
	}

	hb_blob_t *blob = hb_blob_create_from_file(filename);
	hb_face_t *face = hb_face_create(blob, 0);
    font = hb_font_create(face);
	hb_face_destroy(face);
	hb_blob_destroy(blob);

	num_coords = hb_ot_var_get_axis_count(face);
	vars = (hb_variation_t *) calloc(num_coords, sizeof (hb_variation_t));
	hb_ot_var_axis_info_t *var_infos = (hb_ot_var_axis_info_t *) calloc(num_coords, sizeof (hb_ot_var_axis_info_t));
	hb_ot_var_get_axis_infos(face, 0, &num_coords, var_infos);
	for (unsigned i = 0; i < num_coords; ++i) {
		vars[i].tag = var_infos[i].tag;
		vars[i].value = var_infos[i].default_value;

		uiSlider *slider = uiNewSlider(var_infos[i].min_value, var_infos[i].max_value);
		uiSliderSetValue(slider, var_infos[i].default_value);
		uiSliderOnChanged(slider, onVariationSliderChange, &vars[i]);

		char tag[5] = {0};
		hb_tag_to_string(var_infos[i].tag, tag);
		uiFormAppend(entryForm, tag, uiControl(slider), 0);
	}
	free(var_infos);
}

static void onOpenFileClicked(uiButton *b, void *data)
{
	char *filename = uiOpenFile(mainwin);
	if (filename == NULL) return;
	openFont(filename);
	uiFreeText(filename);
}

static void handlerMouseEvent(uiAreaHandler *a, uiArea *area, uiAreaMouseEvent *e) {}
static void handlerMouseCrossed(uiAreaHandler *ah, uiArea *a, int left) {}
static void handlerDragBroken(uiAreaHandler *ah, uiArea *a) {}
static int handlerKeyEvent(uiAreaHandler *ah, uiArea *a, uiAreaKeyEvent *e) { return 0; }

int main(int argc, char **argv)
{
	draw_funcs = hb_draw_funcs_create();
	hb_draw_funcs_set_move_to_func(draw_funcs, (hb_draw_move_to_func_t) _move_to);
	hb_draw_funcs_set_line_to_func(draw_funcs, (hb_draw_line_to_func_t) _line_to);
	// hb_draw_funcs_set_quadratic_to_func (draw_funcs, NULL); let's translate quad calls to cubic
	hb_draw_funcs_set_cubic_to_func(draw_funcs, (hb_draw_cubic_to_func_t) _cubic_to);
	hb_draw_funcs_set_close_path_func(draw_funcs, (hb_draw_close_path_func_t) _close_path);

	handler.Draw = handlerDraw;

	uiInitOptions o = {0};
	const char *err = uiInit(&o);
	if (err != NULL) {
		fprintf(stderr, "error initializing ui: %s\n", err);
		uiFreeInitError(err);
		return 1;
	}

	uiOnShouldQuit(shouldQuit, NULL);

	mainwin = uiNewWindow("HarfBuzz demo app", 640, 480, 1);
	uiWindowSetMargined(mainwin, 1);
	uiWindowOnClosing(mainwin, onClosing, NULL);

	uiBox *hbox = uiNewHorizontalBox();
	uiBoxSetPadded(hbox, 1);
	uiWindowSetChild(mainwin, uiControl(hbox));

	uiBox *vbox = uiNewVerticalBox();
	uiBoxSetPadded(vbox, 1);
	uiBoxAppend(hbox, uiControl(vbox), 0);

	uiGroup *group = uiNewGroup("Configurations");
	uiGroupSetMargined(group, 1);
	uiBoxAppend(vbox, uiControl(group), 0);

	entryForm = uiNewForm();
	uiFormSetPadded(entryForm, 1);
	uiGroupSetChild(group, uiControl(entryForm));

	ecbox = uiNewEditableCombobox();
	uiEditableComboboxSetText(ecbox, "Sample Text");
	uiEditableComboboxAppend(ecbox, "Sample Text");
	uiEditableComboboxAppend(ecbox, "مَََََتنِ آزمایشی");
	uiEditableComboboxOnChanged(ecbox, onEditChange, NULL);
	uiFormAppend(entryForm, "Text", uiControl(ecbox), 0);

	uiSlider *slider = uiNewSlider(12, 400);
	uiSliderSetValue(slider, size);
	uiSliderOnChanged(slider, onFontSizeSliderChange, NULL);
	uiFormAppend(entryForm, "Font Size", uiControl(slider), 0);

	openFont(argc > 1 ? argv[1] : "");

	uiButton *button = uiNewButton("Browse Font Files");
	uiButtonOnClicked(button, onOpenFileClicked, NULL);
	uiBoxAppend(vbox, uiControl(button), 0);

	handler.Draw = handlerDraw;
	handler.MouseEvent = handlerMouseEvent;
	handler.MouseCrossed = handlerMouseCrossed;
	handler.DragBroken = handlerDragBroken;
	handler.KeyEvent = handlerKeyEvent;
	canvas = uiNewArea(&handler);
	uiBoxAppend(hbox, uiControl(canvas), 1);

	uiControlShow(uiControl(mainwin));
	uiMain();
	uiUninit();

	free(vars);
	hb_draw_funcs_destroy(draw_funcs);
	hb_font_destroy(font);
	return 0;
}