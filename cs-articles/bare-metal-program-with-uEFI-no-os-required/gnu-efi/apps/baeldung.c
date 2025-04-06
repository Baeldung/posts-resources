#include <efi.h>
#include <efilib.h>

#define MAX_INPUT 100

// Reads a line from the keyboard and stores it in 'buffer'
EFI_STATUS ReadLine(EFI_SYSTEM_TABLE *systab, CHAR16 *buffer, UINTN bufferSize) {
    UINTN idx = 0;
    EFI_INPUT_KEY key;
    UINTN index;
    while (1) {
        uefi_call_wrapper(systab->BootServices->WaitForEvent, 3, 1, &(systab->ConIn->WaitForKey), &index);
        uefi_call_wrapper(systab->ConIn->ReadKeyStroke, 2, systab->ConIn, &key);
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            buffer[idx] = L'\0';
            Print(u"\n");
            break;
        } else if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (idx > 0) {
                idx--;
                // Erase the character from the display
                Print(u"\b \b");
            }
        } else {
            if (idx < bufferSize - 1) {
                buffer[idx++] = key.UnicodeChar;
                Print(u"%c", key.UnicodeChar);  // Echo the character
            }
        }
    }
    return EFI_SUCCESS;
}

// Waits until the user presses Enter.
EFI_STATUS WaitForEnter(EFI_SYSTEM_TABLE *systab) {
    EFI_INPUT_KEY key;
    UINTN index;
    while (1) {
        uefi_call_wrapper(systab->BootServices->WaitForEvent, 3, 1, &(systab->ConIn->WaitForKey), &index);
        uefi_call_wrapper(systab->ConIn->ReadKeyStroke, 2, systab->ConIn, &key);
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN)
            break;
    }
    return EFI_SUCCESS;
}

// Simple function to convert a decimal string (base 10) to a UINTN
UINTN ConvertStrToUintn(CHAR16 *str) {
    UINTN num = 0;
    while (*str) {
        if (*str < L'0' || *str > L'9')
            break;
        num = num * 10 + (*str - L'0');
        str++;
    }
    return num;
}

EFI_STATUS efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    CHAR16 inputBuffer[MAX_INPUT];
    UINTN number1, number2, sum;

    InitializeLib(image, systab);

    // Print greeting message.
    Print(u"Hello BAELDUNG!!!\n\n");

    // Ask for the first integer.
    Print(u"Please enter the first integer: ");
    ReadLine(systab, inputBuffer, MAX_INPUT);
    number1 = ConvertStrToUintn(inputBuffer);

    // Ask for the second integer.
    Print(u"Please enter the second integer: ");
    ReadLine(systab, inputBuffer, MAX_INPUT);
    number2 = ConvertStrToUintn(inputBuffer);

    // Compute the sum.
    sum = number1 + number2;
    Print(u"\nThe sum is: %d\n", sum);

    // Wait for Enter before exiting.
    Print(u"\nPress Enter to exit...\n");
    WaitForEnter(systab);

    return EFI_SUCCESS;
}

