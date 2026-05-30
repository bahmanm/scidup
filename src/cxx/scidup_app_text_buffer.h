///////////////////////////////////////////////////////////////////////////
//
//  FILE:       textbuf.h
//              TextBuffer class
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    2.7
//
//  Notice:     Copyright (c) 1999-2001 Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
///////////////////////////////////////////////////////////////////////////


#ifndef SCID_TEXTBUF_H
#define SCID_TEXTBUF_H

#include "scid/database/common.h"
#include <stdio.h>

namespace scidup::app {

class TextBuffer
{
private:
    //----------------------------------
    //  TextBuffer:  Data Structures
    
    scid::core::uint   Column;
    scid::core::uint   IndentColumn;
    scid::core::uint   WrapColumn;
    scid::core::uint   LineIsEmpty;  // true if current line is empty.
    scid::core::uint   LineCount;
    scid::core::uint   ByteCount;
    scid::core::uint   BufferSize;
    bool   ConvertNewlines;  // If true, convert newlines to spaces.
    char * Buffer;
    char * Current;

    bool   PausedTranslations;
    bool   HasTranslations;
    const char * Translation [256];

    inline void   AddChar (char ch);
    TextBuffer(const TextBuffer&);
    TextBuffer& operator=(const TextBuffer&);

    //----------------------------------
    //  TextBuffer:  Public Functions
public:
    TextBuffer() {
        Init();
        SetBufferSize(1280000);
    }
    ~TextBuffer()   { Free(); }
    
    void     Init ();
    void     Free ();
    void     Empty ();
    
    void     SetBufferSize (scid::core::uint length);
    scid::core::uint     GetBufferSize()     { return BufferSize; }
    scid::core::uint     GetByteCount()      { return ByteCount; }
    scid::core::uint     GetLineCount()      { return LineCount; }
    scid::core::uint     GetColumn()         { return Column; }
    scid::core::uint     GetWrapColumn ()    { return WrapColumn; }
    void     SetWrapColumn (scid::core::uint column) { WrapColumn = column; }
    scid::core::uint     GetIndent ()        { return IndentColumn; }
    void     SetIndent (scid::core::uint column) { IndentColumn = column; }
    char *   GetBuffer ()        { return Buffer; }
    void     NewlinesToSpaces (bool b) { ConvertNewlines = b; }

    void     AddTranslation (char ch, const char * str);
    // void     ClearTranslation (char ch) { Translation[ch] = NULL; }
    // Changed ch to int, to avoid compiler warnings. 
    void     ClearTranslation (int ch) { Translation[ch] = NULL; }
    void     ClearTranslations () { HasTranslations = false; }
    void     PauseTranslations () { PausedTranslations = true; }
    void     ResumeTranslations () { PausedTranslations = false; }

    scid::core::errorT   NewLine();
    scid::core::errorT   Indent();
    scid::core::errorT   PrintLine (const char * str);
    scid::core::errorT   PrintWord (const char * str);
    scid::core::errorT   PrintString (const char * str);
    scid::core::errorT   PrintSpace ();
    scid::core::errorT   PrintChar (char b);

    scid::core::errorT   PrintInt (scid::core::uint i, const char * str);
    inline scid::core::errorT PrintInt (scid::core::uint i) { return PrintInt (i, ""); }

};

inline void
TextBuffer::AddChar (char ch)
{
    if (HasTranslations  &&  !PausedTranslations) {
        scid::core::byte b = (scid::core::byte) ch;
        const char * str = Translation[b];
        if (str != NULL) {
            const char * s = str;
            while (*s) {
                *Current++ = *s++;
                ByteCount++;
            }
            return;
        }
    }
    *Current = ch;
    Current++;
    ByteCount++;
}


} // namespace scidup::app
#endif  // SCID_TEXTBUF_H

///////////////////////////////////////////////////////////////////////////
//  EOF: textbuf.h
///////////////////////////////////////////////////////////////////////////
