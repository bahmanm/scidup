///////////////////////////////////////////////////////////////////////////
//
//  FILE:       textbuf.cpp
//              TextBuffer class methods
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    2.7
//
//  Notice:     Copyright (c) 1999-2001 Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
///////////////////////////////////////////////////////////////////////////

#include "scidup_app_text_buffer.h"
#include "scid/database/misc.h"
#include <cstdio>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::Init(): Initialise the textbuffer.
namespace scidup::app {

void
TextBuffer::Init (void)
{
    BufferSize = Column = IndentColumn = LineCount = ByteCount = 0;
    LineIsEmpty = 1;
    Buffer = Current = NULL;
    WrapColumn = 80;
    ConvertNewlines = true;
    HasTranslations = false;
    PausedTranslations = false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::Free(): Free the TextBuffer.
void
TextBuffer::Free (void)
{
    if (Buffer != NULL) {
#ifdef WINCE
        my_Tcl_Free( Buffer);
#else
        delete[] Buffer;
#endif
        Buffer = NULL;
        BufferSize = 0;
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::Empty(): Empty the TextBuffer.
void
TextBuffer::Empty (void)
{
    ASSERT(Buffer != NULL);
    ByteCount = Column = LineCount = 0; LineIsEmpty = 1;
    Current = Buffer;
    *Current = 0;
    ConvertNewlines = true;
    HasTranslations = false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TextBuffer::AddTranslation():
//   Adds a translation for a character.
//   The translation string will be printed in place of that character.
void
TextBuffer::AddTranslation (char ch, const char * str)
{
    if (! HasTranslations) {
        HasTranslations = true;
        for (scid::core::uint i=0; i < 256; i++) {
            Translation [i] = NULL;
        }
    }
    Translation [(scid::core::byte) ch] = str;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::SetBufferSize(): Set the buffer size.
void
TextBuffer::SetBufferSize (scid::core::uint length)
{
#ifdef WINCE
    if (Buffer != NULL) { my_Tcl_Free( Buffer); }
    Buffer = my_Tcl_Alloc(sizeof(char[length]));
#else
    if (Buffer != NULL) { delete[] Buffer; }
    Buffer = new char[length];
#endif
    BufferSize = length;
    ByteCount = Column = LineCount = 0; LineIsEmpty = 1;
    Current = Buffer;
    *Current = 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::NewLine(): Add a newline.
scid::core::errorT
TextBuffer::NewLine ()
{
    ASSERT (Current != NULL);
    if (ByteCount >= BufferSize) { return scid::core::ERROR_BufferFull; }
    *Current++ = '\n'; 
    LineCount++; ByteCount++; LineIsEmpty = 1;
    Column = 0; 
    while (Column < IndentColumn) {
        if (ByteCount >= BufferSize) { return scid::core::ERROR_BufferFull; }
        *Current++ = ' '; Column++; ByteCount++;
    }
    *Current = 0;
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::Indent(): Indent to the current Indentation level..
scid::core::errorT
TextBuffer::Indent ()
{
    ASSERT (Current != NULL);
    if (!LineIsEmpty) {
        return NewLine();
    } else {
        while (Column < IndentColumn) {
            if (ByteCount >= BufferSize) { return scid::core::ERROR_BufferFull; }
            *Current++ = ' '; Column++; ByteCount++;
        }
        *Current = 0;
    }
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintLine(): Print a string then newline. Does not
//          check for the line going past WrapColumn.
scid::core::errorT
TextBuffer::PrintLine (const char * str)
{
    ASSERT(Current != NULL);
    while (*str != 0) {
        if (ByteCount > BufferSize) { return scid::core::ERROR_BufferFull; }
        AddChar (*str);
        str++;
    }
    return NewLine();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintWord(): Prints a word, wrapping if necessary.
//     It does NOT add a space, since that is left to the caller to
//     provide in the string.
scid::core::errorT
TextBuffer::PrintWord (const char * str)
{
    ASSERT(Current != NULL);
    scid::core::uint length = scid::database::strLength (str);
    if (Column + length >= WrapColumn)    { NewLine(); }
    if (ByteCount + length >= BufferSize) { return scid::core::ERROR_BufferFull; }
    while (*str != 0) {
        char ch = *str;
        AddChar (ch);
        str++;
        Column++;
    }
    *Current = 0;  // add trailing end-of-string to buffer
    LineIsEmpty = 0;
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintSpace(): Prints a space OR a newline character,
//     but not both.
scid::core::errorT
TextBuffer::PrintSpace (void)
{
    if (ByteCount + 1 >= BufferSize)  { return scid::core::ERROR_BufferFull; }
    if (Column + 1 >= WrapColumn) {
        NewLine();
    } else {
        *Current = ' '; Current++; ByteCount++; Column++; LineIsEmpty = 0;
    }
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintChar(): prints a single char, adding a newline
//      first if necessary.
scid::core::errorT
TextBuffer::PrintChar (char b)
{
    if (Column + 1 >= WrapColumn)  { NewLine(); }
    if (ByteCount + 1 >= BufferSize)  { return scid::core::ERROR_BufferFull; }
    AddChar (b);
    Column++; LineIsEmpty = 0;
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintString(): Print a string, wrapping at spaces.
//      Also converts newlines in the string into spaces.
scid::core::errorT
TextBuffer::PrintString (const char * str)
{
    scid::core::errorT err;
    char currentWord[1024];  // should be long enough for a word
    while (*str != 0) {
        char * b = currentWord;
        *b = 0;
        // get next word and print it:
        while (*str != ' '  && *str != '\n'  &&  *str != '\0') {
            *b = *str; b++; str++;
        }
        // end of word/line/text reached
        *b = 0;
        err = PrintWord (currentWord);
        if (err != scid::core::OK) { return err; }
        if (*str == 0) { return scid::core::OK; }
        if (*str == '\n'  &&  !ConvertNewlines) {
            err = NewLine();
        } else {
            err = PrintSpace();
        }
        if (err != scid::core::OK) { return err; }
        str++;
    }
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//### TextBuffer::PrintInt(): Print a decimal number followed by string
//      as a word (so it appends a space at the end and wraps if
//      necessary).
scid::core::errorT
TextBuffer::PrintInt (scid::core::uint i, const char * str)
{
    char temp[255];
    std::snprintf(temp, sizeof(temp), "%u%s", i, str);
    return PrintWord(temp);
}

///////////////////////////////////////////////////////////////////////////
//  EOF: textbuf.cpp
///////////////////////////////////////////////////////////////////////////

} // namespace scidup::app
