/* Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This file is provided under a dual BSD/GPLv2 license.
 *
 * SipHash: a fast short-input PRF
 * https://131002.net/siphash/
 *
 * This implementation is specifically for SipHash2-4 for a secure PRF
 * and HalfSipHash1-3/SipHash1-3 for an insecure PRF only suitable for
 * hashtables.
 */

#include <linux/siphash.h>
#include <asm/unaligned.h>

#ifndef __fallthrough
# if defined(__GNUC__) && __GNUC__ >= 7
#  define __fallthrough __attribute__ ((fallthrough))
# else
#  define __fallthrough
# endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
#ifdef __LITTLE_ENDIAN
#define bytemask_from_count(cnt)	(~(~0ul << (cnt)*8))
#else
#define bytemask_from_count(cnt)	(~(~0ul >> (cnt)*8))
#endif
#endif

#if defined(CONFIG_DCACHE_WORD_ACCESS) && BITS_PER_LONG == 64
#include <linux/dcache.h>
#include <asm/word-at-a-time.h>
#endif

#define SIPROUND \
	do { \
	v0 += v1; v1 = rol64(v1, 13); v1 ^= v0; v0 = rol64(v0, 32); \
	v2 += v3; v3 = rol64(v3, 16); v3 ^= v2; \
	v0 += v3; v3 = rol64(v3, 21); v3 ^= v0; \
	v2 += v1; v1 = rol64(v1, 17); v1 ^= v2; v2 = rol64(v2, 32); \
	} while (0)

#define PREAMBLE(len) \
	u64 v0 = 0x736f6d6570736575ULL; \
	u64 v1 = 0x646f72616e646f6dULL; \
	u64 v2 = 0x6c7967656e657261ULL; \
	u64 v3 = 0x7465646279746573ULL; \
	u64 b = ((u64)(len)) << 56; \
	v3 ^= key->key[1]; \
	v2 ^= key->key[0]; \
	v1 ^= key->key[1]; \
	v0 ^= key->key[0];

#define POSTAMBLE \
	v3 ^= b; \
	SIPROUND; \
	SIPROUND; \
	v0 ^= b; \
	v2 ^= 0xff; \
	SIPROUND; \
	SIPROUND; \
	SIPROUND; \
	SIPROUND; \
	return (v0 ^ v1) ^ (v2 ^ v3);

#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
u64 __siphash_aligned(const void *data, size_t len, const siphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u64));
	const u8 left = len & (sizeof(u64) - 1);
	u64 m;
	PREAMBLE(len)
	for (; data != end; data += sizeof(u64)) {
		m = le64_to_cpup(data);
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
	}
#if defined(CONFIG_DCACHE_WORD_ACCESS) && BITS_PER_LONG == 64
	if (left)
		b |= le64_to_cpu((__force __le64)(load_unaligned_zeropad(data) &
						  bytemask_from_count(left)));
#else
	switch (left) {
	case 7: b |= ((u64)end[6]) << 48; __fallthrough;
	case 6: b |= ((u64)end[5]) << 40; __fallthrough;
	case 5: b |= ((u64)end[4]) << 32; __fallthrough;
	case 4: b |= le32_to_cpup(data); break;
	case 3: b |= ((u64)end[2]) << 16; __fallthrough;
	case 2: b |= le16_to_cpup(data); break;
	case 1: b |= end[0];
	}
#endif
	POSTAMBLE
}
#endif

u64 __siphash_unaligned(const void *data, size_t len, const siphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u64));
	const u8 left = len & (sizeof(u64) - 1);
	u64 m;
	PREAMBLE(len)
	for (; data != end; data += sizeof(u64)) {
		m = get_unaligned_le64(data);
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
	}
#if defined(CONFIG_DCACHE_WORD_ACCESS) && BITS_PER_LONG == 64
	if (left)
		b |= le64_to_cpu((__force __le64)(load_unaligned_zeropad(data) &
						  bytemask_from_count(left)));
#else
	switch (left) {
	case 7: b |= ((u64)end[6]) << 48; __fallthrough;
	case 6: b |= ((u64)end[5]) << 40; __fallthrough;
	case 5: b |= ((u64)end[4]) << 32; __fallthrough;
	case 4: b |= get_unaligned_le32(end); break;
	case 3: b |= ((u64)end[2]) << 16; __fallthrough;
	case 2: b |= get_unaligned_le16(end); break;
	case 1: b |= end[0];
	}
#endif
	POSTAMBLE
}

/**
 * siphash_1u64 - compute 64-bit siphash PRF value of a u64
 * @first: first u64
 * @key: the siphash key
 */
u64 siphash_1u64(const u64 first, const siphash_key_t *key)
{
	PREAMBLE(8)
	v3 ^= first;
	SIPROUND;
	SIPROUND;
	v0 ^= first;
	POSTAMBLE
}

/**
 * siphash_2u64 - compute 64-bit siphash PRF value of 2 u64
 * @first: first u64
 * @second: second u64
 * @key: the siphash key
 */
u64 siphash_2u64(const u64 first, const u64 second, const siphash_key_t *key)
{
	PREAMBLE(16)
	v3 ^= first;
	SIPROUND;
	SIPROUND;
	v0 ^= first;
	v3 ^= second;
	SIPROUND;
	SIPROUND;
	v0 ^= second;
	POSTAMBLE
}

/**
 * siphash_3u64 - compute 64-bit siphash PRF value of 3 u64
 * @first: first u64
 * @second: second u64
 * @third: third u64
 * @key: the siphash key
 */
u64 siphash_3u64(const u64 first, const u64 second, const u64 third,
		 const siphash_key_t *key)
{
	PREAMBLE(24)
	v3 ^= first;
	SIPROUND;
	SIPROUND;
	v0 ^= first;
	v3 ^= second;
	SIPROUND;
	SIPROUND;
	v0 ^= second;
	v3 ^= third;
	SIPROUND;
	SIPROUND;
	v0 ^= third;
	POSTAMBLE
}

/**
 * siphash_4u64 - compute 64-bit siphash PRF value of 4 u64
 * @first: first u64
 * @second: second u64
 * @third: third u64
 * @forth: forth u64
 * @key: the siphash key
 */
u64 siphash_4u64(const u64 first, const u64 second, const u64 third,
		 const u64 forth, const siphash_key_t *key)
{
	PREAMBLE(32)
	v3 ^= first;
	SIPROUND;
	SIPROUND;
	v0 ^= first;
	v3 ^= second;
	SIPROUND;
	SIPROUND;
	v0 ^= second;
	v3 ^= third;
	SIPROUND;
	SIPROUND;
	v0 ^= third;
	v3 ^= forth;
	SIPROUND;
	SIPROUND;
	v0 ^= forth;
	POSTAMBLE
}

u64 siphash_1u32(const u32 first, const siphash_key_t *key)
{
	PREAMBLE(4)
	b |= first;
	POSTAMBLE
}

u64 siphash_3u32(const u32 first, const u32 second, const u32 third,
		 const siphash_key_t *key)
{
	u64 combined = (u64)second << 32 | first;
	PREAMBLE(12)
	v3 ^= combined;
	SIPROUND;
	SIPROUND;
	v0 ^= combined;
	b |= third;
	POSTAMBLE
}

#if BITS_PER_LONG == 64
/* Note that on 64-bit, we make HalfSipHash1-3 actually be SipHash1-3, for
 * performance reasons. On 32-bit, below, we actually implement HalfSipHash1-3.
 */

#define HSIPROUND SIPROUND
#define HPREAMBLE(len) PREAMBLE(len)
#define HPOSTAMBLE \
	v3 ^= b; \
	HSIPROUND; \
	v0 ^= b; \
	v2 ^= 0xff; \
	HSIPROUND; \
	HSIPROUND; \
	HSIPROUND; \
	return (v0 ^ v1) ^ (v2 ^ v3);

#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
u32 __hsiphash_aligned(const void *data, size_t len, const hsiphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u64));
	const u8 left = len & (sizeof(u64) - 1);
	u64 m;
	HPREAMBLE(len)
	for (; data != end; data += sizeof(u64)) {
		m = le64_to_cpup(data);
		v3 ^= m;
		HSIPROUND;
		v0 ^= m;
	}
#if defined(CONFIG_DCACHE_WORD_ACCESS) && BITS_PER_LONG == 64
	if (left)
		b |= le64_to_cpu((__force __le64)(load_unaligned_zeropad(data) &
						  bytemask_from_count(left)));
#else
	switch (left) {
	case 7: b |= ((u64)end[6]) << 48; __fallthrough;
	case 6: b |= ((u64)end[5]) << 40; __fallthrough;
	case 5: b |= ((u64)end[4]) << 32; __fallthrough;
	case 4: b |= le32_to_cpup(data); break;
	case 3: b |= ((u64)end[2]) << 16; __fallthrough;
	case 2: b |= le16_to_cpup(data); break;
	case 1: b |= end[0];
	}
#endif
	HPOSTAMBLE
}
#endif

u32 __hsiphash_unaligned(const void *data, size_t len,
			 const hsiphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u64));
	const u8 left = len & (sizeof(u64) - 1);
	u64 m;
	HPREAMBLE(len)
	for (; data != end; data += sizeof(u64)) {
		m = get_unaligned_le64(data);
		v3 ^= m;
		HSIPROUND;
		v0 ^= m;
	}
#if defined(CONFIG_DCACHE_WORD_ACCESS) && BITS_PER_LONG == 64
	if (left)
		b |= le64_to_cpu((__force __le64)(load_unaligned_zeropad(data) &
						  bytemask_from_count(left)));
#else
	switch (left) {
	case 7: b |= ((u64)end[6]) << 48; __fallthrough;
	case 6: b |= ((u64)end[5]) << 40; __fallthrough;
	case 5: b |= ((u64)end[4]) << 32; __fallthrough;
	case 4: b |= get_unaligned_le32(end); break;
	case 3: b |= ((u64)end[2]) << 16; __fallthrough;
	case 2: b |= get_unaligned_le16(end); break;
	case 1: b |= end[0];
	}
#endif
	HPOSTAMBLE
}

/**
 * hsiphash_1u32 - compute 64-bit hsiphash PRF value of a u32
 * @first: first u32
 * @key: the hsiphash key
 */
u32 hsiphash_1u32(const u32 first, const hsiphash_key_t *key)
{
	HPREAMBLE(4)
	b |= first;
	HPOSTAMBLE
}

/**
 * hsiphash_2u32 - compute 32-bit hsiphash PRF value of 2 u32
 * @first: first u32
 * @second: second u32
 * @key: the hsiphash key
 */
u32 hsiphash_2u32(const u32 first, const u32 second, const hsiphash_key_t *key)
{
	u64 combined = (u64)second << 32 | first;
	HPREAMBLE(8)
	v3 ^= combined;
	HSIPROUND;
	v0 ^= combined;
	HPOSTAMBLE
}

/**
 * hsiphash_3u32 - compute 32-bit hsiphash PRF value of 3 u32
 * @first: first u32
 * @second: second u32
 * @third: third u32
 * @key: the hsiphash key
 */
u32 hsiphash_3u32(const u32 first, const u32 second, const u32 third,
		  const hsiphash_key_t *key)
{
	u64 combined = (u64)second << 32 | first;
	HPREAMBLE(12)
	v3 ^= combined;
	HSIPROUND;
	v0 ^= combined;
	b |= third;
	HPOSTAMBLE
}

/**
 * hsiphash_4u32 - compute 32-bit hsiphash PRF value of 4 u32
 * @first: first u32
 * @second: second u32
 * @third: third u32
 * @forth: forth u32
 * @key: the hsiphash key
 */
u32 hsiphash_4u32(const u32 first, const u32 second, const u32 third,
		  const u32 forth, const hsiphash_key_t *key)
{
	u64 combined = (u64)second << 32 | first;
	HPREAMBLE(16)
	v3 ^= combined;
	HSIPROUND;
	v0 ^= combined;
	combined = (u64)forth << 32 | third;
	v3 ^= combined;
	HSIPROUND;
	v0 ^= combined;
	HPOSTAMBLE
}
#else
#define HSIPROUND \
	do { \
	v0 += v1; v1 = rol32(v1, 5); v1 ^= v0; v0 = rol32(v0, 16); \
	v2 += v3; v3 = rol32(v3, 8); v3 ^= v2; \
	v0 += v3; v3 = rol32(v3, 7); v3 ^= v0; \
	v2 += v1; v1 = rol32(v1, 13); v1 ^= v2; v2 = rol32(v2, 16); \
	} while (0)

#define HPREAMBLE(len) \
	u32 v0 = 0; \
	u32 v1 = 0; \
	u32 v2 = 0x6c796765U; \
	u32 v3 = 0x74656462U; \
	u32 b = ((u32)(len)) << 24; \
	v3 ^= key->key[1]; \
	v2 ^= key->key[0]; \
	v1 ^= key->key[1]; \
	v0 ^= key->key[0];

#define HPOSTAMBLE \
	v3 ^= b; \
	HSIPROUND; \
	v0 ^= b; \
	v2 ^= 0xff; \
	HSIPROUND; \
	HSIPROUND; \
	HSIPROUND; \
	return v1 ^ v3;

#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
u32 __hsiphash_aligned(const void *data, size_t len, const hsiphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u32));
	const u8 left = len & (sizeof(u32) - 1);
	u32 m;
	HPREAMBLE(len)
	for (; data != end; data += sizeof(u32)) {
		m = le32_to_cpup(data);
		v3 ^= m;
		HSIPROUND;
		v0 ^= m;
	}
	switch (left) {
	case 3: b |= ((u32)end[2]) << 16; __fallthrough;
	case 2: b |= le16_to_cpup(data); break;
	case 1: b |= end[0];
	}
	HPOSTAMBLE
}
#endif

u32 __hsiphash_unaligned(const void *data, size_t len,
			 const hsiphash_key_t *key)
{
	const u8 *end = data + len - (len % sizeof(u32));
	const u8 left = len & (sizeof(u32) - 1);
	u32 m;
	HPREAMBLE(len)
	for (; data != end; data += sizeof(u32)) {
		m = get_unaligned_le32(data);
		v3 ^= m;
		HSIPROUND;
		v0 ^= m;
	}
	switch (left) {
	case 3: b |= ((u32)end[2]) << 16; __fallthrough;
	case 2: b |= get_unaligned_le16(end); break;
	case 1: b |= end[0];
	}
	HPOSTAMBLE
}

/**
 * hsiphash_1u32 - compute 32-bit hsiphash PRF value of a u32
 * @first: first u32
 * @key: the hsiphash key
 */
u32 hsiphash_1u32(const u32 first, const hsiphash_key_t *key)
{
	HPREAMBLE(4)
	v3 ^= first;
	HSIPROUND;
	v0 ^= first;
	HPOSTAMBLE
}

/**
 * hsiphash_2u32 - compute 32-bit hsiphash PRF value of 2 u32
 * @first: first u32
 * @second: second u32
 * @key: the hsiphash key
 */
u32 hsiphash_2u32(const u32 first, const u32 second, const hsiphash_key_t *key)
{
	HPREAMBLE(8)
	v3 ^= first;
	HSIPROUND;
	v0 ^= first;
	v3 ^= second;
	HSIPROUND;
	v0 ^= second;
	HPOSTAMBLE
}

/**
 * hsiphash_3u32 - compute 32-bit hsiphash PRF value of 3 u32
 * @first: first u32
 * @second: second u32
 * @third: third u32
 * @key: the hsiphash key
 */
u32 hsiphash_3u32(const u32 first, const u32 second, const u32 third,
		  const hsiphash_key_t *key)
{
	HPREAMBLE(12)
	v3 ^= first;
	HSIPROUND;
	v0 ^= first;
	v3 ^= second;
	HSIPROUND;
	v0 ^= second;
	v3 ^= third;
	HSIPROUND;
	v0 ^= third;
	HPOSTAMBLE
}

/**
 * hsiphash_4u32 - compute 32-bit hsiphash PRF value of 4 u32
 * @first: first u32
 * @second: second u32
 * @third: third u32
 * @forth: forth u32
 * @key: the hsiphash key
 */
u32 hsiphash_4u32(const u32 first, const u32 second, const u32 third,
		  const u32 forth, const hsiphash_key_t *key)
{
	HPREAMBLE(16)
	v3 ^= first;
	HSIPROUND;
	v0 ^= first;
	v3 ^= second;
	HSIPROUND;
	v0 ^= second;
	v3 ^= third;
	HSIPROUND;
	v0 ^= third;
	v3 ^= forth;
	HSIPROUND;
	v0 ^= forth;
	HPOSTAMBLE
}
#endif
