#include "gui.h"
#include "st7789.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart3;

static DuckButton ListButton[MAX_OBJECT];
static DuckLabel ListLabel[MAX_OBJECT];
static DuckImage ListImage[MAX_OBJECT];
static DuckApp ListApp[MAX_OBJECT];
static DuckPage *ListPage[MAX_OBJECT];

static uint8_t AppIndex = 0;
static uint8_t PageIndex = 0;
static uint8_t ButtonIndex = 0;
static uint8_t LabelIndex = 0;
static uint8_t ImageIndex = 0;
static uint8_t currentPageIndex = 0;

extern DuckEngineDriver *duckDriver;

void DuckEngine_BindDriver(DuckEngineDriver *driver) {
    duckDriver = driver;
}

void DuckEngine_Init_Page(DuckPage *page, PageTypeDef type) {
    page->n = 0;
    page->n_nav = 0;
    page->currentNav = 0;
    page->type = type;
    for (uint8_t i = 0; i < MAX_OBJECT; i++) {
        page->Object[i] = NULL;
        page->Nav[i] = NULL;
    }
    ListPage[PageIndex++] = page;
}

void DuckEngine_Add2Page(DuckPage *page, DuckObject *obj, uint8_t isNav) {
    if (page == NULL) return;
    page->Object[page->n++] = obj;
    if (isNav) {
        page->Nav[page->n_nav++] = obj;
    }
}

void DuckEngine_Set_StartUpPage(DuckPage *page) {
    page->pre = PAGE_NONE;
    DuckEngine_Refresh_Screen();
}

PageTypeDef DuckEngine_Get_Previous_Page(DuckPage *page) {
    return page->pre;
}

DuckPage* DuckEngine_Get_CurrentPage() {
    if (currentPageIndex >= PageIndex) return NULL;
    return ListPage[currentPageIndex];
}

void DuckEngine_Set_CurrentPage(PageTypeDef pre, PageTypeDef next) {
    DuckEngine_Refresh_Screen();
    for (uint8_t i = 0; i < PageIndex; i++) {
        if (ListPage[i]->type == next) {
            currentPageIndex = i;
            ListPage[i]->pre = pre;
            for (uint8_t j = 0; j < ListPage[i]->n; j++) {
                ListPage[i]->Object[j]->isNeedUpdate = 1;
            }
            if (ListPage[i]->n_nav <= 1) {
                DuckObject *obj = ListPage[i]->Nav[ListPage[i]->currentNav];
                DuckEngine_Select_Object(obj);
                return;
            }
            break;
        }
    }
    DuckEngine_Render();
}

void DuckEngine_Refresh_Screen() {
    DuckEngine_Clear_Screen();
    DuckEngine_Draw_Header();
    DuckEngine_Draw_Footer();
}

void DuckEngine_Clear_Screen() {
    duckDriver->FillScreen(BLACK);
}

void DuckEngine_Render() {
    DuckPage *page = DuckEngine_Get_CurrentPage();
    for (uint8_t i = 0; i < page->n; i++) {
        DuckObject *obj = page->Object[i];
        if (obj->isNeedUpdate == 1) {
            obj->Render(obj);
            obj->isNeedUpdate = 0;
        }
    }
}

void DuckEngine_MoveNav_Left() {
    DuckPage *page = DuckEngine_Get_CurrentPage();
    if (page == NULL || page->n_nav <= 1) {
        DuckObject *obj = page->Nav[page->currentNav];
        DuckEngine_Select_Object(obj);
        return;
    }
    DuckObject *obj = page->Nav[page->currentNav];
    obj->isNeedUpdate = 1;
    page->currentNav = (page->currentNav == 0) ? (page->n_nav - 1) : (page->currentNav - 1);
    DuckEngine_Select_Object(page->Nav[page->currentNav]);
}

void DuckEngine_MoveNav_Right() {
    DuckPage *page = DuckEngine_Get_CurrentPage();
    if (page == NULL || page->n_nav <= 1) {
        DuckObject *obj = page->Nav[page->currentNav];
        DuckEngine_Select_Object(obj);
        return;
    }
    DuckObject *obj = page->Nav[page->currentNav];
    obj->isNeedUpdate = 1;
    page->currentNav = (page->currentNav + 1) % page->n_nav;
    DuckEngine_Select_Object(page->Nav[page->currentNav]);
}

__weak void DuckEngineHanleInput() {
}

void DuckEngine_Render_Label(DuckObject *obj) {
    DuckLabel *lb = (DuckLabel*) obj;
    duckDriver->WriteString(lb->base.x, lb->base.y, lb->text, *(lb->font), lb->color, lb->bgcolor);
}

DuckObject* DuckEngine_Create_Label(uint16_t x, uint16_t y, char *text, FontDef *font, uint16_t color, uint16_t bgcolor, DuckPage *page, uint8_t isNav) {
    DuckLabel *lb = &ListLabel[LabelIndex++];
    lb->base.type = DUCK_TYPE_LABEL;
    lb->base.isNeedUpdate = 1;
    lb->base.x = x;
    lb->base.y = y;
    strncpy(lb->text, text, MAX_TEXT_LEN);
    lb->text[MAX_TEXT_LEN - 1] = '\0';
    lb->base.w = strlen(lb->text) * font->width;
    lb->base.h = font->height;
    lb->font = font;
    lb->color = color;
    lb->bgcolor = bgcolor;
    lb->base.Render = DuckEngine_Render_Label;
    DuckEngine_Add2Page(page, (DuckObject*) lb, isNav);
    return (DuckObject*) lb;
}

void DuckEngine_SetText_Label(DuckLabel *label, const char *text) {
    strncpy(label->text, text, sizeof(label->text) - 1);
    label->text[sizeof(label->text) - 1] = '\0';
    label->base.isNeedUpdate = 1;
}

void DuckEngine_Render_Button(DuckObject *obj) {
    DuckButton *btn = (DuckButton*) obj;
    duckDriver->DrawFilledRoundedRect(btn->base.x, btn->base.y, btn->base.w, btn->base.h, btn->radius, btn->color);
    if (btn->label != NULL) {
        btn->label->base.Render((DuckObject*) btn->label);
        btn->label->base.isNeedUpdate = 0;
    }
}

DuckObject* DuckEngine_Create_Button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *label, LabelDirection label_dir, FontDef *font, uint16_t label_color, uint8_t btn_radius, uint16_t btn_color, DuckPage *page, uint8_t isNav, void (*onPressed)(DuckObject*)) {
    DuckLabel *lb = &ListLabel[LabelIndex++];
    DuckButton *btn = &ListButton[ButtonIndex++];
    strncpy(lb->text, label, MAX_TEXT_LEN);
    lb->font = font;
    lb->base.type = DUCK_TYPE_LABEL;
    lb->base.isNeedUpdate = 1;
    lb->color = label_color;
    lb->bgcolor = btn_color;
    lb->base.Render = DuckEngine_Render_Label;
    btn->base.type = DUCK_TYPE_BUTTON;
    btn->base.isNeedUpdate = 1;
    btn->base.x = x;
    btn->base.y = y;
    btn->base.w = w;
    btn->base.h = h;
    btn->radius = btn_radius;
    btn->color = btn_color;
    btn->base.Callback = onPressed;
    btn->base.Render = DuckEngine_Render_Button;
    uint8_t text_w = font->width * strlen(lb->text);
    uint8_t text_h = font->height;
    if (text_w + 4 > btn->base.w) btn->base.w = text_w + 4;
    if (text_h + 2 > btn->base.h) btn->base.h = text_h + 2;
    switch (label_dir) {
        case LABEL_LEFT:
            lb->base.x = btn->base.x + 1;
            lb->base.y = btn->base.y + (btn->base.h - text_h) / 2;
            break;
        case LABEL_RIGHT:
            lb->base.x = btn->base.x + btn->base.w - text_w - 1;
            lb->base.y = btn->base.y + (btn->base.h - text_h) / 2;
            break;
        case LABEL_ABOVE:
            lb->base.x = btn->base.x + (btn->base.w - text_w) / 2;
            lb->base.y = btn->base.y + 1;
            break;
        case LABEL_BELOW:
            lb->base.x = btn->base.x + (btn->base.w - text_w) / 2;
            lb->base.y = btn->base.y + btn->base.h - text_h - 1;
            break;
        case LABEL_CENTER:
        default:
            lb->base.x = btn->base.x + (btn->base.w - text_w) / 2;
            lb->base.y = btn->base.y + (btn->base.h - text_h) / 2;
            break;
    }
    btn->label = lb;
    DuckEngine_Add2Page(page, (DuckObject*) btn, isNav);
    return (DuckObject*) btn;
}

void DuckEngine_Render_Image(DuckObject *obj) {
    DuckImage *img = (DuckImage*) obj;
    duckDriver->DrawImage(img->base.x, img->base.y, img->base.w, img->base.h, img->data);
}

DuckObject* DuckEngine_Create_Image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data, DuckPage *page, uint8_t isNav) {
    DuckImage *img = &ListImage[ImageIndex++];
    img->base.type = DUCK_TYPE_IMAGE;
    img->base.isNeedUpdate = 1;
    img->base.x = x;
    img->base.y = y;
    img->base.w = w;
    img->base.h = h;
    img->data = data;
    img->base.Render = DuckEngine_Render_Image;
    DuckEngine_Add2Page(page, (DuckObject*) img, isNav);
    return (DuckObject*) img;
}

void DuckEngine_Render_App(DuckObject *obj) {
    DuckApp *app = (DuckApp*) obj;
    duckDriver->DrawFilledRoundedRect(app->base.x, app->base.y, app->base.w, app->base.h, app->app_radius, app->app_color);
    if (app->image != NULL) {
        app->image->base.Render((DuckObject*) app->image);
        app->image->base.isNeedUpdate = 0;
    }
    if (app->label != NULL) {
        app->label->base.Render((DuckObject*) app->label);
        app->label->base.isNeedUpdate = 0;
    }
    app->base.isNeedUpdate = 0;
}

DuckObject* DuckEngine_Create_App(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t app_color, uint8_t app_radius, char *label, FontDef *font, uint16_t lb_color, uint16_t lb_bgcolor, LabelDirection label_dir, uint16_t w_img, uint16_t h_img, uint16_t *data, DuckPage *page, uint8_t isNav, void (*onPressed)(DuckObject*)) {
    DuckApp *app = &ListApp[AppIndex++];
    DuckLabel *lb = &ListLabel[LabelIndex++];
    DuckImage *img = &ListImage[ImageIndex++];
    memset(app, 0, sizeof(DuckApp));
    memset(lb, 0, sizeof(DuckLabel));
    memset(img, 0, sizeof(DuckImage));
    app->base.x = x;
    app->base.y = y;
    app->base.w = w;
    app->base.h = h;
    app->base.type = DUCK_TYPE_APP;
    app->base.Render = DuckEngine_Render_App;
    app->base.isNeedUpdate = 1;
    app->base.Callback = onPressed;
    app->label_dir = label_dir;
    app->app_color = app_color;
    app->app_radius = app_radius;
    strncpy(lb->text, label, MAX_TEXT_LEN);
    uint8_t text_w = font->width * strlen(label);
    uint8_t text_h = font->height;
    lb->font = font;
    lb->color = lb_color;
    lb->bgcolor = lb_bgcolor;
    lb->base.type = DUCK_TYPE_LABEL;
    lb->base.isNeedUpdate = 1;
    lb->base.Render = DuckEngine_Render_Label;
    switch (label_dir) {
        case LABEL_LEFT:
            lb->base.x = app->base.x + 2;
            lb->base.y = app->base.y + (app->base.h - text_h) / 2;
            break;
        case LABEL_RIGHT:
            lb->base.x = app->base.x + app->base.w - text_w - 2;
            lb->base.y = app->base.y + (app->base.h - text_h) / 2;
            break;
        case LABEL_ABOVE:
            lb->base.x = app->base.x + (app->base.w - text_w) / 2;
            lb->base.y = app->base.y + 2;
            break;
        case LABEL_BELOW:
            lb->base.x = app->base.x + (app->base.w - text_w) / 2;
            lb->base.y = app->base.y + app->base.h - text_h - 2;
            break;
        case LABEL_CENTER:
        default:
            lb->base.x = app->base.x + (app->base.w - text_w) / 2;
            lb->base.y = app->base.y + (app->base.h - text_h) / 2;
            break;
    }
    img->data = data;
    img->base.w = w_img;
    img->base.h = h_img;
    img->base.x = app->base.x + (app->base.w - w_img) / 2;
    img->base.y = app->base.y + (app->base.h - h_img) / 2;
    img->base.isNeedUpdate = 1;
    img->base.type = DUCK_TYPE_IMAGE;
    img->base.Render = DuckEngine_Render_Image;
    app->label = lb;
    app->image = img;
    DuckEngine_Add2Page(page, (DuckObject*) app, isNav);
    return (DuckObject*) app;
}

FontDef* DuckEngine_Get_Font_From_String(const char *font_str) {
    if (strcmp(font_str, "Font_7x10") == 0) return &Font_7x10;
    if (strcmp(font_str, "Font_11x18") == 0) return &Font_11x18;
    return &Font_7x10;
}

uint16_t* get_icon_ptr(const char *filename) {
    if (filename == NULL) return NULL;
    if (strcmp(filename, "icon_system") == 0) return (uint16_t*) &icon_system[0][0];
    if (strcmp(filename, "icon_folder") == 0) return (uint16_t*) &icon_folder[0][0];
    if (strcmp(filename, "icon_wifi") == 0) return (uint16_t*) &icon_wifi[0][0];
    if (strcmp(filename, "icon_upload") == 0) return (uint16_t*) &icon_upload[0][0];
    return NULL;
}

void (*DuckEngine_Get_OnPressed_From_String(const char *name))(DuckObject *) {
    extern ButtonHandlerMap button_handlers[];
    extern const int button_handlers_count;
    for (int i = 0; i < button_handlers_count; i++) {
        if (strcmp(button_handlers[i].name, name) == 0) {
            return button_handlers[i].handler;
        }
    }
    return NULL;
}

static int Duck_UART_ReadLine(char *buffer, int max_len) {
    int idx = 0;
    char ch;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 1000) {
        if (HAL_UART_Receive(&huart3, (uint8_t*) &ch, 1, 10) == HAL_OK) {
            if (ch == '\n') {
                buffer[idx] = '\0';
                return 1;
            } else if (ch != '\r') {
                if (idx < max_len - 1) buffer[idx++] = ch;
            }
            start = HAL_GetTick();
        }
    }
    buffer[idx] = '\0';
    return (idx > 0);
}

PageTypeDef DuckEngine_Get_PageType_From_String(const char *str) {
    if (strstr(str, "PAGE_HOME")) return PAGE_HOME;
    if (strstr(str, "PAGE_FILE_BROWSER")) return PAGE_FILE_BROWSER;
    if (strstr(str, "PAGE_WIFI")) return PAGE_WIFI;
    if (strstr(str, "PAGE_SYSTEM")) return PAGE_SYSTEM;
    return PAGE_NONE;
}

const char* DuckEngine_PageType_To_String(PageTypeDef type) {
    switch (type) {
        case PAGE_HOME: return "PAGE_HOME";
        case PAGE_FILE_BROWSER: return "PAGE_FILE_BROWSER";
        case PAGE_WIFI: return "PAGE_WIFI";
        case PAGE_SYSTEM: return "PAGE_SYSTEM";
        default: return "UNKNOWN";
    }
}

void DuckEngine_Create_Page_From_UART(DuckPage *page, const char *filename) {
    char line[500];
    char command[64];
    snprintf(command, sizeof(command), "GET_FILE:%s\n", filename);
    HAL_UART_Transmit(&huart3, (uint8_t*) command, strlen(command), 1000);
    while (Duck_UART_ReadLine(line, sizeof(line))) {
        if (strstr(line, "[ESP] ✅ BEGIN")) break;
    }
    while (Duck_UART_ReadLine(line, sizeof(line))) {
        if (strstr(line, "[ESP] ✅ END")) break;
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "PAGE_TYPE:", 10) == 0) {
            char *type_str = line + 10;
            PageTypeDef received_type = DuckEngine_Get_PageType_From_String(type_str);
            if (received_type != page->type) {
                printf("⚠️ PAGE_TYPE mismatch: expected %s, got %s\n", DuckEngine_PageType_To_String(page->type), DuckEngine_PageType_To_String(received_type));
                return;
            }
        } else if (strncmp(line, "Label|", 6) == 0) {
            strtok(line, "|");
            int x = atoi(strtok(NULL, "|"));
            int y = atoi(strtok(NULL, "|"));
            char *text = strtok(NULL, "|");
            char *font_str = strtok(NULL, "|");
            uint32_t color = strtoul(strtok(NULL, "|"), NULL, 16);
            uint32_t bgcolor = strtoul(strtok(NULL, "|"), NULL, 16);
            int isNav = atoi(strtok(NULL, "|"));
            FontDef *font = DuckEngine_Get_Font_From_String(font_str);
            DuckEngine_Create_Label(x, y, text, font, color, bgcolor, page, isNav);
        } else if (strncmp(line, "Button|", 7) == 0) {
            strtok(line, "|");
            int x = atoi(strtok(NULL, "|"));
            int y = atoi(strtok(NULL, "|"));
            int w = atoi(strtok(NULL, "|"));
            int h = atoi(strtok(NULL, "|"));
            char *label_text = strtok(NULL, "|");
            char *label_dir_str = strtok(NULL, "|");
            char *font_str = strtok(NULL, "|");
            uint16_t label_color = strtoul(strtok(NULL, "|"), NULL, 16);
            uint8_t btn_radius = atoi(strtok(NULL, "|"));
            uint16_t btn_color = strtoul(strtok(NULL, "|"), NULL, 16);
            int isNav = atoi(strtok(NULL, "|"));
            char *onPressed_str = strtok(NULL, "|");
            LabelDirection label_dir = LABEL_CENTER;
            if (strcmp(label_dir_str, "LEFT") == 0) label_dir = LABEL_LEFT;
            else if (strcmp(label_dir_str, "RIGHT") == 0) label_dir = LABEL_RIGHT;
            FontDef *font = DuckEngine_Get_Font_From_String(font_str);
            void (*onPressed)(DuckObject*) = DuckEngine_Get_OnPressed_From_String(onPressed_str);
            DuckEngine_Create_Button(x, y, w, h, label_text, label_dir, font, label_color, btn_radius, btn_color, page, isNav, onPressed);
        } else if (strncmp(line, "Image|", 6) == 0) {
            strtok(line, "|");
            int x = atoi(strtok(NULL, "|"));
            int y = atoi(strtok(NULL, "|"));
            int w = atoi(strtok(NULL, "|"));
            int h = atoi(strtok(NULL, "|"));
            char *img_filename = strtok(NULL, "|");
            uint16_t *icon_data = get_icon_ptr(img_filename);
            int isNav = atoi(strtok(NULL, "|"));
            DuckEngine_Create_Image(x, y, w, h, icon_data, page, isNav);
        }
    }
}

void DuckEngine_Select_Object(DuckObject *obj) {
    duckDriver->DrawRectangle(obj->x, obj->y, obj->x + obj->w, obj->y + obj->h, GREEN);
}

void DuckEngine_Remove_Selected_Object(DuckObject *obj) {
    obj->isNeedUpdate = 1;
}
