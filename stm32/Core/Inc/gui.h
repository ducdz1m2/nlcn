#ifndef INC_GUI_H_
#define INC_GUI_H_

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#include <stdint.h>
#include "fonts.h"

#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define GRAY        0X8430
#define BRED        0XF81F
#define GRED        0XFFE0
#define GBLUE       0X07FF
#define BROWN       0XBC40
#define BRRED       0XFC07
#define DARKBLUE    0X01CF
#define LIGHTBLUE   0X7D7C
#define GRAYBLUE    0X5458
#define DEEPPURPLE  0x6294
#define LIGHTGREEN  0X841F
#define LGRAY       0XC618
#define LGRAYBLUE   0XA651
#define LBBLUE      0X2B12
#define DEEP2PURLPLE 0x71D8
#define PASTELPINK    0xFDB8
#define PASTELGREEN   0x97F6
#define PASTELBLUE    0xAEDC
#define PASTELYELLOW  0xFFF8
#define PASTELPURPLE  0xD1B9
#define RETROGREEN    0x03E0
#define RETROBROWN    0x8A22
#define RETRORED      0xB800
#define NIGHTBLUE     0x0010
#define DARKPURPLE    0x3010
#define DEEPGRAY      0x4208
#define SUNORANGE     0xFD20
#define NEONYELLOW    0xFFE6
#define AQUA          0x04FF
#define HOTPINK       0xF81F
#define SOFTWHITE     0xEF5D
#define STONEGRAY     0xA514
#define CLOUDBLUE     0xC7FF

#define MAX_OBJECT 35
#define MAX_TEXT_LEN 50

typedef enum {
    DUCK_TYPE_NONE = 0,
    DUCK_TYPE_BUTTON,
    DUCK_TYPE_LABEL,
    DUCK_TYPE_IMAGE,
    DUCK_TYPE_APP
} DuckObjectType;

typedef enum {
    PAGE_NONE,
    PAGE_HOME,
    PAGE_FILE_BROWSER,
    PAGE_WIFI,
    PAGE_SYSTEM,
	PAGE_WIFI_MODAL,
	PAGE_CONFIG_MODAL
} PageTypeDef;

typedef enum {
    LABEL_LEFT,
    LABEL_RIGHT,
    LABEL_ABOVE,
    LABEL_BELOW,
    LABEL_CENTER
} LabelDirection;

typedef enum {
    BUTTON_A_CLICK,
    BUTTON_B_CLICK,
    J_CLICK,
    J_LEFT,
    J_RIGHT,
    J_UP,
    J_DOWN,
    OPEN_PAGE_BACK,
    OPEN_PAGE_HOME,
    OPEN_PAGE_WIFI,
    OPEN_PAGE_FILE_BROWSER,
    OPEN_PAGE_SYSTEM
} InputEvent;

typedef struct DuckObject DuckObject;
typedef struct DuckButton DuckButton;
typedef struct DuckLabel DuckLabel;
typedef struct DuckImage DuckImage;
typedef struct DuckPage DuckPage;
typedef struct DuckApp DuckApp;

typedef struct {
    void (*DrawPixel)(uint16_t x, uint16_t y, uint16_t color);
    void (*DrawLine)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void (*DrawRectangle)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void (*DrawFilledRectangle)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void (*DrawFilledRoundedRect)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color);
    void (*DrawCircle)(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
    void (*DrawFilledCircle)(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    void (*DrawTriangle)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
    void (*DrawFilledTriangle)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
    void (*DrawImage)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
    void (*WriteChar)(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
    void (*WriteString)(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);
    void (*InvertColors)(uint8_t invert);
    void (*FillScreen)(uint16_t color);
} DuckEngineDriver;

typedef struct {
    const char *name;
    void (*handler)(DuckObject*);
} ButtonHandlerMap;

void DuckEngine_BindDriver(DuckEngineDriver *driver);

struct DuckObject {
    uint16_t x, y, w, h;
    DuckObjectType type;
    uint8_t isNeedUpdate;
    void (*Render)(struct DuckObject*);
    void (*Callback)(struct DuckObject*);
};

struct DuckButton {
    DuckObject base;
    LabelDirection dir;
    uint16_t color;
    uint8_t radius;
    DuckLabel *label;
};

struct DuckLabel {
    DuckObject base;
    char text[MAX_TEXT_LEN];
    uint16_t color;
    uint16_t bgcolor;
    FontDef *font;
};

struct DuckImage {
    DuckObject base;
    const uint16_t *data;
};

struct DuckPage {
    DuckObject *storage_label;
    DuckObject *files_label;
    DuckObject *uptime_label;
    DuckObject *memory_label;
    DuckObject *wifi_label;
    DuckObject *Object[MAX_OBJECT];
    DuckObject *Nav[MAX_OBJECT];
    uint8_t n;
    uint8_t n_nav;
    uint8_t currentNav;
    PageTypeDef type;
    PageTypeDef pre;
};

struct DuckApp {
    DuckObject base;
    DuckLabel *label;
    DuckImage *image;
    uint8_t app_radius;
    uint16_t app_color;
    LabelDirection label_dir;
};

DuckObject* DuckEngine_Create_Label(uint16_t x, uint16_t y, char *text, FontDef *font, uint16_t color, uint16_t bgcolor, DuckPage *page, uint8_t isNav);
DuckObject* DuckEngine_Create_Button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *label, LabelDirection label_dir, FontDef *font, uint16_t label_color, uint8_t btn_radius, uint16_t btn_color, DuckPage *page, uint8_t isNav, void (*onPressed)(DuckObject*));
DuckObject* DuckEngine_Create_Image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data, DuckPage *page, uint8_t isNav);
DuckObject* DuckEngine_Create_App(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t app_color, uint8_t app_radius, char *label, FontDef *font, uint16_t lb_color, uint16_t lb_bgcolor, LabelDirection label_dir, uint16_t w_img, uint16_t h_img, uint16_t *data, DuckPage *page, uint8_t isNav, void (*onPressed)(DuckObject*));
void DuckEngine_SetText_Label(DuckLabel *lb, const char *text);
void DuckEngine_Init_Page(DuckPage *page, PageTypeDef type);
void DuckEngine_Add2Page(DuckPage *page, DuckObject *obj, uint8_t isNav);
void DuckEngine_Set_CurrentPage(PageTypeDef pre, PageTypeDef next);
void DuckEngine_Set_StartUpPage(DuckPage *page);
DuckPage* DuckEngine_Get_CurrentPage();
PageTypeDef DuckEngine_Get_Previous_Page(DuckPage *page);
void DuckEngine_MoveNav_Left();
void DuckEngine_MoveNav_Right();
void DuckEngine_Render();
void DuckEngine_Refresh_Screen();
void DuckEngine_Clear_Screen();
void DuckEngine_Remove_Selected_Object(DuckObject *obj);
void DuckEngine_Select_Object(DuckObject *obj);
void DuckEngineHanleInput();
void DuckEngine_Create_Page_From_UART(DuckPage *page, const char *filename);

#endif /* INC_GUI_H_ */
