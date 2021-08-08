#pragma once
#include "buffer.h"

typedef struct Settings {
	RGBColor palette[16];
	RGBColor cursorColor;
	RGBColor foreground;
	RGBColor background;
	int cursorShape;
	int width;
	int height;
	char* faceName;
	float faceSize;
	char* hyperlinkCommand;
	char* termName;
	int saveLines;
} Settings;

extern Settings settings;

void load_settings(int* argc, char** argv);
