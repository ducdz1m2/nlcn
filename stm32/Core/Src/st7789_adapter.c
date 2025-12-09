/*
 * st7789_adapter.c
 *
 *  Created on: Jul 17, 2025
 *      Author: chienham
 */
#include "st7789.h"
#include "gui.h"


DuckEngineDriver duckDriver = {
	.DrawPixel = ST7789_DrawPixel,
	.DrawLine = ST7789_DrawLine,
	.DrawRectangle = ST7789_DrawRectangle,
	.DrawFilledRectangle = ST7789_DrawFilledRectangle,
	.DrawFilledRoundedRect = ST7789_DrawFilledRoundedRect,
	.DrawCircle = ST7789_DrawCircle,
	.DrawFilledCircle = ST7789_DrawFilledCircle,
	.DrawTriangle = ST7789_DrawTriangle,
	.DrawFilledTriangle = ST7789_DrawFilledTriangle,
	.DrawImage = ST7789_DrawImage,
	.WriteChar = ST7789_WriteChar,
	.WriteString = ST7789_WriteString,
	.InvertColors = ST7789_InvertColors,
	.FillScreen = ST7789_Fill_Color,

};


void DuckEngine_Bind_ST7789(void) {

	ST7789_Init();
	DuckEngine_BindDriver(&duckDriver);
}
