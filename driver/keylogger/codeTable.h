
// Funkcja konwertująca scancode na znak
const char *scancode_to_char(int scancode)
{
    // Tablica skanów i ich odpowiadających znaków
    static const char *keymap[] = {
        [0x01] = "Esc",
        [0x02] = "1",
        [0x03] = "2",
        [0x04] = "3",
        [0x05] = "4",
        [0x06] = "5",
        [0x07] = "6",
        [0x08] = "7",
        [0x09] = "8",
        [0x0A] = "9",
        [0x0B] = "0", // Klawisze numeryczne górne
        [0x1E] = "A",
        [0x30] = "B",
        [0x2E] = "C",
        [0x20] = "D",
        [0x12] = "E",
        [0x21] = "F",
        [0x22] = "G",
        [0x23] = "H",
        [0x17] = "I",
        [0x24] = "J",
        [0x25] = "K",
        [0x26] = "L",
        [0x32] = "M",
        [0x31] = "N",
        [0x18] = "O",
        [0x19] = "P",
        [0x10] = "Q",
        [0x13] = "R",
        [0x1F] = "S",
        [0x14] = "T",
        [0x16] = "U",
        [0x2F] = "V",
        [0x11] = "W",
        [0x2D] = "X",
        [0x15] = "Y",
        [0x2C] = "Z",
        [0x4E] = "-",
        [0x55] = "=",
        [0x4F] = "[",
        [0x50] = "]",
        [0x51] = ";",
        [0x52] = "'",
        [0x53] = ",",
        [0x54] = ".",
        [0x56] = "/",
        [0x29] = "Space",
        [0x1C] = "Enter",
        [0x0E] = "Backspace",
        [0x39] = "Alt",
        [0x3A] = "CapsLock",
        [0x1D] = "Ctrl",
        [0x2A] = "LeftShift",
        [0x36] = "Right Shift",
        [0x38] = "Alt",
        // Możesz dodać inne klawisze funkcyjne, numeryczne itp.
        // Mapowanie dla klawiatury numerycznej, jeśli chcesz:
        [0x47] = "Num 7",
        [0x48] = "Num 8",
        [0x49] = "Num 9",
        [0x4B] = "Num -",
        [0x4F] = "Num 4",
        [0x50] = "Num 5",
        [0x51] = "Num 6",
        [0x4E] = "Num +",
        [0x52] = "Num 1",
        [0x53] = "Num 2",
        [0x54] = "Num 3",
        [0x55] = "Num 0",
        [0x56] = "Num ."
        // Dodaj więcej mapowania w razie potrzeby
    };

    // Jeśli scancode istnieje w mapie, zwróć odpowiadający znak
    if (scancode >= 0 && scancode < sizeof(keymap) / sizeof(keymap[0]))
    {
        return keymap[scancode];
    }

    return "Unknown"; // W przypadku nieznanego skanera
}
