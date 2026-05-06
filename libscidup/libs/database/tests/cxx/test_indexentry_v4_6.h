/*
* Copyright (c) 1999-2002  Shane Hudson
* Copyright (c) 2006-2009  Pascal Georges
* Copyright (C) 2014  Fulvio Benini

* This file is part of Scid (Shane's Chess Information Database).
*
* Scid is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation.
*
* Scid is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Scid.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCID_INDEXENTRY_V4_6_H
#define SCID_INDEXENTRY_V4_6_H

#include "scidup/core/date.h"
#include "scidup/core/game_result.h"
#include "scidup/eco/code.h"
#include "scidup/database/common.h"
#include "scidup/database/matsig.h"
#include "scidup/database/namebase.h"

// HPSIG_SIZE = size of HomePawnData array in an scid::database::IndexEntry.
// It is nine bytes: the first scid::database::byte contains the number of valid entries
// in the array, and the next 8 bytes contain up to 16 half-scid::database::byte entries.
const scid::database::uint HPSIG_SIZE = 9;

const scid::database::uint MAX_ELO = 4000; // Since we store Elo Ratings in 12 bits

const scid::database::byte CUSTOM_FLAG_MASK[] = { 1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5 };

// Total on-disk size per index entry: currently 47 bytes.
const scid::database::uint  INDEX_ENTRY_SIZE = 47;
const scid::database::uint  OLD_INDEX_ENTRY_SIZE = 46;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class scid::database::IndexEntry: one of these per game in the index file.
//
//    It contains more than just the location of the game data in the main
//    data file.  For fast searching, it also store some other important
//    values: players, event, site, date, result, eco, gamelength.
//
//    It takes 48 bytes, assuming sizeof(scid::database::uint) == 4 and sizeof(scid::database::ushort) == 2.

class IndexEntry
{
    uint32_t  Offset;            // Start of gamefile record for this game.
    uint16_t  Length_Low;        // Length of gamefile record for this game. 17 bits are used so the max
                                 // length is 128 ko (131071). So 7 bits are usable for custom flags or other.
    scid::database::byte      Length_High;       // LxFFFFFF ( L = length for long games, x = spare, F = custom flags)
    // Name ID values are packed into 12 bytes, saving 8 bytes over the
    // simpler method of just storing each as a 4-scid::database::byte scid::database::idNumberT.
    scid::database::byte      WhiteBlack_High;   // High bits of White, Black.
    uint16_t  WhiteID_Low;       // Lower 16 bits of White ID.
    uint16_t  BlackID_Low;       // Lower 16 bits of Black ID.
    uint16_t  EventID_Low;       // Lower 16 bits of Site.
    uint16_t  SiteID_Low;        // Lower 16 bits of Site ID.
    uint16_t  RoundID_Low;       // Lower 16 bits of Round ID.
    uint16_t  Flags;
    uint16_t  VarCounts;         // Counters for comments, variations, etc.
                                 // VarCounts also stores the result.
    scidup::eco::Code EcoCode;           // ECO code
    scid::database::dateT     Dates;             // Date and EventDate fields.
    scid::database::eloT      WhiteElo;
    scid::database::eloT      BlackElo;
    scid::database::matSigT   FinalMatSig;       // material of the final position in the game,
                                 // and the StoredLineCode in the top 8 bits.
    uint16_t  NumHalfMoves;
    scid::database::byte      HomePawnData [HPSIG_SIZE];  // homePawnSig data.
    scid::database::byte      EventSiteRnd_High; // High bits of Event, Site, Round.
    
public:
    void Init();
    template <class T> scid::database::errorT Read (T* file, scid::database::versionT version);
    template <class T> scid::database::errorT Write (T* file, scid::database::versionT version) const;


    uint32_t GetOffset () const { return Offset; }
    void SetOffset (uint64_t offset) { Offset = static_cast<uint32_t>(offset); }
    uint32_t GetLength() const {
        return Length_Low + (uint32_t(Length_High & 0x80) << 9);
    }
    void SetLength (size_t length) {
        Length_Low = static_cast<uint16_t>(length & 0xFFFF);
        // preserve the last 7 bits
        Length_High = ( Length_High & 0x7F ) | static_cast<scid::database::byte>( (length >> 16) << 7 );
    }


    // Name Get and Set routines:
    //   WhiteID and BlackID are 20-bit values, EventID and SiteID are
    //   19-bit values, and RoundID is an 18-bit value.
    //
    //   WhiteID high 4 bits = bits 4-7 of WhiteBlack_High.
    //   BlackID high 4 bits = bits 0-3 of WhiteBlack_High.
    //   EventID high 3 bits = bits 5-7 of EventSiteRnd_high.
    //   SiteID  high 3 bits = bits 2-4 of EventSiteRnd_high.
    //   RoundID high 2 bits = bits 0-1 of EventSiteRnd_high.
    scid::database::idNumberT GetWhite () const {
        scid::database::idNumberT id = (scid::database::idNumberT) WhiteBlack_High;
        id = id >> 4;  // High 4 bits = bits 4-7 of WhiteBlack_High.
        id <<= 16;
        id |= (scid::database::idNumberT) WhiteID_Low;
        return id;
    }
    scid::database::idNumberT GetBlack () const {
        scid::database::idNumberT id = (scid::database::idNumberT) WhiteBlack_High;
        id = id & 0xF;   // High 4 bits = bits 0-3 of WhiteBlack_High.
        id <<= 16;
        id |= (scid::database::idNumberT) BlackID_Low;
        return id;
    }
    scid::database::idNumberT GetPlayer(scid::database::colorT col) const {
        if (col == scid::database::BLACK) return GetBlack();
        return GetWhite();
    }
    scid::database::idNumberT GetEvent () const {
        scid::database::uint id = (scid::database::idNumberT) EventSiteRnd_High;
        id >>= 5;  // High 3 bits = bits 5-7 of EventSiteRnd_High.
        id <<= 16;
        id |= (scid::database::idNumberT) EventID_Low;
        return id;
    }
    scid::database::idNumberT GetSite () const {
        scid::database::uint id = (scid::database::idNumberT) EventSiteRnd_High;
        id = (id >> 2) & 7;  // High 3 bits = bits 2-5 of EventSiteRnd_High.
        id <<= 16;
        id |= (scid::database::idNumberT) SiteID_Low;
        return id;
    }
    scid::database::idNumberT GetRound () const {
        scid::database::uint id = (scid::database::idNumberT) EventSiteRnd_High;
        id &= 3;   // High 2 bits = bits 0-1 of EventSiteRnd_High.
        id <<= 16;
        id |= (scid::database::idNumberT) RoundID_Low;
        return id;
    }

    void SetWhite (scid::database::idNumberT id) {
        WhiteID_Low = id & 0xFFFF;
        WhiteBlack_High = WhiteBlack_High & 0x0F;   // Clear bits 4-7.
        WhiteBlack_High |= ((id >> 16) << 4);       // Set bits 4-7.
    }
    void SetBlack (scid::database::idNumberT id) {
        BlackID_Low = id & 0xFFFF;
        WhiteBlack_High = WhiteBlack_High & 0xF0;   // Clear bits 0-3.
        WhiteBlack_High |= (id >> 16);              // Set bits 0-3.
    }
    void SetPlayer (scid::database::colorT col, scid::database::idNumberT id) {
        if (col == scid::database::BLACK) return SetBlack(id);
        return SetWhite(id);
    }
    void SetEvent (scid::database::idNumberT id) {
        EventID_Low = id & 0xFFFF;
        // Clear bits 2-4 of EventSiteRnd_high: 31 = 00011111 binary.
        EventSiteRnd_High = EventSiteRnd_High & 31;
        EventSiteRnd_High |= ((id >> 16) << 5);
    }
    void SetSite (scid::database::idNumberT id) {
        SiteID_Low = id & 0xFFFF;
        // Clear bits 2-4 of EventSiteRnd_high: 227 = 11100011 binary.
        EventSiteRnd_High = EventSiteRnd_High & 227;
        EventSiteRnd_High |= ((id >> 16) << 2);
    }
    void SetRound (scid::database::idNumberT id) {
        RoundID_Low = id & 0xFFFF;
        // Clear bits 0-1 of EventSiteRnd_high: 252 = 11111100 binary.
        EventSiteRnd_High = EventSiteRnd_High & 252;
        EventSiteRnd_High |= (id >> 16);
    }


    const char* GetWhiteName (const scid::database::NameBase* nb) const {
        return nb->GetName (scid::database::NAME_PLAYER, GetWhite()); 
    }
    const char* GetBlackName (const scid::database::NameBase* nb) const {
        return nb->GetName (scid::database::NAME_PLAYER, GetBlack());
    }
    const char* GetEventName (const scid::database::NameBase* nb) const {
        return nb->GetName (scid::database::NAME_EVENT, GetEvent());
    }
    const char* GetSiteName (const scid::database::NameBase* nb) const {
        return nb->GetName (scid::database::NAME_SITE, GetSite());
    }
    const char* GetRoundName (const scid::database::NameBase* nb) const {
        return nb->GetName (scid::database::NAME_ROUND, GetRound());
    }

    scid::database::dateT GetDate () const { return u32_low_20(Dates); }
    scid::database::uint  GetYear () const { return scid::database::date_GetYear (GetDate()); }
    scid::database::uint  GetMonth() const { return scid::database::date_GetMonth (GetDate()); }
    scid::database::uint  GetDay ()  const { return scid::database::date_GetDay (GetDate()); }
    scid::database::dateT GetEventDate () const {
        scid::database::uint dyear = scid::database::date_GetYear (GetDate());
        scid::database::dateT edate = u32_high_12 (Dates);
        scid::database::uint month = scid::database::date_GetMonth (edate);
        scid::database::uint day = scid::database::date_GetDay (edate);
        scid::database::uint year = scid::database::date_GetYear(edate) & 7;
        if (year == 0) { return scid::database::ZERO_DATE; }
        year = dyear + year - 4;
        return (year << scid::database::YEAR_SHIFT) |
               (month << scid::database::MONTH_SHIFT) | day;
    }
    scid::database::resultT GetResult () const { return (VarCounts >> 12); }
    scid::database::eloT GetWhiteElo () const { return u16_low_12(WhiteElo); }
    scid::database::eloT GetBlackElo () const { return u16_low_12(BlackElo); }
    scid::database::eloT GetElo(scid::database::colorT col) const {
        if (col == scid::database::BLACK) return GetBlackElo();
        return GetWhiteElo();
    }
    scid::database::byte   GetWhiteRatingType () const { return u16_high_4 (WhiteElo); }
    scid::database::byte   GetBlackRatingType () const { return u16_high_4 (BlackElo); }
    scidup::eco::Code GetEcoCode() const { return EcoCode; }
    scid::database::ushort GetNumHalfMoves () const { return NumHalfMoves; }
    scid::database::byte   GetRating(const scid::database::NameBase* nb) const;

    void SetDate  (scid::database::dateT date)   {
        Dates = u32_set_low_20 (Dates, date);
    }
    void SetEventDate (scid::database::dateT edate) {
        scid::database::uint codedDate = scid::database::date_GetMonth(edate) << 5;
        codedDate |= scid::database::date_GetDay (edate);
        scid::database::uint eyear = scid::database::date_GetYear (edate);
        scid::database::uint dyear = scid::database::date_GetYear (GetDate());
        // Due to a compact encoding format, the EventDate
        // must be within a few years of the Date.
        if ((eyear + 3) < dyear  ||  eyear > (dyear + 3)) {
            codedDate = 0; 
        } else {
            codedDate |= (((eyear + 4 - dyear) & 7) << 9);
        }
        Dates = u32_set_high_12 (Dates, codedDate);
    }
    void SetResult (scid::database::resultT res) {
        VarCounts = (VarCounts & 0x0FFF) | (((scid::database::ushort)res) << 12);
    }
    void SetWhiteElo (scid::database::eloT elo)  {
        WhiteElo = u16_set_low_12(WhiteElo, elo);
    }
    void SetBlackElo (scid::database::eloT elo)  {
        BlackElo = u16_set_low_12 (BlackElo, elo);
    }
    void SetWhiteRatingType (scid::database::byte b) {
        WhiteElo = u16_set_high_4 (WhiteElo, b);
    }
    void SetBlackRatingType (scid::database::byte b) {
        BlackElo = u16_set_high_4 (BlackElo, b);
    }
    void SetEcoCode(scidup::eco::Code eco) { EcoCode = eco; }
    void SetNumHalfMoves (scid::database::ushort b)  { NumHalfMoves = b; }


    bool GetFlag (uint32_t mask) const {
        uint32_t tmp = Flags;
        if ((mask & 0xFFFF0000) != 0) {
            // The if is not necessary but should be faster
            tmp |= (Length_High & 0x3F) << 16;
        }
        return (tmp & mask) == mask;
    }
    bool GetStartFlag () const      { return (Flags & (1 << IDX_FLAG_START)) != 0; }
    bool GetPromotionsFlag () const { return (Flags & (1 << IDX_FLAG_PROMO)) != 0; }
    bool GetUnderPromoFlag() const  { return (Flags & (1 << IDX_FLAG_UPROMO)) != 0; }
    bool GetCommentsFlag () const   { return (GetCommentCount() > 0); }
    bool GetVariationsFlag () const { return (GetVariationCount() > 0); }
    bool GetNagsFlag () const       { return (GetNagCount() > 0); }
    bool GetDeleteFlag () const     { return (Flags & (1 << IDX_FLAG_DELETE)) != 0; }

    static scid::database::uint CharToFlag (char ch);
    static uint32_t CharToFlagMask (char flag);
    static uint32_t StrToFlagMask (const char* flags);
    scid::database::uint GetFlagStr(char* dest, const char* flags) const;

    scid::database::uint GetVariationCount () const { return DecodeCount(VarCounts & 15); }
    scid::database::uint GetCommentCount () const   { return DecodeCount((VarCounts >> 4) & 15); }
    scid::database::uint GetNagCount () const       { return DecodeCount((VarCounts >> 8) & 15); }

    scid::database::matSigT GetFinalMatSig () const { return u32_low_24 (FinalMatSig); }
    scid::database::byte GetStoredLineCode () const { return u32_high_8 (FinalMatSig); }
    const scid::database::byte* GetHomePawnData () const { return HomePawnData; }
    scid::database::byte* GetHomePawnData () { return HomePawnData; }

    void SetFlag (uint32_t flagMask, bool b) {
        uint16_t flagLow = flagMask & 0xFFFF;
        if (flagLow != 0) {
            if (b) { 
                Flags |= flagLow;
            } else {
                Flags &= ~flagLow;
            }
        }

        scid::database::byte flagHigh = (flagMask >> 16) & 0x3F;
        if (flagHigh != 0) {
            if (b) {
                Length_High |= flagHigh;
            } else {
                Length_High &= ~flagHigh;
            }
        }
    }
    void SetStartFlag (bool b)      { SetFlag(1 << IDX_FLAG_START, b); }
    void SetPromotionsFlag (bool b) { SetFlag(1 << IDX_FLAG_PROMO, b); }
    void SetUnderPromoFlag (bool b) { SetFlag(1 << IDX_FLAG_UPROMO, b); }
    void SetDeleteFlag (bool b)     { SetFlag(1 << IDX_FLAG_DELETE, b); }
    void clearFlags() { return SetFlag(IDX_MASK_ALLFLAGS, false); }

    void SetVariationCount (scid::database::uint x) {
        VarCounts = (VarCounts & 0xFFF0U) | EncodeCount(x);
    }
    void SetCommentCount (scid::database::uint x) {
        VarCounts = (VarCounts & 0xFF0FU) | (EncodeCount(x) << 4);
    }
    void SetNagCount (scid::database::uint x) {
        VarCounts = (VarCounts & 0xF0FFU) | (EncodeCount(x) << 8);
    }

    void SetFinalMatSig (scid::database::matSigT ms) {
        FinalMatSig = u32_set_low_24 (FinalMatSig, ms);
    }
    void SetStoredLineCode (scid::database::byte b)    {
        FinalMatSig = u32_set_high_8 (FinalMatSig, b);
    }

    enum {
        // scid::database::IndexEntry Flag types:
        IDX_FLAG_START      =  0,   // scid::database::Game has own start position.
        IDX_FLAG_PROMO      =  1,   // scid::database::Game contains promotion(s).
        IDX_FLAG_UPROMO     =  2,   // scid::database::Game contains promotion(s).
        IDX_FLAG_DELETE     =  3,   // scid::database::Game marked for deletion.
        IDX_FLAG_WHITE_OP   =  4,   // White openings flag.
        IDX_FLAG_BLACK_OP   =  5,   // Black openings flag.
        IDX_FLAG_MIDDLEGAME =  6,   // Middlegames flag.
        IDX_FLAG_ENDGAME    =  7,   // Endgames flag.
        IDX_FLAG_NOVELTY    =  8,   // Novelty flag.
        IDX_FLAG_PAWN       =  9,   // Pawn structure flag.
        IDX_FLAG_TACTICS    = 10,   // Tactics flag.
        IDX_FLAG_KSIDE      = 11,   // Kingside play flag.
        IDX_FLAG_QSIDE      = 12,   // Queenside play flag.
        IDX_FLAG_BRILLIANCY = 13,   // Brilliancy or good play.
        IDX_FLAG_BLUNDER    = 14,   // Blunder or bad play.
        IDX_FLAG_USER       = 15,   // User-defined flag.
        IDX_FLAG_CUSTOM1    = 16,   // Custom flag.
        IDX_FLAG_CUSTOM2    = 17,   // Custom flag.
        IDX_FLAG_CUSTOM3    = 18,   // Custom flag.
        IDX_FLAG_CUSTOM4    = 19,   // Custom flag.
        IDX_FLAG_CUSTOM5    = 20,   // Custom flag.
        IDX_FLAG_CUSTOM6    = 21,   // Custom flag.
        IDX_NUM_FLAGS       = 22,
    };
    static const uint32_t IDX_MASK_ALLFLAGS = 0xFFFFFFFF;

private:
    static scid::database::uint EncodeCount (scid::database::uint x) {
        if (x <= 10) { return x; }
        if (x <= 12) { return 10; }
        if (x <= 17) { return 11; }  // 11 indicates 15 (13-17)
        if (x <= 24) { return 12; }  // 12 indicates 20 (18-24)
        if (x <= 34) { return 13; }  // 13 indicates 30 (25-34)
        if (x <= 44) { return 14; }  // 14 indicates 40 (35-44)
        return 15;                   // 15 indicates 50 or more
    }
    static scid::database::uint DecodeCount (scid::database::uint x) {
        static scid::database::uint countCodes[16] = {0,1,2,3,4,5,6,7,8,9,10,15,20,30,40,50};
        return countCodes[x & 15];
    }

// Bitmask functions for index entry decoding:
    static scid::database::byte u32_high_8( scid::database::uint x )
    {
        return (scid::database::byte)(x >> 24);
    }

    static scid::database::uint u32_low_24( scid::database::uint x )
    {
        return x & 0x00FFFFFF;
    }

    static scid::database::uint u32_high_12( scid::database::uint x )
    {
        return x >> 20;
    }

    static scid::database::uint u32_low_20( scid::database::uint x )
    {
        return x & 0x000FFFFF;
    }

    static scid::database::byte u16_high_4( scid::database::ushort x )
    {
        return (scid::database::byte)(x >> 12);
    }

    static scid::database::ushort u16_low_12( scid::database::ushort x )
    {
        return x & 0x0FFF;
    }

    static scid::database::uint u32_set_high_8( scid::database::uint u, scid::database::byte x )
    {
        return u32_low_24(u) | ((scid::database::uint)x << 24);
    }

    static scid::database::uint u32_set_low_24( scid::database::uint u, scid::database::uint x )
    {
        return (u & 0xFF000000) | (x & 0x00FFFFFF);
    }

    static scid::database::uint u32_set_high_12( scid::database::uint u, scid::database::uint x )
    {
        return u32_low_20(u) | (x << 20);
    }

    static scid::database::uint u32_set_low_20( scid::database::uint u, scid::database::uint x )
    {
        return (u & 0xFFF00000) | (x & 0x000FFFFF);
    }

    static scid::database::ushort u16_set_high_4( scid::database::ushort u, scid::database::byte x )
    {
        return u16_low_12(u) | ((scid::database::ushort)x << 12);
    }

    static scid::database::ushort u16_set_low_12( scid::database::ushort u, scid::database::ushort x )
    {
        return (u & 0xF000) | (x & 0x0FFF);
    }
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::Init():
//        Initialise a single index entry.
//
inline void
IndexEntry::Init ()
{
    NumHalfMoves = 0;
    WhiteID_Low = 0;
    BlackID_Low = 0;
    EventID_Low = 0;
    SiteID_Low = 0;
    RoundID_Low = 0;
    WhiteBlack_High = 0;
    EventSiteRnd_High = 0;
    EcoCode = 0;
    Dates = 0;
    WhiteElo = 0;
    BlackElo = 0;
    FinalMatSig = 0;
    Flags = 0;
    VarCounts = 0;
    Offset = 0;
    Length_Low = 0;
    Length_High = 0;
    SetDate (scid::database::ZERO_DATE);
    SetEventDate (scid::database::ZERO_DATE);
    SetResult (scid::database::RESULT_None);
    SetEcoCode(scidup::eco::ECO_None);
    SetFinalMatSig (scid::database::MATSIG_Empty);
    for (scid::database::uint i=0; i < HPSIG_SIZE; i++) {
        HomePawnData[i] = 0;
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::Read():
//      Reads a single entry's values from an open index file.
//
template <class T> scid::database::errorT
IndexEntry::Read (T* file, scid::database::versionT version)
{
    // Length of each gamefile record and its offset.
    Offset = file->ReadFourBytes ();
    Length_Low = file->ReadTwoBytes ();
    Length_High = (version < 400) ? 0 : file->ReadOneByte();
    Flags = file->ReadTwoBytes (); 

    // White and Black player names:
    WhiteBlack_High = file->ReadOneByte ();
    WhiteID_Low = file->ReadTwoBytes ();
    BlackID_Low = file->ReadTwoBytes ();

    // Event, Site and Round names:
    EventSiteRnd_High = file->ReadOneByte ();
    EventID_Low = file->ReadTwoBytes ();
    SiteID_Low = file->ReadTwoBytes ();
    RoundID_Low = file->ReadTwoBytes ();

    VarCounts = file->ReadTwoBytes();
    EcoCode = file->ReadTwoBytes ();

    // Date and EventDate are stored in four bytes.
    Dates = file->ReadFourBytes();

    // The two ELO ratings and rating types take 2 bytes each.
    WhiteElo = file->ReadTwoBytes ();
    BlackElo = file->ReadTwoBytes ();
    if (GetWhiteElo() > MAX_ELO) { SetWhiteElo(MAX_ELO); }
    if (GetBlackElo() > MAX_ELO) { SetBlackElo(MAX_ELO); }

    FinalMatSig = file->ReadFourBytes ();
    NumHalfMoves = file->ReadOneByte ();

    // Read the 9-scid::database::byte homePawnData array:
    scid::database::byte * pb = HomePawnData;
    // The first scid::database::byte of HomePawnData has high bits of the NumHalfMoves
    // counter in its top two bits:
    scid::database::uint pb0 = file->ReadOneByte();
    *pb = (pb0 & 63);
    pb++;
    NumHalfMoves = NumHalfMoves | ((pb0 >> 6) << 8);
    for (scid::database::uint i2 = 1; i2 < HPSIG_SIZE; i2++) {
        *pb = file->ReadOneByte ();
        pb++;
    }

    // Top 2 bits of HomePawnData[0] are for NumHalfMoves:
    scid::database::uint numMoves_High = HomePawnData[0];
    HomePawnData[0] = HomePawnData[0] & 63;
    numMoves_High >>= 6;
    numMoves_High <<= 8;
    NumHalfMoves = NumHalfMoves | numMoves_High;

    return scid::database::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::Write():
//      Writes a single index entry to an open index file.
//      INDEX_ENTRY_SIZE must be updated
template <class T> scid::database::errorT
IndexEntry::Write (T* file, scid::database::versionT version) const
{
    // Cannot write old-version index files:
    if (version < 400) { return scid::database::ERROR_FileVersion; }

    version = 0;  // We don't have any version-specific code.
    
    file->WriteFourBytes (Offset);
    
    file->WriteTwoBytes (Length_Low);
    file->WriteOneByte (Length_High);
    file->WriteTwoBytes (Flags);

    file->WriteOneByte (WhiteBlack_High);
    file->WriteTwoBytes (WhiteID_Low);
    file->WriteTwoBytes (BlackID_Low);

    file->WriteOneByte (EventSiteRnd_High);
    file->WriteTwoBytes (EventID_Low);
    file->WriteTwoBytes (SiteID_Low);
    file->WriteTwoBytes (RoundID_Low);

    file->WriteTwoBytes (VarCounts);
    file->WriteTwoBytes (EcoCode);
    file->WriteFourBytes (Dates);

    // Elo ratings and rating types: 2 bytes each.
    file->WriteTwoBytes (WhiteElo);
    file->WriteTwoBytes (BlackElo);

    file->WriteFourBytes (FinalMatSig);
    file->WriteOneByte (NumHalfMoves & 255); 

    // Write the 9-scid::database::byte homePawnData array:
    const scid::database::byte* pb = HomePawnData;
    // The first scid::database::byte of HomePawnData has high bits of the NumHalfMoves
    // counter in its top two bits:
    scid::database::byte pb0 = *pb;
    pb0 = pb0 | ((NumHalfMoves >> 8) << 6);
    file->WriteOneByte (pb0);
    pb++;
    // write 8 bytes
    for (scid::database::uint i2 = 1; i2 < HPSIG_SIZE; i2++) {
        file->WriteOneByte (*pb);
        pb++;
    }

    return scid::database::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::CharToFlag():
//    Returns the flag number corresponding to the given character.
inline scid::database::uint
IndexEntry::CharToFlag (char ch)
{
    scid::database::uint flag = 0;
    switch (toupper(ch)) {
        case 'D': flag = IDX_FLAG_DELETE;     break;
        case 'W': flag = IDX_FLAG_WHITE_OP;   break;
        case 'B': flag = IDX_FLAG_BLACK_OP;   break;
        case 'M': flag = IDX_FLAG_MIDDLEGAME; break;
        case 'E': flag = IDX_FLAG_ENDGAME;    break;
        case 'N': flag = IDX_FLAG_NOVELTY;    break;
        case 'P': flag = IDX_FLAG_PAWN;       break;
        case 'T': flag = IDX_FLAG_TACTICS;    break;
        case 'K': flag = IDX_FLAG_KSIDE;      break;
        case 'Q': flag = IDX_FLAG_QSIDE;      break;
        case '!': flag = IDX_FLAG_BRILLIANCY; break;
        case '?': flag = IDX_FLAG_BLUNDER;    break;
        case 'U': flag = IDX_FLAG_USER;       break;
        case '1': flag = IDX_FLAG_CUSTOM1;    break;
        case '2': flag = IDX_FLAG_CUSTOM2;    break;
        case '3': flag = IDX_FLAG_CUSTOM3;    break;
        case '4': flag = IDX_FLAG_CUSTOM4;    break;
        case '5': flag = IDX_FLAG_CUSTOM5;    break;
        case '6': flag = IDX_FLAG_CUSTOM6;    break;
    }
    return flag;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::CharToFlagMask():
//    Transform a char in a mask that can be used with GetFlag() and SetFlag()
inline uint32_t IndexEntry::CharToFlagMask(char flag)
{
    switch (toupper(flag)) {
        case 'S': return 1 << IDX_FLAG_START;
        case 'X': return 1 << IDX_FLAG_PROMO;
        case 'Y': return 1 << IDX_FLAG_UPROMO;
        case 'D': return 1 << IDX_FLAG_DELETE;
        case 'W': return 1 << IDX_FLAG_WHITE_OP;
        case 'B': return 1 << IDX_FLAG_BLACK_OP;
        case 'M': return 1 << IDX_FLAG_MIDDLEGAME;
        case 'E': return 1 << IDX_FLAG_ENDGAME;
        case 'N': return 1 << IDX_FLAG_NOVELTY;
        case 'P': return 1 << IDX_FLAG_PAWN;
        case 'T': return 1 << IDX_FLAG_TACTICS;
        case 'K': return 1 << IDX_FLAG_KSIDE;
        case 'Q': return 1 << IDX_FLAG_QSIDE;
        case '!': return 1 << IDX_FLAG_BRILLIANCY;
        case '?': return 1 << IDX_FLAG_BLUNDER;
        case 'U': return 1 << IDX_FLAG_USER;
        case '1': return 1 << IDX_FLAG_CUSTOM1;
        case '2': return 1 << IDX_FLAG_CUSTOM2;
        case '3': return 1 << IDX_FLAG_CUSTOM3;
        case '4': return 1 << IDX_FLAG_CUSTOM4;
        case '5': return 1 << IDX_FLAG_CUSTOM5;
        case '6': return 1 << IDX_FLAG_CUSTOM6;
    }

    ASSERT(0);
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::StrToFlagMask():
//    Transform a string in a mask that can be used with GetFlag() and SetFlag()
inline uint32_t IndexEntry::StrToFlagMask(const char* flags)
{
    if (flags == 0) return 0;

    uint32_t res = 0;
    while (*flags != 0) {
        res |= IndexEntry::CharToFlagMask(*(flags++));
    }
    return res;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::IndexEntry::GetFlagStr():
//    Fills in the provided flag string with information on the
//    user-settable flags set for this game.
//    Returns the number of specified flags that are turned on.
inline scid::database::uint
IndexEntry::GetFlagStr(char* dest, const char* flags) const
{
    if (flags == NULL) { flags = "DWBMENPTKQ!?U123456"; }
    scid::database::uint count = 0;
    while (*flags != 0) {
        uint32_t mask = CharToFlagMask(*flags);
        ASSERT(mask != 0);
        if (GetFlag(mask)) {
            *dest++ = *flags;
            count++;
        }
        flags++;
    }
    *dest = 0;
    return count;
}

#endif
