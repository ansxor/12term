#pragma once
// Functions for manipulating the buffer contents

#include "common.h"

// cursor movements
int cursor_up(int amount);
int cursor_down(int amount);
void cursor_right(int amount);
void cursor_left(int amount);
void cursor_to(int x, int y);
void forward_index(int amount);
void reverse_index(int amount);
void backspace(void);
void save_cursor(void);
void restore_cursor(void);
void back_tab(int n);
void forward_tab(int n);

// inserting/deleting
void put_char(Char c);
void delete_chars(int n);
void insert_blank(int n);
void delete_lines(int n);
void insert_lines(int n);
void erase_characters(int n);
void clear_region(int x1, int y1, int x2, int y2);

// scrolling
void set_scroll_region(int y1, int y2);
void scroll_down(int amount);
void scroll_up(int amount);

void set_cursor_style(int n);
void select_charset(int g, Char set);

void full_reset(void);

void switch_buffer(bool alt);





