/*
 * Copyright (c) 2008-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef DIR_ENTRY_H
#define DIR_ENTRY_H

#include "foundation/PxPreprocessor.h"

#if defined PX_PS4
#	include <sys/dirent.h>
#	include <fcntl.h>
#	include <_fs.h>
#else
#	error Unsupported platform
#endif

namespace physx
{
	class DirEntry
	{
	public:

		DirEntry()
		{
			mDirEntBuffer = NULL;
			mDir = 0;
			mEntry = NULL;
			mIdx = 0;
			mCount = 0;
			mDirEntryOffset = 0;
		}

		~DirEntry()
		{
			if (!isDone())
			{
				while (next());
			}

			// The Find.cpp loop behaves badly and doesn't cleanup the DIR pointer
			if (mDir)
			{
				sceKernelClose(mDir);
				mDir = 0;
			}
			if (mDirEntBuffer)
			{
				::free(mDirEntBuffer);
				mDirEntBuffer = NULL;
			}
			mEntry = NULL;
		}

		// Get successive element of directory.
		// Returns true on success, error otherwise.
		bool next()
		{
			if (mIdx < mCount)
			{
				mDirEntryOffset += mEntry->d_reclen;
				mEntry = (SceKernelDirent*)(mDirEntBuffer+mDirEntryOffset);
				++mIdx;
			}
			else
			{
				if (mDir)
				{
					sceKernelClose(mDir);
					mDir = 0;
				}
				if (mDirEntBuffer)
				{
					::free(mDirEntBuffer);
					mDirEntBuffer = NULL;
				}

				mEntry = NULL;
			}
			return true;
		}

		// No more entries in directory?
		bool isDone() const
		{
			return mIdx >= mCount;
		}

		// Is this entry a directory?
		bool isDirectory() const
		{
			if (mEntry)
			{
				return DT_DIR == mEntry->d_type;
			}
			else
			{
				return false;
			}
		}

		// Get name of this entry.
		const char* getName() const
		{
			if (mEntry)
			{
				return mEntry->d_name;
			}
			else
			{
				return NULL;
			}
		}

		// Get first entry in directory.
		static bool GetFirstEntry(const char* path, DirEntry& dentry)
		{
			// open directory
			dentry.mDir = sceKernelOpen(path, O_RDONLY, ALLPERMS);
			if (dentry.mDir < 0)
			{
				return false;
			}

			// get info on the block size using fstat
			SceKernelStat s;
			int statRet = sceKernelFstat(dentry.mDir, &s);
			if (statRet < 0)
			{
				sceKernelClose(dentry.mDir);
				dentry.mDir = 0;
				return false;
			}

			// allocate a buffer large enough for the dirents
			dentry.mDirEntBuffer = (char*)::malloc(s.st_blksize);
			if (!dentry.mDirEntBuffer)
			{
				sceKernelClose(dentry.mDir);
				dentry.mDir = 0;
				::free(dentry.mDirEntBuffer);
				dentry.mDirEntBuffer = NULL;
				return false;
			}

			// count files
			dentry.mCount = 0;
  			if (dentry.mDir != 0)
  			{
				// this returns the bytes used by dirent structs, stored in the buffer
				dentry.mDirEntryMaxOffset = sceKernelGetdents(dentry.mDir, dentry.mDirEntBuffer, s.st_blksize);

				dentry.mEntry = (SceKernelDirent*)dentry.mDirEntBuffer;
				dentry.mDirEntryOffset = 0;
				if (dentry.mDirEntryMaxOffset < (int)s.st_blksize)
				{
					while (dentry.mEntry->d_reclen != 0 && dentry.mDirEntryOffset < dentry.mDirEntryMaxOffset)
					{
						//printf("Entry name: %s\n", dentry.mEntry->d_name);
						dentry.mDirEntryOffset += dentry.mEntry->d_reclen;
						dentry.mEntry = (SceKernelDirent*)(dentry.mDirEntBuffer+dentry.mDirEntryOffset);
						dentry.mCount++;
					}
				}
				// There wasn't enough room in the buffer to get all of the entries, this
				// won't actually happen, it currently just doesn't read everything in the folder.
				// There has to be hundreds of files in the folder, skip making this work for now.
				else 
				{
					sceKernelClose(dentry.mDir);
					dentry.mDir = 0;
					::free(dentry.mDirEntBuffer);
					dentry.mDirEntBuffer = NULL;
					return false;
				}
			}

			dentry.mDirEntryOffset = 0;
			dentry.mEntry = (SceKernelDirent*)dentry.mDirEntBuffer;

			return true;
		}

	private:

		int mDir;
		char *mDirEntBuffer;
		SceKernelDirent* mEntry;
		int mIdx, mCount, mDirEntryOffset, mDirEntryMaxOffset;
	};
}

#endif
