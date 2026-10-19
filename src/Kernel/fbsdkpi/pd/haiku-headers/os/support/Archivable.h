/*
 * Copyright 2001-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCHIVABLE_H
#define _ARCHIVABLE_H


#include <image.h>
#include <Message.h>
#include <SupportDefs.h>


class BMessage;


namespace BPrivate {
namespace Archiving {
	class BArchiveManager;
	class BUnarchiveManager;
}
}

using ::BPrivate::Archiving::BArchiveManager;
using ::BPrivate::Archiving::BUnarchiveManager;


class BArchivable {
public:
								BArchivable(BMessage* from);
								BArchivable();
	virtual 					~BArchivable();

	virtual	status_t 			Archive(BMessage* into, bool deep = true) const;
	static 	BArchivable*		Instantiate(BMessage* archive);

	virtual status_t			Perform(perform_code d, void* arg);

	virtual	status_t 			AllUnarchived(const BMessage* archive);
	virtual	status_t 			AllArchived(BMessage* archive) const;

private:
	friend 	class BUnarchiveManager;

	virtual	void				_ReservedArchivable3();

			int32				fArchivingToken;
			uint32				_reserved;
};


class BArchiver {
public:
								BArchiver(BMessage* archive);
								~BArchiver();

			status_t			AddArchivable(const char* name,
									BArchivable* archivable, bool deep = true);

	inline	status_t			GetTokenForArchivable(BArchivable* archivable,
									int32& _token);
			status_t			GetTokenForArchivable(BArchivable* archivable,
									bool deep, int32& _token);

			bool				IsArchived(BArchivable* archivable);
			status_t			Finish(status_t err = B_OK);
			BMessage*			ArchiveMessage() const;

private:
	friend class BArchivable;

								BArchiver(); // not defined
								BArchiver(const BArchiver&); // not defined

			void				RegisterArchivable(
									const BArchivable* archivable);

			BArchiveManager*	fManager;
			BMessage*			fArchive;
			bool				fFinished;
			uint32				_reserved[2];
};


class BUnarchiver {
public:
			enum ownership_policy {
				B_ASSUME_OWNERSHIP,
				B_DONT_ASSUME_OWNERSHIP
			};

								BUnarchiver(const BMessage* archive);
								~BUnarchiver();

	template<class T>
	inline	status_t			GetObject(int32 token, T*& object);

	template<class T>
			status_t			GetObject(int32 token, ownership_policy owning,
									T*& object);

	template<class T>
	inline	status_t			FindObject(const char* name, T*& object);

	template<class T>
	inline	status_t			FindObject(const char* name,
									ownership_policy owning,
									T*& object);

	template<class T>
	inline	status_t			FindObject(const char* name,
									int32 index, T*& object);

	template<class T>
			status_t			FindObject(const char* name, int32 index,
									ownership_policy owning, T*& object);

	inline	status_t			EnsureUnarchived(const char* name,
									int32 index = 0);
	inline	status_t			EnsureUnarchived(int32 token);

			bool				IsInstantiated(int32 token);
			bool				IsInstantiated(const char* name,
									int32 index = 0);

			status_t			Finish(status_t err = B_OK);
			const BMessage*		ArchiveMessage() const;

			void				AssumeOwnership(BArchivable* archivable);
			void				RelinquishOwnership(BArchivable* archivable);

	static	bool				IsArchiveManaged(const BMessage* archive);
	static	BMessage*			PrepareArchive(BMessage*& archive);

	template<class T>
	static	status_t			InstantiateObject(BMessage* archive,
									T*& object);

private:
	friend class BArchivable;

								BUnarchiver(); // not defined
								BUnarchiver(const BUnarchiver&); // not defined

			void				RegisterArchivable(BArchivable* archivable);

	inline	void				_CallDebuggerIfManagerNull();

			BUnarchiveManager*	fManager;
			const BMessage*		fArchive;
			bool				fFinished;
			uint32				_reserved[2];
};


// global functions

typedef BArchivable* (*instantiation_func)(BMessage*);

BArchivable* instantiate_object(BMessage* from, image_id* id);
BArchivable* instantiate_object(BMessage* from);
bool validate_instantiation(BMessage* from, const char* className);

instantiation_func find_instantiation_func(const char* className,
	const char* signature);
instantiation_func find_instantiation_func(const char* className);
instantiation_func find_instantiation_func(BMessage* archive);


status_t
BArchiver::GetTokenForArchivable(BArchivable* archivable, int32& _token)
{
	return GetTokenForArchivable(archivable, true, _token);
}


template<>
status_t BUnarchiver::FindObject<BArchivable>(const char* name, int32 index,
	ownership_policy owning, BArchivable*& archivable);


template<class T>
status_t
BUnarchiver::FindObject(const char* name, int32 index,
	ownership_policy owning, T*& object)
{
	object = NULL;

	BArchivable* interim;
	status_t err = FindObject(name, index, owning, interim);

	if (err == B_OK && interim) {
		object = dynamic_cast<T*>(interim);
		if (!object) {
			err = B_BAD_TYPE;
			// we will not be deleting this item, but it must be deleted
			if (owning == B_ASSUME_OWNERSHIP)
				RelinquishOwnership(interim);
		}
	}
	return err;
}


template<>
status_t
BUnarchiver::GetObject<BArchivable>(int32 token,
	ownership_policy owning, BArchivable*& object);


template<class T>
status_t
BUnarchiver::GetObject(int32 token, ownership_policy owning, T*& object)
{
	object = NULL;

	BArchivable* interim;
	status_t err = GetObject(token, owning, interim);

	if (err == B_OK && interim) {
		object = dynamic_cast<T*>(interim);
		if (!object) {
			err = B_BAD_TYPE;
			// we will not be deleting this item, but it must be deleted
			if (owning == B_ASSUME_OWNERSHIP)
				RelinquishOwnership(interim);
		}
	}
	return err;
}


template<class T>
status_t
BUnarchiver::GetObject(int32 token, T*& object)
{
	return GetObject<T>(token, B_ASSUME_OWNERSHIP, object);
}


template<class T>
status_t
BUnarchiver::FindObject(const char* name, ownership_policy owning, T*& object)
{
	return FindObject(name, 0, owning, object);
}


template<class T>
status_t
BUnarchiver::FindObject(const char* name, T*& object)
{
	return FindObject<T>(name, 0, B_ASSUME_OWNERSHIP, object);
}


template<class T>
status_t
BUnarchiver::FindObject(const char* name,
	int32 index, T*& object)
{
	return FindObject(name, index, B_ASSUME_OWNERSHIP, object);
}


status_t
BUnarchiver::EnsureUnarchived(int32 token)
{
	BArchivable* dummy;
	return GetObject(token, B_DONT_ASSUME_OWNERSHIP, dummy);
}


status_t
BUnarchiver::EnsureUnarchived(const char* name, int32 index)
{
	BArchivable* dummy;
	return FindObject(name, index, B_DONT_ASSUME_OWNERSHIP, dummy);
}


template<>
status_t
BUnarchiver::InstantiateObject<BArchivable>(BMessage* from,
	BArchivable*& object);


template<class T>
status_t
BUnarchiver::InstantiateObject(BMessage* archive, T*& object)
{
	object = NULL;

	BArchivable* interim;
	status_t err = InstantiateObject(archive, interim);
	if (err != B_OK || interim == NULL)
		return err;

	object = dynamic_cast<T*>(interim);
	if (object == NULL) {
		delete interim;
		return B_BAD_TYPE;
	}

	return B_OK;
}


#endif	// _ARCHIVABLE_H
