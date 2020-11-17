/*
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the author(s) be held liable for any damages arising from
	the use of this software.

	Permission is granted to anyone to use this software for any purpose, including
	commercial applications, and to alter it and redistribute it freely, subject to
	the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Author: Stewart Lynch
	www.puredevsoftware.com
	slynch@puredevsoftware.com

	This code is released to the public domain, as explained at
	http://creativecommons.org/publicdomain/zero/1.0/

	MemProLib is the library that allows the MemPro application to communicate
	with your application.
*/
//------------------------------------------------------------------------
#ifndef MEMPRO_MEMPROPS4_H_INCLUDED
#define MEMPRO_MEMPROPS4_H_INCLUDED

//------------------------------------------------------------------------
#if defined(__UNREAL__)
	#include "MemPro/MemPro.h"
#else
	#include "MemPro.hpp"
#endif

//------------------------------------------------------------------------
#if MEMPRO_ENABLED

//------------------------------------------------------------------------
#ifdef MEMPRO_PLATFORM_PS4

//------------------------------------------------------------------------
#define MEMPRO_INTERLOCKED_ALIGN
#define MEMPRO_INSTRUCTION_BARRIER
#define MEMPRO_PUSH_WARNING_DISABLE
#define MEMPRO_DISABLE_WARNING(w)
#define MEMPRO_POP_WARNING_DISABLE
#define MEMPRO_FORCEINLINE inline
#define MEMPRO64
#define ENUMERATE_ALL_MODULES
#define THREAD_LOCAL_STORAGE __thread
#define MEMPRO_PORT "27016"
#define STACK_TRACE_SIZE 128
#define MEMPRO_ALIGN_SUFFIX(n) __attribute__ ((aligned(n)))

#ifdef OVERRIDE_NEW_DELETE
	void *user_new(std::size_t size) throw(std::bad_alloc)
	{
		void* p = malloc(size);
		MEMPRO_TRACK_ALLOC(p, size);
		return p;
	}

	void *user_new(std::size_t size, const std::nothrow_t& x) throw()
	{
		void* p = malloc(size);
		MEMPRO_TRACK_ALLOC(p, size);
		return p;
	}

	void *user_new_array(std::size_t size) throw(std::bad_alloc)
	{
		return user_new(size);
	}

	void *user_new_array(std::size_t size, const std::nothrow_t& x) throw()
	{
		return user_new(size, x);
	}

	void user_delete(void *ptr) throw()
	{
		MEMPRO_TRACK_FREE(ptr);
		free(ptr);
	}

	void user_delete(void *ptr, const std::nothrow_t& x) throw()
	{
		(void)x;
		user_delete(ptr);
	}

	void user_delete_array(void *ptr) throw()
	{
		user_delete(ptr);
	}

	void user_delete_array(void *ptr, const std::nothrow_t& x) throw()
	{
		user_delete(ptr, x);
	}
#endif

//------------------------------------------------------------------------
#endif		// #ifdef MEMPRO_PLATFORM_PS4

//------------------------------------------------------------------------
#endif		// #if MEMPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef MEMPRO_MEMPROPS4_H_INCLUDED

