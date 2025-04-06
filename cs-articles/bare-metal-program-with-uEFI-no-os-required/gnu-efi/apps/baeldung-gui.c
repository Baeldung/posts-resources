#include <efi.h>
#include <efilib.h>
#include "baeldung-font9x16.h"

// Each font character is 9×16, but we draw them at 2× scale (18×32).
#define FONT_WIDTH   9
#define FONT_HEIGHT 16
#define SCALE_FACTOR 2

// We'll use a helper macro to clamp the pointer within screen bounds.
#define CLAMP(val, minVal, maxVal) \
  ((val) < (minVal) ? (minVal) : ((val) > (maxVal) ? (maxVal) : (val)))

//--------------------------------------------------------------------
// Drawing routines
//--------------------------------------------------------------------

// Draw a single 8×16 character from the font, scaled by “scale”
void DrawCharScaled(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
    UINTN x,
    UINTN y,
    UINTN scale,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL color,
    CHAR16 ch
)
{
    // For each of the 16 rows:
    for (UINTN row = 0; row < FONT_HEIGHT; row++) {
        UINT8 bits = Font9X16[(UINT8)ch][row]; 
        for (UINTN col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                // “On” pixel => fill a (scale×scale) rectangle.
                UINTN startX = x + col * scale;
                UINTN startY = y + row * scale;
                for (UINTN sy = 0; sy < scale; sy++) {
                    for (UINTN sx = 0; sx < scale; sx++) {
                        gop->Blt(
                            gop, 
                            &color,
                            EfiBltVideoFill,
                            0, 0,
                            startX + sx,
                            startY + sy,
                            1, 1,
                            0
                        );
                    }
                }
            }
        }
    }
}

// Draw a null‐terminated CHAR16 string scaled by “scale”
void DrawStringScaled(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
    UINTN x,
    UINTN y,
    UINTN scale,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL color,
    CHAR16 *str
)
{
    while (*str) {
        DrawCharScaled(gop, x, y, scale, color, *str);
        x += FONT_WIDTH * scale;
        str++;
    }
}

// Draw the initial screen (background, button, text, pointer).
void DrawInitialUI(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
    UINTN screenW,
    UINTN screenH,
    UINTN btnX,
    UINTN btnY,
    UINTN btnW,
    UINTN btnH,
    UINTN pointerX,
    UINTN pointerY
)
{
    // Define colors for BGR (Blue, Green, Red)
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL lightGray = {0xC0, 0xC0, 0xC0, 0x00};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL blue      = {0xFF, 0x00, 0x00, 0x00};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL white     = {0xFF, 0xFF, 0xFF, 0x00};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL red       = {0x00, 0x00, 0xFF, 0x00};

    // Fill the entire screen with light gray.
    gop->Blt(gop, &lightGray, EfiBltVideoFill,
             0, 0, 0, 0, screenW, screenH, 0);

    // Draw the centered blue button.
    gop->Blt(gop, &blue, EfiBltVideoFill,
             0, 0, btnX, btnY, btnW, btnH, 0);

    // Write “Click here” at the center of that button.
    CHAR16 *buttonText = u"Click here";
    UINTN len = StrLen(buttonText);
    UINTN textW = len * FONT_WIDTH * SCALE_FACTOR;
    UINTN textH = FONT_HEIGHT * SCALE_FACTOR;
    UINTN textX = btnX + (btnW - textW) / 2;
    UINTN textY = btnY + (btnH - textH) / 2;
    DrawStringScaled(gop, textX, textY, SCALE_FACTOR, white, buttonText);

    // Draw the pointer (red rectangle) at (pointerX, pointerY).
    // Size 18×32 because we do 2× scaling for an 9×16 “cursor”
    UINTN ptrW = FONT_WIDTH  * SCALE_FACTOR; // 16
    UINTN ptrH = FONT_HEIGHT * SCALE_FACTOR; // 32
    gop->Blt(gop, &red, EfiBltVideoFill,
             0, 0, pointerX, pointerY, ptrW, ptrH, 0);
}

// Draw the final screen after activation.
void DrawFinalUI(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
    UINTN screenW,
    UINTN screenH
)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL lightGray = {0xC0, 0xC0, 0xC0, 0x00};
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL black     = {0x00, 0x00, 0x00, 0x00};

    // Fill screen with light gray.
    gop->Blt(gop, &lightGray, EfiBltVideoFill,
             0, 0, 0, 0, screenW, screenH, 0);

    // “It works! Press Enter to exit”
    CHAR16 *finalText = u"It works! Press Enter to exit";
    UINTN len = StrLen(finalText);
    UINTN txtW = len * FONT_WIDTH * SCALE_FACTOR;
    UINTN txtH = FONT_HEIGHT * SCALE_FACTOR;
    UINTN textX = (screenW - txtW) / 2;
    UINTN textY = (screenH - txtH) / 2;
    DrawStringScaled(gop, textX, textY, SCALE_FACTOR, black, finalText);
}

//--------------------------------------------------------------------

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    // Locate the Graphics Output Protocol for 2D drawing.
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = systab->BootServices->LocateProtocol(&gopGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status) || !gop) {
        Print(u"ERROR: No Graphics Output Protocol.\n");
        return EFI_UNSUPPORTED;
    }

    // Get screen resolution.
    UINTN screenW = gop->Mode->Info->HorizontalResolution;
    UINTN screenH = gop->Mode->Info->VerticalResolution;

    // Define the button in the center of the screen.
    UINTN btnW = 300;
    UINTN btnH = 60;
    UINTN btnX = (screenW - btnW) / 2;
    UINTN btnY = (screenH - btnH) / 2;

    // We'll keep a pointer (red rectangle) that starts near top-left.
    // (You can start it at center if you like.)
    UINTN pointerX = 50;
    UINTN pointerY = 50;

    // The pointer is 18×32 in size.
    UINTN pointerW = FONT_WIDTH  * SCALE_FACTOR; // 18
    UINTN pointerH = FONT_HEIGHT * SCALE_FACTOR; // 32

    // Draw the initial UI once.
    DrawInitialUI(gop, screenW, screenH, btnX, btnY, btnW, btnH, pointerX, pointerY);

    // We'll watch for keyboard events only (arrow keys to move, Enter to click).
    EFI_EVENT waitEvent = systab->ConIn->WaitForKey;
    BOOLEAN activated = FALSE;

    while (!activated) {
        UINTN index;
        status = systab->BootServices->WaitForEvent(1, &waitEvent, &index);
        EFI_INPUT_KEY key;
        if (!EFI_ERROR(systab->ConIn->ReadKeyStroke(systab->ConIn, &key))) {
            // Check if arrow key
            if (key.ScanCode == SCAN_UP) {
                // Move pointer up
                // We'll pick a small step of 10 pixels
                INTN newY = (INTN)pointerY - 10;
                pointerY = CLAMP(newY, 0, (INTN)(screenH - pointerH));
            } 
            else if (key.ScanCode == SCAN_DOWN) {
                INTN newY = (INTN)pointerY + 10;
                pointerY = CLAMP(newY, 0, (INTN)(screenH - pointerH));
            } 
            else if (key.ScanCode == SCAN_LEFT) {
                INTN newX = (INTN)pointerX - 10;
                pointerX = CLAMP(newX, 0, (INTN)(screenW - pointerW));
            } 
            else if (key.ScanCode == SCAN_RIGHT) {
                INTN newX = (INTN)pointerX + 10;
                pointerX = CLAMP(newX, 0, (INTN)(screenW - pointerW));
            }
            // If user presses Enter, see if pointer is inside the button
            else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
                if (pointerX + pointerW > btnX && pointerX < (btnX + btnW) &&
                    pointerY + pointerH > btnY && pointerY < (btnY + btnH))
                {
                    // The pointer overlaps the button area => activated
                    activated = TRUE;
                    break;
                }
            }

            // Redraw the initial UI with the pointer in its new position
            DrawInitialUI(gop, screenW, screenH, btnX, btnY, btnW, btnH, pointerX, pointerY);
        }
    }

    // Once activated, show final screen:
    DrawFinalUI(gop, screenW, screenH);

    // Wait for Enter to exit
    while (TRUE) {
        UINTN idx;
        systab->BootServices->WaitForEvent(1, &systab->ConIn->WaitForKey, &idx);
        EFI_INPUT_KEY key;
        if (!EFI_ERROR(systab->ConIn->ReadKeyStroke(systab->ConIn, &key))) {
            if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
                break;
            }
        }
    }

    return EFI_SUCCESS;
}

