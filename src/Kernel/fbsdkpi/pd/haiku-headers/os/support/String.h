/*
 * Copyright 2001-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _B_STRING_H
#define _B_STRING_H


#include <stdarg.h>
#include <string.h>

#include <SupportDefs.h>


class BStringList;
class BStringRef;


class BString {
public:
							BString();
							BString(const char* string);
							BString(const BString& string);
							BString(const char* string, int32 maxLength);
#if __cplusplus >= 201103L
							BString(BString&& string) noexcept;
#endif
							~BString();

			// Access
			const char*		String() const;
			int32 			Length() const;
			int32			CountChars() const;
			int32			CountBytes(int32 fromCharOffset,
								int32 charCount) const;
			bool			IsEmpty() const;

			uint32			HashValue() const;
	static	uint32			HashValue(const char* string);

			// Assignment
			BString&		operator=(const BString& string);
			BString&		operator=(const char* string);
			BString&		operator=(char c);
#if __cplusplus >= 201103L
			BString&		operator=(BString&& string) noexcept;
#endif

			BString&		SetTo(const char* string);
			BString&		SetTo(const char* string, int32 maxLength);

			BString&		SetTo(const BString& string);
			BString&		Adopt(BString& from);

			BString&		SetTo(const BString& string, int32 maxLength);
			BString&		Adopt(BString& from, int32 maxLength);

			BString&		SetTo(char c, int32 count);

			BString&		SetToChars(const char* string, int32 charCount);
			BString&		SetToChars(const BString& string, int32 charCount);
			BString&		AdoptChars(BString& from, int32 charCount);

			BString&		SetToFormat(const char* format, ...)
								__attribute__((__format__(__printf__, 2, 3)));
			BString&		SetToFormatVarArgs(const char* format,
								va_list args)
								__attribute__((__format__(__printf__, 2, 0)));

			int				ScanWithFormat(const char* format, ...)
								__attribute__((__format__(__scanf__, 2, 3)));
			int				ScanWithFormatVarArgs(const char* format,
								va_list args)
								__attribute__((__format__(__scanf__, 2, 0)));

			// Substring copying
			BString&		CopyInto(BString& into, int32 fromOffset,
								int32 length) const;
			void			CopyInto(char* into, int32 fromOffset,
								int32 length) const;

			BString&		CopyCharsInto(BString& into, int32 fromCharOffset,
								int32 charCount) const;
			bool			CopyCharsInto(char* into, int32* intoLength,
								int32 fromCharOffset, int32 charCount) const;

			bool			Split(const char* separator, bool noEmptyStrings,
								BStringList& _list) const;

			// Appending
			BString&		operator+=(const BString& string);
			BString&		operator+=(const char* string);
			BString&		operator+=(char c);

			BString&		Append(const BString& string);
			BString&		Append(const char* string);

			BString&		Append(const BString& string, int32 length);
			BString&		Append(const char* string, int32 length);
			BString&		Append(char c, int32 count);

			BString&		AppendChars(const BString& string, int32 charCount);
			BString&		AppendChars(const char* string, int32 charCount);

			// Prepending
			BString&		Prepend(const char* string);
			BString&		Prepend(const BString& string);
			BString&		Prepend(const char* string, int32 length);
			BString&		Prepend(const BString& string, int32 length);
			BString&		Prepend(char c, int32 count);

			BString&		PrependChars(const char* string, int32 charCount);
			BString&		PrependChars(const BString& string,
								int32 charCount);

			// Inserting
			BString&		Insert(const char* string, int32 position);
			BString&		Insert(const char* string, int32 length,
								int32 position);
			BString&		Insert(const char* string, int32 fromOffset,
								int32 length, int32 position);
			BString&		Insert(const BString& string, int32 position);
			BString&		Insert(const BString& string, int32 length,
								int32 position);
			BString&		Insert(const BString& string, int32 fromOffset,
								int32 length, int32 position);
			BString&		Insert(char c, int32 count, int32 position);

			BString&		InsertChars(const char* string, int32 charPosition);
			BString&		InsertChars(const char* string, int32 charCount,
								int32 charPosition);
			BString&		InsertChars(const char* string,
								int32 fromCharOffset, int32 charCount,
								int32 charPosition);
			BString&		InsertChars(const BString& string,
								int32 charPosition);
			BString&		InsertChars(const BString& string, int32 charCount,
								int32 charPosition);
			BString&		InsertChars(const BString& string,
								int32 fromCharOffset, int32 charCount,
								int32 charPosition);

			// Removing
			BString&		Truncate(int32 newLength, bool lazy = true);
			BString&		TruncateChars(int32 newCharCount, bool lazy = true);

			BString&		Remove(int32 from, int32 length);
			BString&		RemoveChars(int32 fromCharOffset, int32 charCount);

			BString&		RemoveFirst(const BString& string);
			BString&		RemoveLast(const BString& string);
			BString&		RemoveAll(const BString& string);

			BString&		RemoveFirst(const char* string);
			BString&		RemoveLast(const char* string);
			BString&		RemoveAll(const char* string);

			BString&		RemoveSet(const char* setOfBytesToRemove);
			BString&		RemoveCharsSet(const char* setOfCharsToRemove);

			BString&		MoveInto(BString& into, int32 from, int32 length);
			void			MoveInto(char* into, int32 from, int32 length);

			BString&		MoveCharsInto(BString& into, int32 fromCharOffset,
								int32 charCount);
			bool			MoveCharsInto(char* into, int32* intoLength,
								int32 fromCharOffset, int32 charCount);

			// Compare functions
			bool			operator<(const BString& string) const;
			bool			operator<=(const BString& string) const;
			bool			operator==(const BString& string) const;
			bool			operator>=(const BString& string) const;
			bool			operator>(const BString& string) const;
			bool			operator!=(const BString& string) const;

			bool			operator<(const char* string) const;
			bool			operator<=(const char* string) const;
			bool			operator==(const char* string) const;
			bool			operator>=(const char* string) const;
			bool			operator>(const char* string) const;
			bool			operator!=(const char* string) const;

							operator const char*() const;

			// strcmp()-style compare functions
			int				Compare(const BString& string) const;
			int				Compare(const char* string) const;
			int				Compare(const BString& string, int32 length) const;
			int				Compare(const char* string, int32 length) const;

			int				CompareAt(size_t offset, const BString& string,
								int32 length) const;

			int				CompareChars(const BString& string,
								int32 charCount) const;
			int				CompareChars(const char* string,
								int32 charCount) const;

			int				ICompare(const BString& string) const;
			int				ICompare(const char* string) const;
			int				ICompare(const BString& string, int32 length) const;
			int				ICompare(const char* string, int32 length) const;

			// Searching
			int32			FindFirst(const BString& string) const;
			int32			FindFirst(const char* string) const;
			int32			FindFirst(const BString& string,
								int32 fromOffset) const;
			int32			FindFirst(const char* string,
								int32 fromOffset) const;
			int32			FindFirst(char c) const;
			int32			FindFirst(char c, int32 fromOffset) const;

			int32			FindFirstChars(const BString& string,
								int32 fromCharOffset) const;
			int32			FindFirstChars(const char* string,
								int32 fromCharOffset) const;

			int32			FindLast(const BString& string) const;
			int32			FindLast(const char* string) const;
			int32			FindLast(const BString& string,
								int32 beforeOffset) const;
			int32			FindLast(const char* string,
								int32 beforeOffset) const;
			int32			FindLast(char c) const;
			int32			FindLast(char c, int32 beforeOffset) const;

			int32			FindLastChars(const BString& string,
								int32 beforeCharOffset) const;
			int32			FindLastChars(const char* string,
								int32 beforeCharOffset) const;

			int32			IFindFirst(const BString& string) const;
			int32			IFindFirst(const char* string) const;
			int32			IFindFirst(const BString& string,
								int32 fromOffset) const;
			int32			IFindFirst(const char* string,
								int32 fromOffset) const;

			int32			IFindLast(const BString& string) const;
			int32			IFindLast(const char* string) const;
			int32			IFindLast(const BString& string,
								int32 beforeOffset) const;
			int32			IFindLast(const char* string,
								int32 beforeOffset) const;

			bool			StartsWith(const BString& string) const;
			bool			StartsWith(const char* string) const;
			bool			StartsWith(const char* string, int32 length) const;

			bool			IStartsWith(const BString& string) const;
			bool			IStartsWith(const char* string) const;
			bool			IStartsWith(const char* string, int32 length) const;

			bool			EndsWith(const BString& string) const;
			bool			EndsWith(const char* string) const;
			bool			EndsWith(const char* string, int32 length) const;

			bool			IEndsWith(const BString& string) const;
			bool			IEndsWith(const char* string) const;
			bool			IEndsWith(const char* string, int32 length) const;

			// Replacing
			BString&		ReplaceFirst(char replaceThis, char withThis);
			BString&		ReplaceLast(char replaceThis, char withThis);
			BString&		ReplaceAll(char replaceThis, char withThis,
								int32 fromOffset = 0);
			BString&		Replace(char replaceThis, char withThis,
								int32 maxReplaceCount, int32 fromOffset = 0);
			BString&		ReplaceFirst(const char* replaceThis,
								const char* withThis);
			BString&		ReplaceLast(const char* replaceThis,
								const char* withThis);
			BString&		ReplaceAll(const char* replaceThis,
								const char* withThis, int32 fromOffset = 0);
			BString&		Replace(const char* replaceThis,
								const char* withThis, int32 maxReplaceCount,
								int32 fromOffset = 0);

			BString&		ReplaceAllChars(const char* replaceThis,
								const char* withThis, int32 fromCharOffset);
			BString&		ReplaceChars(const char* replaceThis,
								const char* withThis, int32 maxReplaceCount,
								int32 fromCharOffset);

			BString&		IReplaceFirst(char replaceThis, char withThis);
			BString&		IReplaceLast(char replaceThis, char withThis);
			BString&		IReplaceAll(char replaceThis, char withThis,
								int32 fromOffset = 0);
			BString&		IReplace(char replaceThis, char withThis,
								int32 maxReplaceCount, int32 fromOffset = 0);
			BString&		IReplaceFirst(const char* replaceThis,
								const char* withThis);
			BString&		IReplaceLast(const char* replaceThis,
								const char* withThis);
			BString&		IReplaceAll(const char* replaceThis,
								const char* withThis, int32 fromOffset = 0);
			BString&		IReplace(const char* replaceThis,
								const char* withThis, int32 maxReplaceCount,
								int32 fromOffset = 0);

			BString&		ReplaceSet(const char* setOfBytes, char with);
			BString&		ReplaceSet(const char* setOfBytes,
								const char* with);

			BString&		ReplaceCharsSet(const char* setOfChars,
								const char* with);

			// Unchecked char access
			char			operator[](int32 index) const;

#if __GNUC__ == 2
			char&			operator[](int32 index);
#endif

			// Checked char access
			char			ByteAt(int32 index) const;
			const char*		CharAt(int32 charIndex, int32* bytes = NULL) const;
			bool			CharAt(int32 charIndex, char* buffer,
								int32* bytes) const;

			// Fast low-level manipulation
			char*			LockBuffer(int32 maxLength);
			BString&		UnlockBuffer(int32 length = -1);
			BString&		SetByteAt(int32 pos, char to);

			// Upercase <-> Lowercase
			BString&		ToLower();
			BString&		ToUpper();

			BString&		Capitalize();
			BString&		CapitalizeEachWord();

			// Escaping and De-escaping
			BString&		CharacterEscape(const char* original,
								const char* setOfCharsToEscape,
								char escapeWith);
			BString&		CharacterEscape(const char* setOfCharsToEscape,
								char escapeWith);
			BString&		CharacterDeescape(const char* original,
								char escapeChar);
			BString&		CharacterDeescape(char escapeChar);

			// Trimming
			BString&		Trim();

			// Insert
			BString&		operator<<(const char* string);
			BString&		operator<<(const BString& string);
			BString&		operator<<(char c);
			BString&		operator<<(bool value);
			BString&		operator<<(int value);
			BString&		operator<<(unsigned int value);
			BString&		operator<<(unsigned long value);
			BString&		operator<<(long value);
			BString&		operator<<(unsigned long long value);
			BString&		operator<<(long long value);
			// float/double output hardcodes %.2f style formatting
			BString&		operator<<(float value);
			BString&		operator<<(double value);

public:
			class Private;
			friend class Private;

private:
			class PosVect;
			friend class BStringRef;

			enum PrivateDataTag {
				PRIVATE_DATA
			};

private:
							BString(char* privateData, PrivateDataTag tag);

			// Management
			status_t		_MakeWritable();
			status_t		_MakeWritable(int32 length, bool copy);
	static	char*			_Allocate(int32 length);
			char*			_Resize(int32 length);
			void			_Init(const char* src, int32 length);
			char*			_Clone(const char* data, int32 length);
			char*			_OpenAtBy(int32 offset, int32 length);
			char*			_ShrinkAtBy(int32 offset, int32 length);

			// Data
			void			_SetLength(int32 length);
			bool			_DoAppend(const char* string, int32 length);
			bool			_DoPrepend(const char* string, int32 length);
			bool			_DoInsert(const char* string, int32 offset,
								int32 length);

			// Search
			int32			_ShortFindAfter(const char* string,
								int32 length) const;
			int32			_FindAfter(const char* string, int32 offset,
								int32 length) const;
			int32			_IFindAfter(const char* string, int32 offset,
								int32 length) const;
			int32			_FindBefore(const char* string, int32 offset,
								int32 length) const;
			int32			_IFindBefore(const char* string, int32 offset,
								int32 length) const;

			// Escape
			BString&		_DoCharacterEscape(const char* string,
								const char* setOfCharsToEscape, char escapeChar);
			BString&		_DoCharacterDeescape(const char* string,
								char escapeChar);

			// Replace
			BString&		_DoReplace(const char* findThis,
								const char* replaceWith, int32 maxReplaceCount,
								int32 fromOffset, bool ignoreCase);
			void			_ReplaceAtPositions(const PosVect* positions,
								int32 searchLength, const char* with,
								int32 withLength);

private:
			int32& 			_ReferenceCount();
			const int32& 	_ReferenceCount() const;
			bool			_IsShareable() const;
			void			_FreePrivateData();
			void			_ReleasePrivateData();

			char*			fPrivateData;
};


// Commutative compare operators
bool operator<(const char* a, const BString& b);
bool operator<=(const char* a, const BString& b);
bool operator==(const char* a, const BString& b);
bool operator>(const char* a, const BString& b);
bool operator>=(const char* a, const BString& b);
bool operator!=(const char* a, const BString& b);


// Non-member compare for sorting, etc.
int Compare(const BString& a, const BString& b);
int ICompare(const BString& a, const BString& b);
int Compare(const BString* a, const BString* b);
int ICompare(const BString* a, const BString* b);


inline int32
BString::Length() const
{
	// the most significant bit is reserved; accessing
	// it in any way will cause the computer to explode
	return fPrivateData ? (*(((int32*)fPrivateData) - 1) & 0x7fffffff) : 0;
}


inline bool
BString::IsEmpty() const
{
	return !Length();
}


inline const char*
BString::String() const
{
	if (!fPrivateData)
		return "";
	return fPrivateData;
}


inline uint32
BString::HashValue() const
{
	return HashValue(String());
}


inline BString&
BString::SetTo(const char* string)
{
	return operator=(string);
}


inline char
BString::operator[](int32 index) const
{
	return fPrivateData[index];
}


inline char
BString::ByteAt(int32 index) const
{
	if (!fPrivateData || index < 0 || index >= Length())
		return 0;
	return fPrivateData[index];
}


inline BString&
BString::operator+=(const BString& string)
{
	_DoAppend(string.String(), string.Length());
	return *this;
}


inline BString&
BString::Append(const BString& string)
{
	_DoAppend(string.String(), string.Length());
	return *this;
}


inline BString&
BString::Append(const char* string)
{
	return operator+=(string);
}


inline bool
BString::operator==(const BString& string) const
{
	return strcmp(String(), string.String()) == 0;
}


inline bool
BString::operator<(const BString& string) const
{
	return strcmp(String(), string.String()) < 0;
}


inline bool
BString::operator<=(const BString& string) const
{
	return strcmp(String(), string.String()) <= 0;
}


inline bool
BString::operator>=(const BString& string) const
{
	return strcmp(String(), string.String()) >= 0;
}


inline bool
BString::operator>(const BString& string) const
{
	return strcmp(String(), string.String()) > 0;
}


inline bool
BString::operator!=(const BString& string) const
{
	return strcmp(String(), string.String()) != 0;
}


inline bool
BString::operator!=(const char* string) const
{
	return !operator==(string);
}


inline
BString::operator const char*() const
{
	return String();
}


inline bool
operator<(const char* str, const BString& string)
{
	return string > str;
}


inline bool
operator<=(const char* str, const BString& string)
{
	return string >= str;
}


inline bool
operator==(const char* str, const BString& string)
{
	return string == str;
}


inline bool
operator>(const char* str, const BString& string)
{
	return string < str;
}


inline bool
operator>=(const char* str, const BString& string)
{
	return string <= str;
}


inline bool
operator!=(const char* str, const BString& string)
{
	return string != str;
}


#endif	// _B_STRING_H
